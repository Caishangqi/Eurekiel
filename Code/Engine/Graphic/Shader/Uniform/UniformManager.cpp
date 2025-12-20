// Windows头文件预处理宏
#define WIN32_LEAN_AND_MEAN  // 减少Windows.h包含内容
#define NOMINMAX             // 防止Windows.h定义min/max宏

#include "UniformManager.hpp"

#include "Engine/Core/ErrorWarningAssert.hpp"

#include <cstring>

// Follow the four-layer architecture: UniformManager → D3D12RenderSystem → D12Buffer → DX12 Native#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Resource/BindlessRootSignature.hpp"
#include "Engine/Graphic/Resource/GlobalDescriptorHeapManager.hpp"

namespace enigma::graphic
{
    // [NOTE] PerObjectBufferState::GetDataAt and GetCurrentIndex are now defined in UniformCommon.cpp

    UniformManager::UniformManager()
    {
        // Step 1: Get GlobalDescriptorHeapManager
        auto* heapMgr = D3D12RenderSystem::GetGlobalDescriptorHeapManager();
        if (!heapMgr)
        {
            LogError(LogUniform, "GlobalDescriptorHeapManager not available");
            ERROR_AND_DIE("UniformManager construction failed: GlobalDescriptorHeapManager not available");
        }

        // [FIX] Step 1.5: Get the Descriptor increment size (used to calculate the Ring Descriptor Table offset)
        auto* device = D3D12RenderSystem::GetDevice();
        if (device)
        {
            m_cbvSrvUavDescriptorIncrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        // [FIX] Step 2: Pre-allocate Ring Descriptor Pool
        // Total quantity = MAX_RING_FRAMES * MAX_CUSTOM_BUFFERS (copies per frame * number of slots per copy)
        // For example: 64 * 100 = 6400 Descriptors
        const uint32_t totalDescriptors = BindlessRootSignature::MAX_RING_FRAMES * BindlessRootSignature::MAX_CUSTOM_BUFFERS;
        auto           allocations      = heapMgr->BatchAllocateCustomCbv(totalDescriptors);
        if (allocations.size() != totalDescriptors)
        {
            LogError(LogUniform, "Failed to allocate Ring Descriptor Pool: expected %u, got %zu",
                     totalDescriptors, allocations.size());
            ERROR_AND_DIE(Stringf("UniformManager construction failed: expected %u descriptors, got %zu",
                totalDescriptors, allocations.size()));
        }

        // Step 3: Verify continuity (required by Descriptor Table)
        for (size_t i = 1; i < allocations.size(); i++)
        {
            if (allocations[i].heapIndex != allocations[i - 1].heapIndex + 1)
            {
                LogError(LogUniform, "Ring Descriptor Pool not contiguous: [%zu]=%u, [%zu]=%u",
                         i - 1, allocations[i - 1].heapIndex, i, allocations[i].heapIndex);
                ERROR_AND_DIE(Stringf("UniformManager construction failed: descriptors not contiguous at index %zu", i));
            }
        }

        // 步骤4: 存储第一个Descriptor的GPU Handle（用于SetGraphicsRootDescriptorTable）
        m_customBufferDescriptorTableBaseGPUHandle = allocations[0].gpuHandle;

        // [DIAGNOSTIC] 打印 GPU Handle 的 ptr 值以确认有效性
        LogInfo(LogUniform, "UniformManager constructed - Ring Descriptor Pool Base GPU Handle: ptr=0x%llX, incrementSize=%u",
                m_customBufferDescriptorTableBaseGPUHandle.ptr, m_cbvSrvUavDescriptorIncrementSize);

        // 步骤5: 记录所有分配到Descriptor池
        m_customBufferDescriptorPool.resize(totalDescriptors);
        for (size_t i = 0; i < allocations.size(); i++)
        {
            m_customBufferDescriptorPool[i] = allocations[i];
        }

        m_initialized = true;

        LogInfo(LogUniform, "UniformManager constructed successfully: %u Ring Descriptor Pool (MAX_RING_FRAMES=%u * MAX_CUSTOM_BUFFERS=%u)",
                totalDescriptors, BindlessRootSignature::MAX_RING_FRAMES, BindlessRootSignature::MAX_CUSTOM_BUFFERS);
    }

    UniformManager::~UniformManager()
    {
    }

    bool UniformManager::IsSlotRegistered(uint32_t slot) const
    {
        return m_slotToTypeMap.find(slot) != m_slotToTypeMap.end();
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

    PerObjectBufferState* UniformManager::GetBufferStateBySlot(uint32_t rootSlot) const
    {
        // 查找slot是否已注册
        auto slotIt = m_slotToTypeMap.find(rootSlot);
        if (slotIt == m_slotToTypeMap.end())
        {
            return nullptr; // Slot未注册，静默返回
        }

        // 获取TypeId并查找BufferState（使用引用避免std::type_index复制构造）
        const std::type_index& typeId   = slotIt->second;
        auto                   bufferIt = m_perObjectBuffers.find(typeId);
        if (bufferIt != m_perObjectBuffers.end())
        {
            return const_cast<PerObjectBufferState*>(&bufferIt->second);
        }

        // TypeId已注册但Buffer未创建（异常情况）
        LogWarn("UniformManager", "Slot %u registered but buffer not created for TypeId: %s", rootSlot, typeId.name());
        ERROR_RECOVERABLE(Stringf("Slot %u registered but buffer not created for TypeId: %s", rootSlot, typeId.name()));
        return nullptr;
    }

    void UniformManager::UpdateRingBufferOffsets(UpdateFrequency frequency)
    {
        // Get all slots of the specified frequency
        const auto& slots = GetSlotsByFrequency(frequency);
        if (slots.empty())
        {
            return; // There is no Buffer registered for this frequency
        }

        // Traverse all slots, update Ring Buffer offset and perform Delayed Fill
        for (uint32_t slotId : slots)
        {
            // [FIX] Check if this is a Custom Buffer (stored in m_customBufferStates)
            auto customIt = m_customBufferStates.find(slotId);
            if (customIt != m_customBufferStates.end())
            {
                // Custom Buffer path - use CustomBufferState
                CustomBufferState* customState = customIt->second.get();
                if (!customState || !customState->gpuBuffer)
                {
                    continue;
                }

                // [NOTE] Custom Buffer offset update is handled in UpdateDescriptorTableOffset
                // which is called during UploadCustomBuffer, so we just skip here
                // The Ring Buffer index is managed per-draw via IncrementDrawCount()
                continue;
            }

            // Engine Buffer path - use PerObjectBufferState
            auto* state = GetBufferStateBySlot(slotId);
            if (!state || !state->gpuBuffer)
            {
                continue; // The slot is not registered or the Buffer is not created, skip
            }

            // Step 1: Calculate the current Ring Buffer index
            size_t currentIndex = (frequency == UpdateFrequency::PerObject)
                                      ? (m_currentDrawCount % state->maxCount)
                                      : 0;

            // Step 2: Delayed Fill mechanism
            // If the current index has not been updated, automatically copy the last updated value to avoid repeated uploads
            if (state->lastUpdatedIndex != currentIndex)
            {
                void* destPtr = state->GetDataAt(currentIndex);
                std::memcpy(destPtr, state->lastUpdatedValue.data(), state->elementSize);
            }

            // Step 3: Update offset (select different paths according to Buffer type)
            // [NOTE] Only Engine buffers reach here, Custom buffers are handled above
            UpdateRootCBVOffset(slotId, currentIndex);
        }
    }

    // ========================================================================
    // [NEW] RegisterEngineBuffer_Internal实现
    // ========================================================================
    bool UniformManager::RegisterEngineBuffer(
        uint32_t        slotId,
        std::type_index typeId,
        size_t          bufferSize,
        UpdateFrequency freq,
        size_t          maxDraws,
        uint32_t        space)
    {
        // [REFACTOR] Validate space must be 0 for Engine buffers - throw exception
        if (space != 0)
        {
            throw UniformBufferException("Engine Buffer must use space=0", slotId, space);
        }

        // Step 1: Validate Slot range (Engine Buffer must use slot 0-14)
        if (!BufferHelper::IsEngineReservedSlot(slotId))
        {
            throw UniformBufferException("Engine Buffer slot must be 0-14", slotId, space);
        }

        // Step 2: Check Slot conflict
        if (IsSlotRegistered(slotId))
        {
            throw UniformBufferException("Slot already registered", slotId, space);
        }

        // Step 3: Calculate 256-byte aligned Buffer size
        size_t alignedSize = BufferHelper::CalculateAlignedSize(bufferSize);

        // Step 4: Calculate Ring Buffer count based on frequency
        size_t ringBufferCount = 1;
        switch (freq)
        {
        case UpdateFrequency::PerObject:
            ringBufferCount = maxDraws; // Default 10000
            break;
        case UpdateFrequency::PerPass:
            ringBufferCount = 20; // Conservative estimate
            break;
        case UpdateFrequency::PerFrame:
        case UpdateFrequency::Static:
            ringBufferCount = 1;
            break;
        }

        size_t totalSize = alignedSize * ringBufferCount;

        // Step 5: Create GPU Buffer
        BufferCreateInfo createInfo;
        createInfo.size          = totalSize;
        createInfo.usage         = BufferUsage::ConstantBuffer;
        createInfo.memoryAccess  = MemoryAccess::CPUToGPU; // Upload Heap
        createInfo.initialData   = nullptr;
        std::string debugNameStr = Stringf("EngineBuffer_Slot%u", slotId);
        createInfo.debugName     = debugNameStr.c_str();

        auto gpuBuffer = D3D12RenderSystem::CreateBuffer(createInfo);
        if (!gpuBuffer)
        {
            throw UniformBufferException("Failed to create GPU buffer", slotId, space);
        }

        // Step 6: Persistent mapping
        void* mappedData = gpuBuffer->MapPersistent();
        if (!mappedData)
        {
            throw UniformBufferException("Failed to map GPU buffer", slotId, space);
        }

        // Step 7: Create PerObjectBufferState
        PerObjectBufferState state;
        state.gpuBuffer    = std::move(gpuBuffer);
        state.mappedData   = mappedData;
        state.elementSize  = alignedSize;
        state.maxCount     = ringBufferCount;
        state.frequency    = freq;
        state.currentIndex = 0;
        state.lastUpdatedValue.resize(alignedSize, 0);
        state.lastUpdatedIndex = SIZE_MAX;

        // Step 8: Store to m_perObjectBuffers using typeId
        m_perObjectBuffers[typeId] = std::move(state);

        // Step 9: Add to frequency classification
        m_frequencyToSlotsMap[freq].push_back(slotId);

        LogInfo(LogUniform, "Engine Buffer registered: Slot=%u, Size=%zu, Frequency=%d, Count=%zu",
                slotId, alignedSize, static_cast<int>(freq), ringBufferCount);
        return true;
    }

    // ========================================================================
    // [NEW] RegisterCustomBuffer实现
    // ========================================================================
    bool UniformManager::RegisterCustomBuffer(
        uint32_t slotId,
        std::type_index /*typeId*/, // [NOTE] typeId unused - Custom buffers use slotId as key
        size_t          bufferSize,
        UpdateFrequency freq,
        size_t          maxDraws,
        uint32_t        space)
    {
        // [REFACTOR] Validate space must be 1 for Custom buffers - throw exception
        if (space != 1)
        {
            throw UniformBufferException("Custom Buffer must use space=1", slotId, space);
        }

        // [NOTE] Custom Buffer uses space=1, shader must use register(bN, space1)
        // Slot range validation: allow any slot 0-99 (unlike Engine Buffer 0-14 limit)
        if (slotId >= BindlessRootSignature::MAX_CUSTOM_BUFFERS)
        {
            throw UniformBufferException("Custom Buffer slot exceeds MAX_CUSTOM_BUFFERS", slotId, space);
        }

        // [WARNING] Slot 0-14 conflict with engine reserved slots (warn only, allow registration)
        // When using space=1, even slots in 0-14 range use Descriptor Table binding
        if (BufferHelper::IsEngineReservedSlot(slotId))
        {
            LogWarn(LogUniform, "Custom Buffer slot %u is in engine reserved range (0-14), "
                    "ensure shader uses register(b%u, space1) to avoid conflicts", slotId, slotId);
        }

        // Step 2: Check Slot conflict
        if (IsSlotRegistered(slotId))
        {
            throw UniformBufferException("Slot already registered", slotId, space);
        }

        // Step 3: Calculate 256-byte aligned Buffer size
        size_t alignedSize = BufferHelper::CalculateAlignedSize(bufferSize);

        // Step 4: Calculate Ring Buffer count based on frequency
        size_t ringBufferCount = 1;
        switch (freq)
        {
        case UpdateFrequency::PerObject:
            ringBufferCount = maxDraws;
            break;
        case UpdateFrequency::PerPass:
            ringBufferCount = 20;
            break;
        case UpdateFrequency::PerFrame:
        case UpdateFrequency::Static:
            ringBufferCount = 1;
            break;
        }

        size_t totalSize = alignedSize * ringBufferCount;

        // Step 5: Create GPU Buffer
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

        // Step 6: Persistent mapping
        void* mappedData = gpuBuffer->MapPersistent();
        if (!mappedData)
        {
            throw UniformBufferException("Failed to map GPU buffer", slotId, space);
        }

        // Step 7: Allocate Descriptor
        if (!AllocateCustomBufferDescriptor(slotId))
        {
            throw DescriptorHeapException("Failed to allocate descriptor for Custom Buffer");
        }

        // [FIX] Step 7.5: Create CBV for ALL Ring Frames (Ring Descriptor Table 架构)
        // 为每个 ringFrame 创建独立的 CBV，解决 CBV 覆盖问题
        auto* device = D3D12RenderSystem::GetDevice();
        if (!device)
        {
            throw DescriptorHeapException("D3D12 device not available for CBV creation");
        }

        D3D12_GPU_VIRTUAL_ADDRESS baseGpuAddress = gpuBuffer->GetGPUVirtualAddress();

        // [FIX] 确保 ringBufferCount >= MAX_RING_FRAMES，避免越界
        size_t effectiveRingCount = std::min(ringBufferCount, static_cast<size_t>(BindlessRootSignature::MAX_RING_FRAMES));

        for (uint32_t ringFrame = 0; ringFrame < BindlessRootSignature::MAX_RING_FRAMES; ++ringFrame)
        {
            // [FIX] 计算 Descriptor 在 Ring Descriptor Pool 中的位置
            // Layout: [ringFrame0: slot0,slot1,...,slot99][ringFrame1: slot0,slot1,...,slot99]...
            uint32_t descriptorIndex = ringFrame * BindlessRootSignature::MAX_CUSTOM_BUFFERS + slotId;

            if (descriptorIndex >= m_customBufferDescriptorPool.size())
            {
                LogError(LogUniform, "Descriptor index %u out of range (pool size=%zu)", descriptorIndex, m_customBufferDescriptorPool.size());
                continue;
            }

            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_customBufferDescriptorPool[descriptorIndex].cpuHandle;

            // [FIX] 计算 Ring Buffer slice 的 GPU 地址
            // ringFrame 对应的 slice = ringFrame % ringBufferCount（处理 ringBufferCount < MAX_RING_FRAMES 的情况）
            size_t                    sliceIndex   = ringFrame % effectiveRingCount;
            D3D12_GPU_VIRTUAL_ADDRESS sliceAddress = baseGpuAddress + sliceIndex * alignedSize;

            // Create CBV descriptor
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation                  = sliceAddress;
            cbvDesc.SizeInBytes                     = static_cast<UINT>(alignedSize);

            device->CreateConstantBufferView(&cbvDesc, cpuHandle);
        }

        LogInfo(LogUniform, "Created %u CBVs for Custom Buffer Slot=%u (Ring Descriptor Table), Base GPU Address=0x%llX",
                BindlessRootSignature::MAX_RING_FRAMES, slotId, baseGpuAddress);

        // [NOTE] 保留 m_customBufferDescriptors 的兼容性（存储第一个 ringFrame 的 Descriptor）
        auto                        it             = m_customBufferDescriptors.find(slotId);
        D3D12_CPU_DESCRIPTOR_HANDLE firstCpuHandle = {};
        if (it != m_customBufferDescriptors.end() && it->second.isValid)
        {
            firstCpuHandle = it->second.allocation.cpuHandle;
        }

        // [REFACTOR] Step 8: Create CustomBufferState instead of PerObjectBufferState
        auto customState              = std::make_unique<CustomBufferState>();
        customState->elementSize      = alignedSize;
        customState->maxDraws         = ringBufferCount;
        customState->currentDrawIndex = 0;
        customState->gpuBuffer        = gpuBuffer->GetResource(); // [REFACTOR] Store ComPtr<ID3D12Resource>
        customState->mappedPtr        = static_cast<uint8_t*>(mappedData);
        customState->bindlessIndex    = it->second.allocation.heapIndex;
        customState->cpuHandle        = firstCpuHandle;
        customState->slot             = slotId;
        customState->frequency        = freq;
        customState->isDirty          = false;

        // [REFACTOR] Step 9: Store in m_customBufferStates instead of m_perObjectBuffers
        m_customBufferStates[slotId] = std::move(customState);

        // [NOTE] Step 10 removed: TypeId mappings are updated in RegisterBuffer template method
        // to avoid duplicate updates and ensure consistency

        // Step 11: Add to frequency classification
        m_frequencyToSlotsMap[freq].push_back(slotId);

        LogInfo(LogUniform, "Custom Buffer registered: Slot=%u, Space=1, Size=%zu, MaxDraws=%zu",
                slotId, alignedSize, ringBufferCount);
        return true;
    }

    // ========================================================================
    // [NEW] UploadEngineBuffer实现
    // ========================================================================
    bool UniformManager::UploadEngineBuffer(uint32_t slotId, const void* data, size_t size)
    {
        // 步骤1: 获取Buffer状态
        auto* state = GetBufferStateBySlot(slotId);
        if (!state)
        {
            LogError(LogUniform, "Engine Buffer slot %u not registered", slotId);
            return false;
        }

        // 步骤2: 验证数据大小
        if (size > state->elementSize)
        {
            LogWarn(LogUniform, "Data size (%zu) exceeds buffer element size (%zu)", size, state->elementSize);
        }

        // 步骤3: 计算当前Ring Buffer索引
        size_t currentIndex = state->GetCurrentIndex();

        // 步骤4: 复制数据到GPU Buffer
        void* destPtr = state->GetDataAt(currentIndex);
        std::memcpy(destPtr, data, size);

        // 步骤5: 更新Delayed Fill缓存
        std::memcpy(state->lastUpdatedValue.data(), data, size);
        state->lastUpdatedIndex = currentIndex;

        // 步骤6: 递增索引
        state->currentIndex++;

        return true;
    }

    // ========================================================================
    // [REFACTOR] UploadCustomBuffer - uses CustomBufferState from m_customBufferStates
    // ========================================================================
    bool UniformManager::UploadCustomBuffer(uint32_t slotId, const void* data, size_t size)
    {
        // [REFACTOR] Step 1: Find Custom Buffer state in m_customBufferStates
        auto it = m_customBufferStates.find(slotId);
        if (it == m_customBufferStates.end())
        {
            LogError(LogUniform, "Custom Buffer slot %u not registered", slotId);
            return false;
        }

        CustomBufferState* state = it->second.get();

        // Step 2: 验证数据大小
        if (size > state->elementSize)
        {
            LogWarn(LogUniform, "Data size (%zu) exceeds buffer element size (%zu)", size, state->elementSize);
        }

        // [REFACTOR] Step 3: Calculate Ring Buffer index using currentDrawIndex
        uint32_t ringIndex = state->currentDrawIndex % static_cast<uint32_t>(state->maxDraws);
        size_t   offset    = ringIndex * state->elementSize;

        // Step 4: Copy data to Ring Buffer slice
        std::memcpy(state->mappedPtr + offset, data, size);

        // [FIX] Step 5: CBV 已在 RegisterCustomBuffer 中预创建 (Ring Descriptor Table 架构)
        // 不再每次 Upload 时更新 CBV Descriptor，避免覆盖问题
        // 只需写入数据到 Ring Buffer，GPU 通过 ringIndex 选择正确的 CBV

        // [FIX] Step 6: 不在这里递增 currentDrawIndex
        // 计数器统一由 IncrementDrawCount() 管理（在每次 Draw 后调用）
        // 这样确保 Upload 和 Bind 使用相同的 ring index

        // Step 7: Mark as dirty
        state->isDirty = true;

        return true;
    }

    // ========================================================================
    // [NEW] UpdateRootCBVOffset实现
    // ========================================================================
    void UniformManager::UpdateRootCBVOffset(uint32_t slotId, size_t currentIndex)
    {
        // [ARCHITECTURE] Root CBV binding is responsible for RendererSubsystem
        // UniformManager is only responsible for updating Ring Buffer data and Descriptor
        // RendererSubsystem will call GetEngineBufferGPUAddress() to obtain the address and then call SetGraphicsRootConstantBufferView
        //
        // Keep the implementation empty because:
        // 1. UniformManager does not hold CommandList (architecture separation)
        // 2. Root CBV binding is performed in the Draw function and requires the CommandList context
        // 3. This method is only used as a placeholder for the architecture, and the actual binding logic is in RendererSubsystem::DrawXXX()
        UNUSED(slotId)
        UNUSED(currentIndex)
    }

    // ========================================================================
    // [NEW] UpdateDescriptorTableOffset实现 - 动态更新CBV Descriptor
    // ========================================================================
    void UniformManager::UpdateDescriptorTableOffset(uint32_t slotId, size_t currentIndex)
    {
        // 步骤1: 获取Buffer状态
        auto* state = GetBufferStateBySlot(slotId);
        if (!state || !state->gpuBuffer)
        {
            return; // Buffer未注册或未创建，静默返回
        }

        // 步骤2: 获取Custom Buffer的Descriptor分配
        auto it = m_customBufferDescriptors.find(slotId);
        if (it == m_customBufferDescriptors.end() || !it->second.isValid)
        {
            LogError(LogUniform, "Custom Buffer descriptor not allocated for slot %u", slotId);
            return;
        }

        // 步骤3: 计算当前Ring Buffer slice的GPU虚拟地址
        D3D12_GPU_VIRTUAL_ADDRESS bufferAddress = state->gpuBuffer->GetGPUVirtualAddress();
        bufferAddress                           += currentIndex * state->elementSize;

        // 步骤4: 创建CBV描述符
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation                  = bufferAddress;
        cbvDesc.SizeInBytes                     = static_cast<UINT>(state->elementSize);

        // 步骤5: 使用GlobalDescriptorHeapManager分配的CPU Handle更新Descriptor
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = it->second.allocation.cpuHandle;

        auto* device = D3D12RenderSystem::GetDevice();
        if (!device)
        {
            LogError(LogUniform, "D3D12 device not available");
            return;
        }

        device->CreateConstantBufferView(&cbvDesc, cpuHandle);

        // [REMOVED] LogDebug太频繁，影响性能
    }

    // ========================================================================
    // [NEW] AllocateCustomBufferDescriptor实现 - 直接映射Slot到Index
    // ========================================================================
    bool UniformManager::AllocateCustomBufferDescriptor(uint32_t slotId)
    {
        // 步骤1: 检查是否已分配
        if (m_customBufferDescriptors.find(slotId) != m_customBufferDescriptors.end())
        {
            LogWarn(LogUniform, "Custom Buffer descriptor already allocated for slot %u", slotId);
            return true; // 已分配，视为成功
        }

        // 步骤2: 验证初始化状态
        if (!m_initialized || m_customBufferDescriptorPool.empty())
        {
            LogError(LogUniform, "UniformManager not initialized or descriptor pool empty");
            return false;
        }

        // 步骤3: [FIX] 直接使用Slot作为Descriptor索引（直接映射）
        // 原逻辑：descriptorIndex = slotId - ROOT_DESCRIPTOR_TABLE_CUSTOM（导致Slot 88 → Index 73）
        // 新逻辑：descriptorIndex = slotId（直接映射，Slot 88 → Index 88）
        uint32_t descriptorIndex = slotId;

        if (descriptorIndex >= BindlessRootSignature::MAX_CUSTOM_BUFFERS)
        {
            LogError(LogUniform, "Custom Buffer slot %u exceeds MAX_CUSTOM_BUFFERS (%u)",
                     slotId, BindlessRootSignature::MAX_CUSTOM_BUFFERS);
            return false;
        }

        // 步骤4: 从预分配池中获取Descriptor（池索引 = Slot编号）
        auto allocation = m_customBufferDescriptorPool[descriptorIndex];

        if (!allocation.isValid)
        {
            LogError(LogUniform, "Failed to get pre-allocated descriptor for slot %u (index %u)",
                     slotId, descriptorIndex);
            return false;
        }

        // 步骤5: 创建CustomBufferDescriptor并存储
        CustomBufferDescriptor desc;
        desc.allocation = allocation;
        desc.slotId     = slotId;
        desc.isValid    = true;

        m_customBufferDescriptors[slotId] = desc;

        LogInfo(LogUniform, "Allocated Custom Buffer descriptor: Slot=%u → Descriptor Index=%u (direct mapping)",
                slotId, descriptorIndex);
        return true;
    }

    // ========================================================================
    // [NEW] FreeCustomBufferDescriptor实现
    // ========================================================================
    void UniformManager::FreeCustomBufferDescriptor(uint32_t slotId)
    {
        auto it = m_customBufferDescriptors.find(slotId);
        if (it != m_customBufferDescriptors.end())
        {
            m_customBufferDescriptors.erase(it);
            LogInfo(LogUniform, "Custom Buffer descriptor freed: Slot=%u", slotId);
        }
    }

    // ========================================================================
    // [FIX] GetCustomBufferDescriptorTableGPUHandle implementation - Ring Descriptor Table architecture
    // ========================================================================
    D3D12_GPU_DESCRIPTOR_HANDLE UniformManager::GetCustomBufferDescriptorTableGPUHandle(uint32_t ringIndex) const
    {
        // [FIX] Calculate Descriptor Table offset based on ringIndex
        // Layout: [ringFrame0: slot0,slot1,...,slot99][ringFrame1: slot0,slot1,...,slot99]...
        //Each ringFrame occupies MAX_CUSTOM_BUFFERS Descriptors
        D3D12_GPU_DESCRIPTOR_HANDLE handle = m_customBufferDescriptorTableBaseGPUHandle;

        // ringIndex % MAX_RING_FRAMES ensures that the index is within the valid range
        uint32_t effectiveRingIndex = ringIndex % BindlessRootSignature::MAX_RING_FRAMES;

        // Calculate offset: ringIndex * MAX_CUSTOM_BUFFERS * incrementSize
        handle.ptr += effectiveRingIndex * BindlessRootSignature::MAX_CUSTOM_BUFFERS * m_cbvSrvUavDescriptorIncrementSize;

        return handle;
    }

    // ========================================================================
    // [NEW] GetCustomBufferCPUHandle实现
    // ========================================================================
    D3D12_CPU_DESCRIPTOR_HANDLE UniformManager::GetCustomBufferCPUHandle(uint32_t slotId) const
    {
        auto it = m_customBufferDescriptors.find(slotId);
        if (it != m_customBufferDescriptors.end())
        {
            return it->second.allocation.cpuHandle;
        }

        // 返回空句柄
        D3D12_CPU_DESCRIPTOR_HANDLE nullHandle = {};
        nullHandle.ptr                         = 0;
        return nullHandle;
    }

    // ========================================================================
    // [NEW] GetEngineBufferGPUAddress实现 - 用于Root CBV绑定
    // ========================================================================
    D3D12_GPU_VIRTUAL_ADDRESS UniformManager::GetEngineBufferGPUAddress(uint32_t slotId) const
    {
        // 步骤1: 验证是否为Engine Buffer Slot
        if (!BufferHelper::IsEngineReservedSlot(slotId))
        {
            LogError(LogUniform, "Slot %u is not an engine reserved slot", slotId);
            return 0;
        }

        // [FIX] 步骤1.5: 检查该slot是否被注册为Custom Buffer
        // Custom Buffer使用space1，通过Descriptor Table绑定，不需要Root CBV
        // 如果slot被Custom Buffer占用，直接返回0跳过Engine Buffer绑定
        if (m_customBufferStates.find(slotId) != m_customBufferStates.end())
        {
            return 0; // Slot被Custom Buffer占用，跳过Engine Buffer绑定
        }

        // 步骤2: 获取Buffer状态
        auto* state = GetBufferStateBySlot(slotId);
        if (!state || !state->gpuBuffer)
        {
            return 0; // Buffer未注册或未创建，返回0
        }

        // 步骤3: 计算当前Ring Buffer索引
        size_t currentIndex = 0;
        switch (state->frequency)
        {
        case UpdateFrequency::PerObject:
            currentIndex = m_currentDrawCount % state->maxCount;
            break;
        case UpdateFrequency::PerPass:
        case UpdateFrequency::PerFrame:
        case UpdateFrequency::Static:
            currentIndex = 0; // 非PerObject频率使用索引0
            break;
        }

        // 步骤4: 计算GPU虚拟地址
        D3D12_GPU_VIRTUAL_ADDRESS baseAddress    = state->gpuBuffer->GetGPUVirtualAddress();
        D3D12_GPU_VIRTUAL_ADDRESS currentAddress = baseAddress + currentIndex * state->elementSize;

        return currentAddress;
    }
} // namespace enigma::graphic
