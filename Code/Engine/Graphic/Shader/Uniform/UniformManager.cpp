// Windows头文件预处理宏
#define WIN32_LEAN_AND_MEAN  // 减少Windows.h包含内容
#define NOMINMAX             // 防止Windows.h定义min/max宏

#include "UniformManager.hpp"

#include "Engine/Core/ErrorWarningAssert.hpp"

#include <cstring>

// 遵循四层架构: UniformManager → D3D12RenderSystem → D12Buffer → DX12 Native
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Resource/BindlessRootSignature.hpp"
#include "Engine/Graphic/Resource/GlobalDescriptorHeapManager.hpp"

namespace enigma::graphic
{
    void* PerObjectBufferState::GetDataAt(size_t index)
    {
        return static_cast<uint8_t*>(mappedData) + index * elementSize;
    }

    size_t PerObjectBufferState::GetCurrentIndex() const
    {
        switch (frequency)
        {
        case UpdateFrequency::PerObject:
            return currentIndex % maxCount;
        case UpdateFrequency::PerPass:
        case UpdateFrequency::PerFrame:
            return 0; // single index
        default:
            return 0;
        }
    }

    UniformManager::UniformManager()
    {
        // [RAII] 构造函数完成所有初始化工作
        
        // 步骤1: 获取GlobalDescriptorHeapManager
        auto* heapMgr = D3D12RenderSystem::GetGlobalDescriptorHeapManager();
        if (!heapMgr)
        {
            LogError(LogRenderer, "GlobalDescriptorHeapManager not available");
            ERROR_AND_DIE("UniformManager construction failed: GlobalDescriptorHeapManager not available");
        }

        // 步骤2: 预分配100个连续的Custom Buffer CBV
        auto allocations = heapMgr->BatchAllocateCustomCbv(BindlessRootSignature::MAX_CUSTOM_BUFFERS);
        if (allocations.size() != BindlessRootSignature::MAX_CUSTOM_BUFFERS)
        {
            LogError(LogRenderer, "Failed to allocate Custom Buffer descriptors: expected %u, got %zu",
                     BindlessRootSignature::MAX_CUSTOM_BUFFERS, allocations.size());
            ERROR_AND_DIE(Stringf("UniformManager construction failed: expected %u descriptors, got %zu",
                                  BindlessRootSignature::MAX_CUSTOM_BUFFERS, allocations.size()));
        }

        // 步骤3: 验证连续性（Descriptor Table要求）
        for (size_t i = 1; i < allocations.size(); i++)
        {
            if (allocations[i].heapIndex != allocations[i - 1].heapIndex + 1)
            {
                LogError(LogRenderer, "Custom Buffer descriptors are not contiguous: [%zu]=%u, [%zu]=%u",
                         i - 1, allocations[i - 1].heapIndex, i, allocations[i].heapIndex);
                ERROR_AND_DIE(Stringf("UniformManager construction failed: descriptors not contiguous at index %zu", i));
            }
        }

        // 步骤4: 存储第一个Descriptor的GPU Handle（用于SetGraphicsRootDescriptorTable）
        m_customBufferDescriptorTableBaseGPUHandle = allocations[0].gpuHandle;

        // [DIAGNOSTIC] 打印 GPU Handle 的 ptr 值以确认有效性
        LogInfo(LogRenderer, "UniformManager constructed - Custom Buffer Descriptor Table Base GPU Handle: ptr=0x%llX",
                m_customBufferDescriptorTableBaseGPUHandle.ptr);

        // 步骤5: 记录所有分配到Descriptor池
        m_customBufferDescriptorPool.resize(BindlessRootSignature::MAX_CUSTOM_BUFFERS);
        for (size_t i = 0; i < allocations.size(); i++)
        {
            m_customBufferDescriptorPool[i] = allocations[i];
        }

        m_initialized = true;

        LogInfo(LogRenderer, "UniformManager constructed successfully: %u Custom Buffer descriptors pre-allocated (contiguous)",
                BindlessRootSignature::MAX_CUSTOM_BUFFERS);
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

    // ========================================================================
    // [NEW] UpdateRingBufferOffsets实现 - 集成Delayed Fill机制
    // ========================================================================
    void UniformManager::UpdateRingBufferOffsets(UpdateFrequency frequency)
    {
        // 获取指定频率的所有slot
        const auto& slots = GetSlotsByFrequency(frequency);
        if (slots.empty())
        {
            return; // 没有注册该频率的Buffer
        }

        // 遍历所有slot，更新Ring Buffer offset并执行Delayed Fill
        for (uint32_t slotId : slots)
        {
            auto* state = GetBufferStateBySlot(slotId);
            if (!state || !state->gpuBuffer)
            {
                continue; // slot未注册或Buffer未创建，跳过
            }

            // 步骤1: 计算当前Ring Buffer索引
            size_t currentIndex = (frequency == UpdateFrequency::PerObject)
                                      ? (m_currentDrawCount % state->maxCount)
                                      : 0;

            // 步骤2: Delayed Fill机制（参考RendererSubsystem.cpp:2391-2400）
            // 如果当前索引未被更新，自动复制上次更新的值，避免重复上传
            if (state->lastUpdatedIndex != currentIndex)
            {
                void* destPtr = state->GetDataAt(currentIndex);
                std::memcpy(destPtr, state->lastUpdatedValue.data(), state->elementSize);

                // [REMOVED] LogDebug太频繁，影响性能，仅在需要时启用
            }

            // 步骤3: 更新offset（根据Buffer类型选择不同路径）
            if (BufferHelper::IsEngineReservedSlot(slotId))
            {
                // Root CBV路径（引擎Buffer）
                // [NOTE] 实现为空，因为Root CBV在Draw时直接绑定
                UpdateRootCBVOffset(slotId, currentIndex);
            }
            else
            {
                // Descriptor Table路径（Custom Buffer）
                // 动态更新CBV Descriptor指向当前Ring Buffer slice
                UpdateDescriptorTableOffset(slotId, currentIndex);
            }
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
        UNUSED(space)
        // 步骤1: 验证Slot范围
        if (!BufferHelper::IsEngineReservedSlot(slotId))
        {
            LogError(LogRenderer, "Slot %u is not an engine reserved slot (0-14)", slotId);
            return false;
        }

        // 步骤2: 检查Slot冲突
        if (IsSlotRegistered(slotId))
        {
            LogError(LogRenderer, "Slot %u already registered!", slotId);
            return false;
        }

        // 步骤3: 计算256字节对齐的Buffer大小
        size_t alignedSize = BufferHelper::CalculateAlignedSize(bufferSize);

        // 步骤4: 根据频率计算Ring Buffer数量
        size_t ringBufferCount = 1;
        switch (freq)
        {
        case UpdateFrequency::PerObject:
            ringBufferCount = maxDraws; // 默认10000
            break;
        case UpdateFrequency::PerPass:
            ringBufferCount = 20; // 保守估计
            break;
        case UpdateFrequency::PerFrame:
        case UpdateFrequency::Static:
            ringBufferCount = 1;
            break;
        }

        size_t totalSize = alignedSize * ringBufferCount;

        // 步骤5: 创建GPU Buffer
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
            LogError(LogRenderer, "Failed to create engine buffer for slot %u", slotId);
            return false;
        }

        // 步骤6: 持久映射
        void* mappedData = gpuBuffer->MapPersistent();
        if (!mappedData)
        {
            LogError(LogRenderer, "Failed to map engine buffer for slot %u", slotId);
            return false;
        }

        // 步骤7: 创建PerObjectBufferState
        PerObjectBufferState state;
        state.gpuBuffer    = std::move(gpuBuffer);
        state.mappedData   = mappedData;
        state.elementSize  = alignedSize;
        state.maxCount     = ringBufferCount;
        state.frequency    = freq;
        state.currentIndex = 0;
        state.lastUpdatedValue.resize(alignedSize, 0);
        state.lastUpdatedIndex = SIZE_MAX;

        // 步骤8: 直接使用传入的typeId存储到m_perObjectBuffers
        m_perObjectBuffers[typeId] = std::move(state);

        // 步骤9: 添加到频率分类
        m_frequencyToSlotsMap[freq].push_back(slotId);

        LogInfo(LogRenderer, "Engine Buffer registered: Slot=%u, Size=%zu, Frequency=%d, Count=%zu",
                slotId, alignedSize, static_cast<int>(freq), ringBufferCount);
        return true;
    }

    // ========================================================================
    // [NEW] RegisterCustomBuffer实现
    // ========================================================================
    bool UniformManager::RegisterCustomBuffer(
        uint32_t        slotId,
        std::type_index typeId,
        size_t          bufferSize,
        UpdateFrequency freq,
        size_t          maxDraws,
        uint32_t        space)
    {
        UNUSED(space)

        // [IMPORTANT] Shader绑定规则：
        // - Slot 0-14:  使用Root CBV (space0)，shader中使用 register(bN)
        // - Slot 15-99: 使用Descriptor Table (space1)，shader中必须使用 register(bN, space1)
        if (slotId >= BindlessRootSignature::MAX_CUSTOM_BUFFERS)
        {
            LogError(LogRenderer, "Custom Buffer slot %u exceeds MAX_CUSTOM_BUFFERS (%u), must be in range [0, %u]",
                     slotId, BindlessRootSignature::MAX_CUSTOM_BUFFERS, BindlessRootSignature::MAX_CUSTOM_BUFFERS - 1);
            return false;
        }

        // [WARNING] Slot 0-14 与引擎保留Slot冲突检查
        // 虽然技术上可以使用Slot 0-14，但会与引擎保留的Root CBV冲突
        // 建议使用Slot 15+以避免冲突，且必须在shader中使用space1
        if (BufferHelper::IsEngineReservedSlot(slotId))
        {
            LogWarn(LogRenderer, "Custom Buffer slot %u conflicts with engine reserved slot (0-14), "
                    "this may cause binding conflicts. Recommend using slot >= 15 with space1 in shader", slotId);
        }

        // 步骤2: 检查Slot冲突
        if (IsSlotRegistered(slotId))
        {
            LogError(LogRenderer, "Slot %u already registered!", slotId);
            return false;
        }

        // 步骤3: 计算256字节对齐的Buffer大小
        size_t alignedSize = BufferHelper::CalculateAlignedSize(bufferSize);

        // 步骤4: 根据频率计算Ring Buffer数量
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

        // 步骤5: 创建GPU Buffer
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
            LogError(LogRenderer, "Failed to create Custom Buffer for slot %u", slotId);
            return false;
        }

        // 步骤6: 持久映射
        void* mappedData = gpuBuffer->MapPersistent();
        if (!mappedData)
        {
            LogError(LogRenderer, "Failed to map Custom Buffer for slot %u", slotId);
            return false;
        }

        // 步骤7: 分配Descriptor
        if (!AllocateCustomBufferDescriptor(slotId))
        {
            LogError(LogRenderer, "Failed to allocate descriptor for Custom Buffer slot %u", slotId);
            return false;
        }

        // 步骤7.5: [FIX] 创建初始CBV到Descriptor中
        // 获取分配的Descriptor的CPU Handle
        auto it = m_customBufferDescriptors.find(slotId);
        if (it == m_customBufferDescriptors.end() || !it->second.isValid)
        {
            LogError(LogRenderer, "Failed to get allocated descriptor for Custom Buffer slot %u", slotId);
            return false;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = it->second.allocation.cpuHandle;

        // 创建CBV描述符（指向Ring Buffer的第一个slice）
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation                  = gpuBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes                     = static_cast<UINT>(alignedSize);

        auto* device = D3D12RenderSystem::GetDevice();
        if (!device)
        {
            LogError(LogRenderer, "D3D12 device not available");
            return false;
        }

        device->CreateConstantBufferView(&cbvDesc, cpuHandle);

        LogInfo(LogRenderer, "Created initial CBV for Custom Buffer Slot=%u at Descriptor Index=%u, GPU Address=0x%llX",
                slotId, it->second.allocation.heapIndex, cbvDesc.BufferLocation);

        // 步骤8: 创建PerObjectBufferState
        PerObjectBufferState state;
        state.gpuBuffer    = std::move(gpuBuffer);
        state.mappedData   = mappedData;
        state.elementSize  = alignedSize;
        state.maxCount     = ringBufferCount;
        state.frequency    = freq;
        state.currentIndex = 0;
        state.lastUpdatedValue.resize(alignedSize, 0);
        state.lastUpdatedIndex = SIZE_MAX;

        // 步骤9: 直接使用传入的typeId存储到m_perObjectBuffers
        m_perObjectBuffers[typeId] = std::move(state);

        // 步骤10: 添加到频率分类
        m_frequencyToSlotsMap[freq].push_back(slotId);

        LogInfo(LogRenderer, "Custom Buffer registered: Slot=%u, Size=%zu, Frequency=%d, Count=%zu",
                slotId, alignedSize, static_cast<int>(freq), ringBufferCount);
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
            LogError(LogRenderer, "Engine Buffer slot %u not registered", slotId);
            return false;
        }

        // 步骤2: 验证数据大小
        if (size > state->elementSize)
        {
            LogWarn(LogRenderer, "Data size (%zu) exceeds buffer element size (%zu)", size, state->elementSize);
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
    // [NEW] UploadCustomBuffer实现
    // ========================================================================
    bool UniformManager::UploadCustomBuffer(uint32_t slotId, const void* data, size_t size)
    {
        // 步骤1: 获取Buffer状态
        auto* state = GetBufferStateBySlot(slotId);
        if (!state)
        {
            LogError(LogRenderer, "Custom Buffer slot %u not registered", slotId);
            return false;
        }

        // 步骤2: 验证数据大小
        if (size > state->elementSize)
        {
            LogWarn(LogRenderer, "Data size (%zu) exceeds buffer element size (%zu)", size, state->elementSize);
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
            LogError(LogRenderer, "Custom Buffer descriptor not allocated for slot %u", slotId);
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
            LogError(LogRenderer, "D3D12 device not available");
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
            LogWarn(LogRenderer, "Custom Buffer descriptor already allocated for slot %u", slotId);
            return true; // 已分配，视为成功
        }

        // 步骤2: 验证初始化状态
        if (!m_initialized || m_customBufferDescriptorPool.empty())
        {
            LogError(LogRenderer, "UniformManager not initialized or descriptor pool empty");
            return false;
        }

        // 步骤3: [FIX] 直接使用Slot作为Descriptor索引（直接映射）
        // 原逻辑：descriptorIndex = slotId - ROOT_DESCRIPTOR_TABLE_CUSTOM（导致Slot 88 → Index 73）
        // 新逻辑：descriptorIndex = slotId（直接映射，Slot 88 → Index 88）
        uint32_t descriptorIndex = slotId;

        if (descriptorIndex >= BindlessRootSignature::MAX_CUSTOM_BUFFERS)
        {
            LogError(LogRenderer, "Custom Buffer slot %u exceeds MAX_CUSTOM_BUFFERS (%u)",
                     slotId, BindlessRootSignature::MAX_CUSTOM_BUFFERS);
            return false;
        }

        // 步骤4: 从预分配池中获取Descriptor（池索引 = Slot编号）
        auto allocation = m_customBufferDescriptorPool[descriptorIndex];

        if (!allocation.isValid)
        {
            LogError(LogRenderer, "Failed to get pre-allocated descriptor for slot %u (index %u)",
                     slotId, descriptorIndex);
            return false;
        }

        // 步骤5: 创建CustomBufferDescriptor并存储
        CustomBufferDescriptor desc;
        desc.allocation = allocation;
        desc.slotId     = slotId;
        desc.isValid    = true;

        m_customBufferDescriptors[slotId] = desc;

        LogInfo(LogRenderer, "Allocated Custom Buffer descriptor: Slot=%u → Descriptor Index=%u (direct mapping)",
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
            LogInfo(LogRenderer, "Custom Buffer descriptor freed: Slot=%u", slotId);
        }
    }

    // ========================================================================
    // [NEW] GetCustomBufferDescriptorTableGPUHandle实现
    // ========================================================================
    D3D12_GPU_DESCRIPTOR_HANDLE UniformManager::GetCustomBufferDescriptorTableGPUHandle() const
    {
        return m_customBufferDescriptorTableBaseGPUHandle;
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
            LogError(LogRenderer, "Slot %u is not an engine reserved slot", slotId);
            return 0;
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
