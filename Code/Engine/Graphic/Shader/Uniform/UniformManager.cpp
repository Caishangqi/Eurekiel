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
        // [REFACTORED] Unified: use m_bufferStates with ringIndex
        for (auto& [typeId, state] : m_bufferStates)
        {
            state.ringIndex++;
        }
    }

    void UniformManager::ResetDrawCount()
    {
        m_currentDrawCount = 0;
        // [REFACTORED] Unified: reset all buffers' ringIndex
        for (auto& [typeId, state] : m_bufferStates)
        {
            state.ringIndex = 0;
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

        // [REFACTORED] Get TypeId and find in unified m_bufferStates
        const std::type_index& typeId   = slotIt->second;
        auto                   bufferIt = m_bufferStates.find(typeId);
        if (bufferIt != m_bufferStates.end())
        {
            return const_cast<PerObjectBufferState*>(&bufferIt->second);
        }

        // TypeId registered but buffer not created (abnormal)
        LogWarn(LogUniform, "Slot %u (space=%u) registered but buffer not found: %s", rootSlot, space, typeId.name());
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
            // [REFACTORED] Use unified GetBufferStateBySlot for both Engine and Custom
            auto* state = GetBufferStateBySlot(slotId);
            if (!state || !state->buffer)
            {
                continue;
            }

            // Skip Custom Buffers (handled separately via Descriptor Table)
            if (state->space == BufferSpace::Custom)
            {
                continue;
            }

            // Calculate current Ring Buffer index
            size_t currentIndex = state->GetCurrentRingIndex();

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
    // [REFACTORED] RegisterBufferInternal - Unified buffer registration
    // Routes to Engine (Root CBV) or Custom (Descriptor Table) based on space
    // ========================================================================
    bool UniformManager::RegisterBufferInternal(
        uint32_t        slotId,
        std::type_index typeId,
        size_t          bufferSize,
        UpdateFrequency freq,
        size_t          maxDraws,
        BufferSpace     space)
    {
        uint32_t spaceValue = static_cast<uint32_t>(space);

        // Validate space-specific constraints
        if (space == BufferSpace::Engine)
        {
            if (!BufferHelper::IsEngineReservedSlot(slotId))
            {
                throw UniformBufferException("Engine Buffer slot must be 0-14", slotId, spaceValue);
            }
        }
        else if (space == BufferSpace::Custom)
        {
            if (slotId >= BindlessRootSignature::MAX_CUSTOM_BUFFERS)
            {
                throw UniformBufferException("Custom Buffer slot exceeds MAX_CUSTOM_BUFFERS", slotId, spaceValue);
            }
            if (BufferHelper::IsEngineReservedSlot(slotId))
            {
                LogWarn(LogUniform, "Custom Buffer slot %u is in engine range (0-14), ensure shader uses register(b%u, space1)", slotId, slotId);
            }
        }

        // Check slot conflict
        if (IsSlotRegistered(slotId, spaceValue))
        {
            throw UniformBufferException("Slot already registered", slotId, spaceValue);
        }

        // Calculate 256-byte aligned size
        size_t alignedSize = BufferHelper::CalculateAlignedSize(bufferSize);

        // Calculate Ring Buffer count based on frequency
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

        // Create GPU Buffer using D12Buffer
        BufferCreateInfo createInfo;
        createInfo.size          = totalSize;
        createInfo.usage         = BufferUsage::ConstantBuffer;
        createInfo.memoryAccess  = MemoryAccess::CPUToGPU;
        createInfo.initialData   = nullptr;
        std::string debugNameStr = (space == BufferSpace::Engine)
                                       ? Stringf("EngineBuffer_Slot%u", slotId)
                                       : Stringf("CustomBuffer_Slot%u_Space1", slotId);
        createInfo.debugName = debugNameStr.c_str();

        auto gpuBuffer = D3D12RenderSystem::CreateBuffer(createInfo);
        if (!gpuBuffer)
        {
            throw UniformBufferException("Failed to create GPU buffer", slotId, spaceValue);
        }

        // Persistent mapping
        void* mappedData = gpuBuffer->MapPersistent();
        if (!mappedData)
        {
            throw UniformBufferException("Failed to map GPU buffer", slotId, spaceValue);
        }

        // Custom Buffer needs descriptor allocation and CBV creation
        if (space == BufferSpace::Custom)
        {
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
        }

        // [REFACTORED] Create unified UniformBufferState
        UniformBufferState state;
        state.buffer      = std::move(gpuBuffer);
        state.elementSize = alignedSize;
        state.maxCount    = ringBufferCount;
        state.ringIndex   = 0;
        state.frequency   = freq;
        state.slot        = slotId;
        state.space       = space;
        state.lastUpdatedValue.resize(alignedSize, 0);
        state.lastUpdatedIndex = SIZE_MAX;

        m_bufferStates[typeId] = std::move(state);
        m_frequencyToSlotsMap[freq].push_back(slotId);

        const char* spaceStr = (space == BufferSpace::Engine) ? "Engine" : "Custom";
        LogInfo(LogUniform, "%s Buffer registered: Slot=%u, Space=%u, Size=%zu, Freq=%d, Count=%zu",
                spaceStr, slotId, spaceValue, alignedSize, static_cast<int>(freq), ringBufferCount);
        return true;
    }

    // [REMOVED] RegisterCustomBuffer - merged into RegisterBufferInternal

    // ========================================================================
    // [REFACTORED] UploadBufferInternal - Unified buffer upload
    // Routes based on space stored in UniformBufferState
    // ========================================================================
    bool UniformManager::UploadBufferInternal(const std::type_index& typeId, const void* data, size_t size)
    {
        // Find buffer state by TypeId
        auto it = m_bufferStates.find(typeId);
        if (it == m_bufferStates.end())
        {
            LogError(LogUniform, "Buffer not registered: %s", typeId.name());
            return false;
        }

        UniformBufferState& state = it->second;

        if (size > state.elementSize)
        {
            LogWarn(LogUniform, "Data size (%zu) exceeds element size (%zu)", size, state.elementSize);
        }

        // Calculate current ring index
        size_t currentIndex = state.GetCurrentRingIndex();
        void*  destPtr      = state.GetDataAt(currentIndex);

        if (!destPtr)
        {
            LogError(LogUniform, "Failed to get mapped data for buffer slot %u", state.slot);
            return false;
        }

        std::memcpy(destPtr, data, size);

        // Update Delayed Fill cache
        std::memcpy(state.lastUpdatedValue.data(), data, size);
        state.lastUpdatedIndex = currentIndex;

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
        // [REFACTORED] Use unified m_bufferStates
        auto* state = GetBufferStateBySlot(slotId, 1); // space=1 for Custom
        if (!state || !state->buffer)
        {
            return;
        }

        auto it = m_customBufferDescriptors.find(slotId);
        if (it == m_customBufferDescriptors.end() || !it->second.isValid)
        {
            LogError(LogUniform, "Custom Buffer descriptor not found for slot %u", slotId);
            return;
        }

        D3D12_GPU_VIRTUAL_ADDRESS bufferAddress = state->GetGPUVirtualAddress();
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

        // [REFACTORED] Use unified m_bufferStates via GetBufferStateBySlot
        auto* state = GetBufferStateBySlot(slotId, 0); // space=0 for Engine
        if (!state || !state->buffer)
        {
            return 0;
        }

        // Skip if this is a Custom Buffer (space=1)
        if (state->space == BufferSpace::Custom)
        {
            return 0;
        }

        size_t currentIndex = state->GetCurrentRingIndex();

        D3D12_GPU_VIRTUAL_ADDRESS baseAddress    = state->GetGPUVirtualAddress();
        D3D12_GPU_VIRTUAL_ADDRESS currentAddress = baseAddress + currentIndex * state->elementSize;

        return currentAddress;
    }
} // namespace enigma::graphic
