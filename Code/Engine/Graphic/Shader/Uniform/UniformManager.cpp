// Windows header preprocessing
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "UniformManager.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

#include <cstring>

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Resource/BindlessRootSignature.hpp"
#include "Engine/Graphic/Resource/GlobalDescriptorHeapManager.hpp"

namespace enigma::graphic
{
    UniformManager::UniformManager()
    {
        // Step 1: Get GlobalDescriptorHeapManager
        auto* heapMgr = D3D12RenderSystem::GetGlobalDescriptorHeapManager();
        if (!heapMgr)
        {
            LogError(LogUniform, "GlobalDescriptorHeapManager not available");
            ERROR_AND_DIE("UniformManager: GlobalDescriptorHeapManager not available");
        }

        // Step 2: Get Descriptor increment size
        auto* device = D3D12RenderSystem::GetDevice();
        if (device)
        {
            m_cbvSrvUavDescriptorIncrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        // Step 3: Pre-allocate Ring Descriptor Pool (MAX_RING_FRAMES * MAX_CUSTOM_BUFFERS)
        const uint32_t totalDescriptors = BindlessRootSignature::MAX_RING_FRAMES * BindlessRootSignature::MAX_CUSTOM_BUFFERS;
        auto           allocations      = heapMgr->BatchAllocateCustomCbv(totalDescriptors);
        if (allocations.size() != totalDescriptors)
        {
            LogError(LogUniform, "Failed to allocate Ring Descriptor Pool: expected %u, got %zu",
                     totalDescriptors, allocations.size());
            ERROR_AND_DIE(Stringf("UniformManager: expected %u descriptors, got %zu",
                totalDescriptors, allocations.size()));
        }

        // Step 4: Verify descriptor continuity (required by Descriptor Table)
        for (size_t i = 1; i < allocations.size(); i++)
        {
            if (allocations[i].heapIndex != allocations[i - 1].heapIndex + 1)
            {
                LogError(LogUniform, "Ring Descriptor Pool not contiguous at index %zu", i);
                ERROR_AND_DIE(Stringf("UniformManager: descriptors not contiguous at index %zu", i));
            }
        }

        // Step 5: Store base GPU handle for SetGraphicsRootDescriptorTable
        m_customBufferDescriptorTableBaseGPUHandle = allocations[0].gpuHandle;

        LogInfo(LogUniform, "UniformManager: Ring Descriptor Pool Base GPU Handle=0x%llX, incrementSize=%u",
                m_customBufferDescriptorTableBaseGPUHandle.ptr, m_cbvSrvUavDescriptorIncrementSize);

        // Step 6: Store all allocations
        m_customBufferDescriptorPool.resize(totalDescriptors);
        for (size_t i = 0; i < allocations.size(); i++)
        {
            m_customBufferDescriptorPool[i] = allocations[i];
        }

        m_initialized = true;

        LogInfo(LogUniform, "UniformManager: %u Ring Descriptors allocated (MAX_RING_FRAMES=%u * MAX_CUSTOM_BUFFERS=%u)",
                totalDescriptors, BindlessRootSignature::MAX_RING_FRAMES, BindlessRootSignature::MAX_CUSTOM_BUFFERS);
    }

    UniformManager::~UniformManager()
    {
    }

    void UniformManager::IncrementDrawCount()
    {
        m_currentDrawCount++;
        for (auto& [slotId, state] : m_customBufferStates)
        {
            state->currentDrawIndex++;
        }

        for (auto& [typeId, state] : m_perObjectBuffers)
        {
            state.currentIndex++;
        }
    }

    void UniformManager::ResetDrawCount()
    {
        m_currentDrawCount = 0;

        // [FIX] Reset Engine Buffer currentIndex to sync with m_currentDrawCount
        // Bug: Previously only Custom Buffers were reset, causing Engine Buffers
        // to write to index N while GetEngineBufferGPUAddress reads from index 0
        for (auto& [typeId, state] : m_perObjectBuffers)
        {
            state.currentIndex = 0;
        }


        // Reset Custom Buffer draw indices
        for (auto& [slotId, state] : m_customBufferStates)
        {
            state->currentDrawIndex = 0;
        }
    }

    bool UniformManager::IsSlotRegistered(uint32_t slot, uint32_t space) const
    {
        return m_slotToTypeMap.find(SlotSpaceKey{slot, space}) != m_slotToTypeMap.end();
    }

    const std::vector<uint32_t>& UniformManager::GetSlotsByFrequency(UpdateFrequency frequency) const
    {
        static const std::vector<uint32_t> emptyVector;

        auto it = m_frequencyToSlotsMap.find(frequency);
        if (it != m_frequencyToSlotsMap.end())
        {
            return it->second;
        }

        return emptyVector;
    }

    PerObjectBufferState* UniformManager::GetBufferStateBySlot(uint32_t rootSlot, uint32_t space) const
    {
        // Find (slot, space) combination
        auto slotIt = m_slotToTypeMap.find(SlotSpaceKey{rootSlot, space});
        if (slotIt == m_slotToTypeMap.end())
        {
            return nullptr;
        }

        // Get TypeId and find BufferState
        const std::type_index& typeId   = slotIt->second;
        auto                   bufferIt = m_perObjectBuffers.find(typeId);
        if (bufferIt != m_perObjectBuffers.end())
        {
            return const_cast<PerObjectBufferState*>(&bufferIt->second);
        }

        // TypeId registered but buffer not created (abnormal)
        LogWarn("UniformManager", "Slot %u (space=%u) registered but buffer not found: %s", rootSlot, space, typeId.name());
        ERROR_RECOVERABLE(Stringf("Slot %u (space=%u) registered but buffer not found", rootSlot, space));
        return nullptr;
    }

    void UniformManager::UpdateRingBufferOffsets(UpdateFrequency frequency)
    {
        const auto& slots = GetSlotsByFrequency(frequency);
        if (slots.empty())
        {
            return;
        }

        for (uint32_t slotId : slots)
        {
            // Check if Custom Buffer (skip - handled separately)
            auto customIt = m_customBufferStates.find(slotId);
            if (customIt != m_customBufferStates.end())
            {
                continue;
            }

            // Engine Buffer path
            auto* state = GetBufferStateBySlot(slotId);
            if (!state || !state->gpuBuffer)
            {
                continue;
            }

            // Calculate current Ring Buffer index
            size_t currentIndex = (frequency == UpdateFrequency::PerObject)
                                      ? (m_currentDrawCount % state->maxCount)
                                      : 0;

            // Delayed Fill: copy last value if index not updated
            if (state->lastUpdatedIndex != currentIndex)
            {
                void* destPtr = state->GetDataAt(currentIndex);
                std::memcpy(destPtr, state->lastUpdatedValue.data(), state->elementSize);
            }

            UpdateRootCBVOffset(slotId, currentIndex);
        }
    }

    // ========================================================================
    // RegisterEngineBuffer - Engine slot 0-14, Root CBV binding
    // ========================================================================
    bool UniformManager::RegisterEngineBuffer(
        uint32_t        slotId,
        std::type_index typeId,
        size_t          bufferSize,
        UpdateFrequency freq,
        size_t          maxDraws,
        uint32_t        space)
    {
        // Validate space must be 0 for Engine buffers
        if (space != 0)
        {
            throw UniformBufferException("Engine Buffer must use space=0", slotId, space);
        }

        // Validate slot range (0-14)
        if (!BufferHelper::IsEngineReservedSlot(slotId))
        {
            throw UniformBufferException("Engine Buffer slot must be 0-14", slotId, space);
        }

        // Check slot conflict
        if (IsSlotRegistered(slotId))
        {
            throw UniformBufferException("Slot already registered", slotId, space);
        }

        // Calculate 256-byte aligned size
        size_t alignedSize = BufferHelper::CalculateAlignedSize(bufferSize);

        // Calculate Ring Buffer count
        size_t ringBufferCount = 1;
        switch (freq)
        {
        case UpdateFrequency::PerObject: ringBufferCount = maxDraws;
            break;
        case UpdateFrequency::PerPass: ringBufferCount = 20;
            break;
        case UpdateFrequency::PerFrame:
        case UpdateFrequency::Static: ringBufferCount = 1;
            break;
        }

        size_t totalSize = alignedSize * ringBufferCount;

        // Create GPU Buffer
        BufferCreateInfo createInfo;
        createInfo.size          = totalSize;
        createInfo.usage         = BufferUsage::ConstantBuffer;
        createInfo.memoryAccess  = MemoryAccess::CPUToGPU;
        createInfo.initialData   = nullptr;
        std::string debugNameStr = Stringf("EngineBuffer_Slot%u", slotId);
        createInfo.debugName     = debugNameStr.c_str();

        auto gpuBuffer = D3D12RenderSystem::CreateBuffer(createInfo);
        if (!gpuBuffer)
        {
            throw UniformBufferException("Failed to create GPU buffer", slotId, space);
        }

        // Persistent mapping
        void* mappedData = gpuBuffer->MapPersistent();
        if (!mappedData)
        {
            throw UniformBufferException("Failed to map GPU buffer", slotId, space);
        }

        // Create PerObjectBufferState
        PerObjectBufferState state;
        state.gpuBuffer    = std::move(gpuBuffer);
        state.mappedData   = mappedData;
        state.elementSize  = alignedSize;
        state.maxCount     = ringBufferCount;
        state.frequency    = freq;
        state.currentIndex = 0;
        state.lastUpdatedValue.resize(alignedSize, 0);
        state.lastUpdatedIndex = SIZE_MAX;

        m_perObjectBuffers[typeId] = std::move(state);
        m_frequencyToSlotsMap[freq].push_back(slotId);

        LogInfo(LogUniform, "Engine Buffer registered: Slot=%u, Size=%zu, Freq=%d, Count=%zu",
                slotId, alignedSize, static_cast<int>(freq), ringBufferCount);
        return true;
    }

    // ========================================================================
    // RegisterCustomBuffer - Custom slot 0-99, Descriptor Table binding (space1)
    // ========================================================================
    bool UniformManager::RegisterCustomBuffer(
        uint32_t slotId,
        std::type_index /*typeId*/,
        size_t          bufferSize,
        UpdateFrequency freq,
        size_t          maxDraws,
        uint32_t        space)
    {
        // Validate space must be 1 for Custom buffers
        if (space != 1)
        {
            throw UniformBufferException("Custom Buffer must use space=1", slotId, space);
        }

        // Validate slot range (0-99)
        if (slotId >= BindlessRootSignature::MAX_CUSTOM_BUFFERS)
        {
            throw UniformBufferException("Custom Buffer slot exceeds MAX_CUSTOM_BUFFERS", slotId, space);
        }

        // Warn if using engine reserved slot range (0-14)
        if (BufferHelper::IsEngineReservedSlot(slotId))
        {
            LogWarn(LogUniform, "Custom Buffer slot %u is in engine range (0-14), ensure shader uses register(b%u, space1)", slotId, slotId);
        }

        // Check slot conflict
        if (IsSlotRegistered(slotId))
        {
            throw UniformBufferException("Slot already registered", slotId, space);
        }

        // Calculate 256-byte aligned size
        size_t alignedSize = BufferHelper::CalculateAlignedSize(bufferSize);

        // Calculate Ring Buffer count
        size_t ringBufferCount = 1;
        switch (freq)
        {
        case UpdateFrequency::PerObject: ringBufferCount = maxDraws;
            break;
        case UpdateFrequency::PerPass: ringBufferCount = 20;
            break;
        case UpdateFrequency::PerFrame:
        case UpdateFrequency::Static: ringBufferCount = 1;
            break;
        }

        size_t totalSize = alignedSize * ringBufferCount;

        // Create GPU Buffer
        BufferCreateInfo createInfo;
        createInfo.size          = totalSize;
        createInfo.usage         = BufferUsage::ConstantBuffer;
        createInfo.memoryAccess  = MemoryAccess::CPUToGPU;
        createInfo.initialData   = nullptr;
        std::string debugNameStr = Stringf("CustomBuffer_Slot%u", slotId);
        createInfo.debugName     = debugNameStr.c_str();

        auto gpuBuffer = D3D12RenderSystem::CreateBuffer(createInfo);
        if (!gpuBuffer)
        {
            throw UniformBufferException("Failed to create GPU buffer", slotId, space);
        }

        // Persistent mapping
        void* mappedData = gpuBuffer->MapPersistent();
        if (!mappedData)
        {
            throw UniformBufferException("Failed to map GPU buffer", slotId, space);
        }

        // Allocate Descriptor
        if (!AllocateCustomBufferDescriptor(slotId))
        {
            throw DescriptorHeapException("Failed to allocate descriptor for Custom Buffer");
        }

        // Create CBV for ALL Ring Frames
        auto* device = D3D12RenderSystem::GetDevice();
        if (!device)
        {
            throw DescriptorHeapException("D3D12 device not available for CBV creation");
        }

        D3D12_GPU_VIRTUAL_ADDRESS baseGpuAddress     = gpuBuffer->GetGPUVirtualAddress();
        size_t                    effectiveRingCount = std::min(ringBufferCount, static_cast<size_t>(BindlessRootSignature::MAX_RING_FRAMES));

        for (uint32_t ringFrame = 0; ringFrame < BindlessRootSignature::MAX_RING_FRAMES; ++ringFrame)
        {
            uint32_t descriptorIndex = ringFrame * BindlessRootSignature::MAX_CUSTOM_BUFFERS + slotId;
            if (descriptorIndex >= m_customBufferDescriptorPool.size())
            {
                LogError(LogUniform, "Descriptor index %u out of range (pool size=%zu)", descriptorIndex, m_customBufferDescriptorPool.size());
                continue;
            }

            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle    = m_customBufferDescriptorPool[descriptorIndex].cpuHandle;
            size_t                      sliceIndex   = ringFrame % effectiveRingCount;
            D3D12_GPU_VIRTUAL_ADDRESS   sliceAddress = baseGpuAddress + sliceIndex * alignedSize;

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation                  = sliceAddress;
            cbvDesc.SizeInBytes                     = static_cast<UINT>(alignedSize);

            device->CreateConstantBufferView(&cbvDesc, cpuHandle);
        }

        LogInfo(LogUniform, "Created %u CBVs for Custom Buffer Slot=%u, Base GPU=0x%llX",
                BindlessRootSignature::MAX_RING_FRAMES, slotId, baseGpuAddress);

        // Get first frame CPU handle for compatibility
        auto                        it             = m_customBufferDescriptors.find(slotId);
        D3D12_CPU_DESCRIPTOR_HANDLE firstCpuHandle = {};
        if (it != m_customBufferDescriptors.end() && it->second.isValid)
        {
            firstCpuHandle = it->second.allocation.cpuHandle;
        }

        // Create CustomBufferState
        auto customState              = std::make_unique<CustomBufferState>();
        customState->elementSize      = alignedSize;
        customState->maxDraws         = ringBufferCount;
        customState->currentDrawIndex = 0;
        customState->gpuBuffer        = gpuBuffer->GetResource();
        customState->mappedPtr        = static_cast<uint8_t*>(mappedData);
        customState->bindlessIndex    = it->second.allocation.heapIndex;
        customState->cpuHandle        = firstCpuHandle;
        customState->slot             = slotId;
        customState->frequency        = freq;
        customState->isDirty          = false;

        m_customBufferStates[slotId] = std::move(customState);
        m_frequencyToSlotsMap[freq].push_back(slotId);

        LogInfo(LogUniform, "Custom Buffer registered: Slot=%u, Space=1, Size=%zu, MaxDraws=%zu",
                slotId, alignedSize, ringBufferCount);
        return true;
    }

    // ========================================================================
    // UploadEngineBuffer
    // ========================================================================
    bool UniformManager::UploadEngineBuffer(uint32_t slotId, const void* data, size_t size)
    {
        auto* state = GetBufferStateBySlot(slotId);
        if (!state)
        {
            LogError(LogUniform, "Engine Buffer slot %u not registered", slotId);
            return false;
        }

        if (size > state->elementSize)
        {
            LogWarn(LogUniform, "Data size (%zu) exceeds element size (%zu)", size, state->elementSize);
        }

        size_t currentIndex = state->GetCurrentIndex();
        void*  destPtr      = state->GetDataAt(currentIndex);
        std::memcpy(destPtr, data, size);

        // Update Delayed Fill cache
        std::memcpy(state->lastUpdatedValue.data(), data, size);
        state->lastUpdatedIndex = currentIndex;
        state->currentIndex++;

        return true;
    }

    // ========================================================================
    // UploadCustomBuffer
    // ========================================================================
    bool UniformManager::UploadCustomBuffer(uint32_t slotId, const void* data, size_t size)
    {
        auto it = m_customBufferStates.find(slotId);
        if (it == m_customBufferStates.end())
        {
            LogError(LogUniform, "Custom Buffer slot %u not registered", slotId);
            return false;
        }

        CustomBufferState* state = it->second.get();

        if (size > state->elementSize)
        {
            LogWarn(LogUniform, "Data size (%zu) exceeds element size (%zu)", size, state->elementSize);
        }

        uint32_t ringIndex = state->currentDrawIndex % static_cast<uint32_t>(state->maxDraws);
        size_t   offset    = ringIndex * state->elementSize;
        std::memcpy(state->mappedPtr + offset, data, size);

        state->isDirty = true;
        return true;
    }

    // ========================================================================
    // UpdateRootCBVOffset - Placeholder (actual binding in RendererSubsystem)
    // ========================================================================
    void UniformManager::UpdateRootCBVOffset(uint32_t slotId, size_t currentIndex)
    {
        UNUSED(slotId)
        UNUSED(currentIndex)
    }

    // ========================================================================
    // UpdateDescriptorTableOffset
    // ========================================================================
    void UniformManager::UpdateDescriptorTableOffset(uint32_t slotId, size_t currentIndex)
    {
        auto* state = GetBufferStateBySlot(slotId);
        if (!state || !state->gpuBuffer)
        {
            return;
        }

        auto it = m_customBufferDescriptors.find(slotId);
        if (it == m_customBufferDescriptors.end() || !it->second.isValid)
        {
            LogError(LogUniform, "Custom Buffer descriptor not found for slot %u", slotId);
            return;
        }

        D3D12_GPU_VIRTUAL_ADDRESS bufferAddress = state->gpuBuffer->GetGPUVirtualAddress();
        bufferAddress                           += currentIndex * state->elementSize;

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation                  = bufferAddress;
        cbvDesc.SizeInBytes                     = static_cast<UINT>(state->elementSize);

        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = it->second.allocation.cpuHandle;

        auto* device = D3D12RenderSystem::GetDevice();
        if (!device)
        {
            LogError(LogUniform, "D3D12 device not available");
            return;
        }

        device->CreateConstantBufferView(&cbvDesc, cpuHandle);
    }

    // ========================================================================
    // AllocateCustomBufferDescriptor
    // ========================================================================
    bool UniformManager::AllocateCustomBufferDescriptor(uint32_t slotId)
    {
        if (m_customBufferDescriptors.find(slotId) != m_customBufferDescriptors.end())
        {
            LogWarn(LogUniform, "Descriptor already allocated for slot %u", slotId);
            return true;
        }

        if (!m_initialized || m_customBufferDescriptorPool.empty())
        {
            LogError(LogUniform, "UniformManager not initialized or pool empty");
            return false;
        }

        uint32_t descriptorIndex = slotId;
        if (descriptorIndex >= BindlessRootSignature::MAX_CUSTOM_BUFFERS)
        {
            LogError(LogUniform, "Slot %u exceeds MAX_CUSTOM_BUFFERS (%u)", slotId, BindlessRootSignature::MAX_CUSTOM_BUFFERS);
            return false;
        }

        auto allocation = m_customBufferDescriptorPool[descriptorIndex];
        if (!allocation.isValid)
        {
            LogError(LogUniform, "Failed to get descriptor for slot %u (index %u)", slotId, descriptorIndex);
            return false;
        }

        CustomBufferDescriptor desc;
        desc.allocation = allocation;
        desc.slotId     = slotId;
        desc.isValid    = true;

        m_customBufferDescriptors[slotId] = desc;

        LogInfo(LogUniform, "Allocated descriptor: Slot=%u -> Index=%u", slotId, descriptorIndex);
        return true;
    }

    // ========================================================================
    // FreeCustomBufferDescriptor
    // ========================================================================
    void UniformManager::FreeCustomBufferDescriptor(uint32_t slotId)
    {
        auto it = m_customBufferDescriptors.find(slotId);
        if (it != m_customBufferDescriptors.end())
        {
            m_customBufferDescriptors.erase(it);
            LogInfo(LogUniform, "Freed descriptor for slot %u", slotId);
        }
    }

    // ========================================================================
    // GetCustomBufferDescriptorTableGPUHandle
    // ========================================================================
    D3D12_GPU_DESCRIPTOR_HANDLE UniformManager::GetCustomBufferDescriptorTableGPUHandle(uint32_t ringIndex) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle             = m_customBufferDescriptorTableBaseGPUHandle;
        uint32_t                    effectiveRingIndex = ringIndex % BindlessRootSignature::MAX_RING_FRAMES;
        handle.ptr                                     += effectiveRingIndex * BindlessRootSignature::MAX_CUSTOM_BUFFERS * m_cbvSrvUavDescriptorIncrementSize;
        return handle;
    }

    // ========================================================================
    // GetCustomBufferCPUHandle
    // ========================================================================
    D3D12_CPU_DESCRIPTOR_HANDLE UniformManager::GetCustomBufferCPUHandle(uint32_t slotId) const
    {
        auto it = m_customBufferDescriptors.find(slotId);
        if (it != m_customBufferDescriptors.end())
        {
            return it->second.allocation.cpuHandle;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE nullHandle = {};
        nullHandle.ptr                         = 0;
        return nullHandle;
    }

    // ========================================================================
    // GetEngineBufferGPUAddress
    // ========================================================================
    D3D12_GPU_VIRTUAL_ADDRESS UniformManager::GetEngineBufferGPUAddress(uint32_t slotId) const
    {
        if (!BufferHelper::IsEngineReservedSlot(slotId))
        {
            LogError(LogUniform, "Slot %u is not an engine reserved slot", slotId);
            return 0;
        }

        // Skip if slot is registered as Custom Buffer
        if (m_customBufferStates.find(slotId) != m_customBufferStates.end())
        {
            return 0;
        }

        auto* state = GetBufferStateBySlot(slotId);
        if (!state || !state->gpuBuffer)
        {
            return 0;
        }

        size_t currentIndex = 0;
        if (state->frequency == UpdateFrequency::PerObject)
        {
            currentIndex = m_currentDrawCount % state->maxCount;
        }

        D3D12_GPU_VIRTUAL_ADDRESS baseAddress    = state->gpuBuffer->GetGPUVirtualAddress();
        D3D12_GPU_VIRTUAL_ADDRESS currentAddress = baseAddress + currentIndex * state->elementSize;

        return currentAddress;
    }
} // namespace enigma::graphic
