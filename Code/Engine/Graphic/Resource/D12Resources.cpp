#include "D12Resources.hpp"
#include <cassert>
#include "Engine/Graphic/Resource/BindlessIndexAllocator.hpp"
#include "Engine/Graphic/Resource/GlobalDescriptorHeapManager.hpp"
#include "Engine/Graphic/Resource/CommandListManager.hpp"
#include "Engine/Graphic/Resource/UploadContext.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"

namespace enigma::graphic
{
    // ===== D12Resource基类实现 =====

    /**
     * 构造函数实现
     * 教学要点：基类构造函数初始化通用属性
     */
    D12Resource::D12Resource()
        : m_resource(nullptr)
          , m_currentState(D3D12_RESOURCE_STATE_COMMON)
          , m_debugName()
          , m_size(0)
          , m_isValid(false)
    {
        // 基类构造函数只进行基本初始化
        // 实际资源创建由派生类负责
    }

    /**
     * 虚析构函数实现
     * 教学要点：虚析构函数确保派生类正确释放资源
     */
    D12Resource::~D12Resource()
    {
        // 释放DirectX 12资源
        D12Resource::ReleaseResource();
    }

    /**
     * 获取GPU虚拟地址实现
     * DirectX 12 API: ID3D12Resource::GetGPUVirtualAddress()
     * 教学要点：GPU虚拟地址是DX12 Bindless资源绑定的关键
     */
    D3D12_GPU_VIRTUAL_ADDRESS D12Resource::GetGPUVirtualAddress() const
    {
        if (!m_resource)
        {
            return 0;
        }
        // DirectX 12 API: ID3D12Resource::GetGPUVirtualAddress()
        // 返回资源在GPU虚拟地址空间中的地址
        // 用于Bindless描述符绑定和Shader Resource View
        return m_resource->GetGPUVirtualAddress();
    }

    /**
     * 设置调试名称实现
     * DirectX 12 API: ID3D12Object::SetName()
     * 教学要点：调试名称对PIX调试和性能分析非常重要
     */
    void D12Resource::SetDebugName(const std::string& name)
    {
        m_debugName = name;

        if (m_resource && !name.empty())
        {
            // 转换为宽字符串
            size_t   len      = name.length();
            wchar_t* wideName = new wchar_t[len + 1];

            size_t convertedChars = 0;
            mbstowcs_s(&convertedChars, wideName, len + 1, name.c_str(), len);

            // DirectX 12 API: ID3D12Object::SetName()
            // 设置DirectX对象的调试名称，便于PIX调试
            m_resource->SetName(wideName);

            delete[] wideName;
        }
    }

    /**
     * 设置资源指针（由派生类调用）
     * 教学要点：统一的资源设置接口，确保状态一致性
     */
    void D12Resource::SetResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState, size_t size)
    {
        // 释放之前的资源
        ReleaseResource();

        // 设置新资源
        m_resource     = resource;
        m_currentState = initialState;
        m_size         = size;
        m_isValid      = (resource != nullptr);

        // 如果有调试名称，应用到新资源
        if (m_resource && !m_debugName.empty())
        {
            SetDebugName(m_debugName);
        }
    }

    /**
     * 释放资源实现
     * 教学要点：安全释放DX12资源，避免重复释放
     */
    void D12Resource::ReleaseResource()
    {
        if (m_resource)
        {
            // DirectX 12资源使用COM接口
            // Release()会减少引用计数，当计数为0时自动销毁
            m_resource->Release();
            m_resource = nullptr;
        }

        // 重置状态
        m_currentState = D3D12_RESOURCE_STATE_COMMON;
        m_size         = 0;
        m_isValid      = false;
    }

    // ========================================================================
    // CPU数据管理实现 (Milestone 2.7)
    // ========================================================================

    /**
     * @brief 设置初始数据(CPU端)
     *
     * 教学要点: 复制数据到m_cpuData，等待Upload()调用
     */
    void D12Resource::SetInitialData(const void* data, size_t dataSize)
    {
        if (!data || dataSize == 0)
        {
            core::LogDebug(RendererSubsystem::GetStaticSubsystemName(),
                           "SetInitialData: Invalid data (data=%p, size=%zu)",
                           data, dataSize);
            return;
        }

        // 复制数据到CPU缓存
        m_cpuData.resize(dataSize);
        memcpy(m_cpuData.data(), data, dataSize);

        core::LogDebug(RendererSubsystem::GetStaticSubsystemName(),
                       "SetInitialData: Cached %zu bytes for '%s'",
                       dataSize, m_debugName);
    }

    /**
     * @brief 上传资源到GPU
     *
     * 教学要点:
     * 1. Template Method模式: 基类管理流程，子类实现细节
     * 2. 使用Copy Command List专用队列
     * 3. 同步等待上传完成
     * 4. 虚函数: 复合资源可重写来管理子资源上传
     */
    bool D12Resource::Upload(ID3D12GraphicsCommandList* providedCommandList)
    {
        UNUSED(providedCommandList) // 基类实现不使用外部命令列表，获取自己的

        // 1. 检查是否有CPU数据（仅对需要CPU数据的资源）
        // Milestone 3.0 Bug Fix: RenderTarget/DepthStencil不需要CPU数据
        if (RequiresCPUData() && !HasCPUData())
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "Upload: No CPU data for '%s'. Call SetInitialData() first.",
                           m_debugName);
            return false;
        }

        // 2. 检查资源有效性
        if (!IsValid())
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "Upload: Resource '%s' is invalid",
                           m_debugName);
            return false;
        }

        // 2.5. Milestone 3.0 Bug Fix: 对于不需要CPU数据的资源（如RenderTarget），直接标记为已上传
        // 教学要点:
        // - RenderTarget/DepthStencil是GPU输出纹理，没有CPU数据
        // - 它们的m_cpuData.size() == 0，导致UploadContext创建失败
        // - 但仍需调用Upload()来设置m_isUploaded=true，通过RegisterBindless()的安全检查
        // - 这里跳过实际的上传流程，直接标记完成
        if (!RequiresCPUData())
        {
            m_isUploaded = true;
            core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                          "Upload: Successfully marked '%s' as uploaded (no CPU data required)",
                          m_debugName);
            return true;
        }

        // 3. 获取CommandListManager
        auto* cmdListManager = D3D12RenderSystem::GetCommandListManager();
        if (!cmdListManager)
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "Upload: CommandListManager not available");
            return false;
        }

        // 4. 创建UploadContext (Upload Heap)
        auto* device = D3D12RenderSystem::GetDevice();
        if (!device)
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "Upload: Device not available");
            return false;
        }

        UploadContext uploadContext(device, GetCPUDataSize());
        if (!uploadContext.IsValid())
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "Upload: Failed to create UploadContext for '%s'",
                           m_debugName);
            return false;
        }

        // 5. 获取Graphics Command List（而非Copy）
        // ⭐ Milestone 2.8 修复: 使用Graphics Command List上传资源
        //
        // 教学要点 - 为什么使用Graphics而非Copy？
        // 1. Copy Command List只支持有限的资源状态: COMMON, COPY_SOURCE, COPY_DEST
        // 2. Uniform Buffer使用MemoryAccess::CPUToGPU创建，初始状态是GENERIC_READ (0x2C3)
        // 3. GENERIC_READ包含PIXEL_SHADER_RESOURCE等Graphics专属标志
        // 4. 在Copy Command List上转换GENERIC_READ ↔ COPY_DEST会导致DirectX错误:
        //    "D3D12 ERROR: D3D12_RESOURCE_STATES has invalid flags (0x2c3) for copy command list"
        // 5. Graphics Command List支持所有资源状态转换，包括GENERIC_READ
        //
        // Microsoft最佳实践:
        // - Graphics Queue: 复杂操作、小数据传输、需要Graphics状态的资源
        // - Copy Queue: 大量简单拷贝、纹理/大缓冲区上传、异步后台传输
        //
        // Uniform Buffer特性:
        // - 数量少(11个)、大小小(几百字节)、上传频率低(初始化时)
        // - 需要GENERIC_READ状态(用于Vertex/Pixel/Compute Shader读取)
        // - 适合使用Graphics Queue而非Copy Queue
        //
        // 🔍 DEBUG: 在获取命令列表前检查可用数量
        uint32_t availableBefore = cmdListManager->GetAvailableCount(CommandListManager::Type::Graphics);
        uint32_t executingBefore = cmdListManager->GetExecutingCount(CommandListManager::Type::Graphics);
        core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                      "Upload[%s]: BEFORE Acquire - Available=%u, Executing=%u",
                      m_debugName, availableBefore, executingBefore);

        auto* commandList = cmdListManager->AcquireCommandList(CommandListManager::Type::Graphics, "ResourceUpload");
        if (!commandList)
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "Upload: Failed to acquire Graphics command list for '%s'",
                           m_debugName);
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "  Available Graphics Lists: %u, Executing: %u",
                           availableBefore, executingBefore);
            return false;
        }

        // 6. 资源状态转换: 当前状态 → COPY_DEST
        D3D12RenderSystem::TransitionResource(commandList, m_resource, m_currentState, D3D12_RESOURCE_STATE_COPY_DEST, m_debugName.c_str());

        // 7. 调用子类实现的上传逻辑
        bool uploadSuccess = UploadToGPU(commandList, uploadContext);
        if (!uploadSuccess)
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "Upload: UploadToGPU failed for '%s'",
                           m_debugName);
            return false;
        }

        // 8. 资源状态转换: COPY_DEST → 目标状态
        D3D12_RESOURCE_STATES targetState = GetUploadDestinationState();

        D3D12RenderSystem::TransitionResource(commandList, m_resource,
                                              D3D12_RESOURCE_STATE_COPY_DEST, targetState,
                                              m_debugName.c_str());

        // 9. 执行命令列表并同步等待完成
        uint64_t fenceValue = cmdListManager->ExecuteCommandList(commandList);
        cmdListManager->WaitForFence(fenceValue);

        // 9.5. 立即回收已完成的命令列表 - 确保资源池化正确运作
        // 教学要点: WaitForFence() 只等待GPU完成，不会自动回收命令列表
        // UpdateCompletedCommandLists() 检查围栏值，将完成的命令列表放回可用队列
        // 这是 DirectX 12 命令列表池化的正确实践

        // 🔍 DEBUG: 在回收前检查状态
        uint32_t availableBeforeRecycle = cmdListManager->GetAvailableCount(CommandListManager::Type::Graphics);
        uint32_t executingBeforeRecycle = cmdListManager->GetExecutingCount(CommandListManager::Type::Graphics);
        core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                      "Upload[%s]: BEFORE Recycle - Available=%u, Executing=%u",
                      m_debugName, availableBeforeRecycle, executingBeforeRecycle);

        cmdListManager->UpdateCompletedCommandLists();

        // 🔍 DEBUG: 在回收后检查状态
        uint32_t availableAfterRecycle = cmdListManager->GetAvailableCount(CommandListManager::Type::Graphics);
        uint32_t executingAfterRecycle = cmdListManager->GetExecutingCount(CommandListManager::Type::Graphics);
        core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                      "Upload[%s]: AFTER Recycle - Available=%u, Executing=%u (Recycled=%u)",
                      m_debugName, availableAfterRecycle, executingAfterRecycle,
                      availableAfterRecycle - availableBeforeRecycle);

        // 10. 更新资源状态和上传标记
        m_currentState = targetState;
        m_isUploaded   = true;

        core::LogDebug(RendererSubsystem::GetStaticSubsystemName(),
                       "Upload: Successfully uploaded '%s' (%zu bytes)",
                       m_debugName, GetCPUDataSize());

        return true;
    }

    // ========================================================================
    // SM6.6 Bindless资源注册接口实现 (Milestone 2.7)
    // ========================================================================

    /**
     * @brief 注册到Bindless系统
     *
     * 教学要点:
     * 1. SM6.6 Bindless流程: Create → Upload → RegisterBindless
     * 2. 安全检查: 必须先Upload才能Register
     * 3. 三步注册: 分配索引 → 存储索引 → 创建描述符
     * 4. 架构分离: BindlessIndexAllocator(索引) + GlobalDescriptorHeapManager(描述符)
     *
     * @return 成功返回Bindless全局索引，失败返回nullopt
     */
    std::optional<uint32_t> D12Resource::RegisterBindless()
    {
        // 1. 防止重复注册
        if (IsBindlessRegistered())
        {
            core::LogWarn(RendererSubsystem::GetStaticSubsystemName(),
                          "RegisterBindless: Resource '%s' is already registered (index=%u)",
                          m_debugName, m_bindlessIndex);
            return std::nullopt;
        }

        // 2. 安全检查: 必须先上传到GPU
        if (!IsUploaded())
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "RegisterBindless: SAFETY VIOLATION - Resource '%s' not uploaded yet!\n"
                           "  True Bindless Flow: Create() → Upload() → RegisterBindless()\n"
                           "  Please call Upload() before RegisterBindless()",
                           m_debugName);
            return std::nullopt;
        }

        // 3. 检查资源有效性
        if (!IsValid())
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "RegisterBindless: Resource '%s' is invalid",
                           m_debugName);
            return std::nullopt;
        }

        // 4. 获取Bindless系统组件
        auto* indexAllocator = D3D12RenderSystem::GetBindlessIndexAllocator();
        auto* heapManager    = D3D12RenderSystem::GetGlobalDescriptorHeapManager();
        auto* device         = D3D12RenderSystem::GetDevice();

        if (!indexAllocator || !heapManager || !device)
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "RegisterBindless: Bindless system not initialized (allocator=%p, heap=%p, device=%p)",
                           indexAllocator, heapManager, device);
            return std::nullopt;
        }

        // 5. 调用子类实现的索引分配逻辑 (Template Method模式)
        uint32_t index = AllocateBindlessIndexInternal(indexAllocator);

        // 6. 检查索引分配是否成功
        if (index == INVALID_BINDLESS_INDEX)
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "RegisterBindless: Failed to allocate index for '%s'",
                           m_debugName);
            return std::nullopt;
        }

        // 7. 存储索引
        SetBindlessIndex(index);

        // 8. 在全局描述符堆中创建描述符(子类实现)
        CreateDescriptorInGlobalHeap(device, heapManager);

        core::LogDebug(RendererSubsystem::GetStaticSubsystemName(),
                       "RegisterBindless: Resource '%s' registered (index=%u)",
                       m_debugName, index);

        return index;
    }

    /**
     * @brief 从Bindless系统注销
     *
     * 教学要点:
     * 1. RAII设计: 确保资源正确注销，避免索引泄漏
     * 2. 架构分离: 只释放索引，描述符堆自动管理
     * 3. 幂等操作: 未注册时调用也是安全的
     *
     * @return 成功返回true，失败返回false
     */
    bool D12Resource::UnregisterBindless()
    {
        // 1. 检查是否已注册
        if (!IsBindlessRegistered())
        {
            return false; // 未注册，无需注销
        }

        // 2. 获取BindlessIndexAllocator
        auto* indexAllocator = D3D12RenderSystem::GetBindlessIndexAllocator();
        if (!indexAllocator)
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "UnregisterBindless: BindlessIndexAllocator not available");
            return false;
        }

        // 3. 调用子类实现的索引释放逻辑 (Template Method模式)
        bool success = FreeBindlessIndexInternal(indexAllocator, m_bindlessIndex);

        // 4. 清除索引标记
        if (success)
        {
            core::LogDebug(RendererSubsystem::GetStaticSubsystemName(),
                           "UnregisterBindless: Resource '%s' unregistered (index=%u)",
                           m_debugName, m_bindlessIndex);
            ClearBindlessIndex();
        }
        else
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "UnregisterBindless: Failed to free index %u for '%s'",
                           m_bindlessIndex, m_debugName);
        }

        return success;
    }
} // namespace enigma::graphic
