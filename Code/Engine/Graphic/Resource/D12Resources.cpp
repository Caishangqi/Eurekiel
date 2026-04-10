#include "D12Resources.hpp"
#include <cassert>

#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Graphic/Resource/BindlessIndexAllocator.hpp"
#include "Engine/Graphic/Resource/GlobalDescriptorHeapManager.hpp"
#include "Engine/Graphic/Resource/CommandListManager.hpp"
#include "Engine/Graphic/Resource/UploadContext.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"

namespace enigma::graphic
{
    // ===== D12Resource base implementation =====

    /**
     * Initialize common resource state for derived wrappers.
     */
    D12Resource::D12Resource()
        : m_resource(nullptr)
          , m_currentState(D3D12_RESOURCE_STATE_COMMON)
          , m_debugName()
          , m_size(0)
          , m_isValid(false)
    {
        // Derived classes own actual resource creation.
    }

    /**
     * Virtual destructor keeps derived resource cleanup safe.
     */
    D12Resource::~D12Resource()
    {
        D12Resource::ReleaseResource();
    }

    /**
     * Return the GPU virtual address when the resource supports it.
     */
    D3D12_GPU_VIRTUAL_ADDRESS D12Resource::GetGPUVirtualAddress() const
    {
        if (!m_resource)
        {
            return 0;
        }
        return m_resource->GetGPUVirtualAddress();
    }

    /**
     * Apply a debug label to the native DX12 object.
     */
    void D12Resource::SetDebugName(const std::string& name)
    {
        m_debugName = name;

        if (m_resource && !name.empty())
        {
            size_t   len      = name.length();
            wchar_t* wideName = new wchar_t[len + 1];

            size_t convertedChars = 0;
            mbstowcs_s(&convertedChars, wideName, len + 1, name.c_str(), len);

            m_resource->SetName(wideName);

            delete[] wideName;
        }
    }

    /**
     * Replace the wrapped resource and refresh tracked state.
     */
    void D12Resource::SetResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState, size_t size)
    {
        ReleaseResource();

        m_resource     = resource;
        m_currentState = initialState;
        m_size         = size;
        m_isValid      = (resource != nullptr);

        if (m_resource && !m_debugName.empty())
        {
            SetDebugName(m_debugName);
        }
    }

    /**
     * Release resource implementation.
     * During runtime, multi-frame execution may leave prior queue submissions
     * referencing this resource. Runtime release is deferred behind queue fences,
     * while backend shutdown still owns the global drain path.
     */
    void D12Resource::ReleaseResource()
    {
        if (m_resource)
        {
            if (D3D12RenderSystem::ShouldDeferIndividualResourceRelease())
            {
                D3D12RenderSystem::DeferResourceRelease(m_resource, m_debugName.c_str());
                m_resource = nullptr;
            }
            else
            {
                m_resource->Release();
                m_resource = nullptr;
            }
        }

        m_currentState = D3D12_RESOURCE_STATE_COMMON;
        m_size         = 0;
        m_isValid      = false;
    }

    // ========================================================================
    // Resource State Transition Implementation (Component 6: Shader RT Fetching)
    // ========================================================================

    /**
     * @brief Transition resource to specified state
     *
     * Default implementation uses D3D12RenderSystem::TransitionResource.
     * Subclasses can override for specialized behavior.
     *
     * @param targetState Target resource state
     */
    void D12Resource::TransitionResourceTo(D3D12_RESOURCE_STATES targetState)
    {
        // Skip if already in target state
        if (m_currentState == targetState)
        {
            return;
        }

        // Validate resource
        if (!m_resource)
        {
            LogError(LogRenderer,
                     "TransitionResourceTo: Resource '%s' is null, cannot transition",
                     m_debugName.c_str());
            return;
        }

        // Get current command list from render system
        auto* commandList = D3D12RenderSystem::GetCurrentCommandList();
        if (!commandList)
        {
            LogError(LogRenderer,
                     "TransitionResourceTo: No command list available for '%s'",
                     m_debugName.c_str());
            return;
        }

        // Execute transition using D3D12RenderSystem helper
        D3D12RenderSystem::TransitionResource(
            commandList,
            m_resource,
            m_currentState,
            targetState,
            m_debugName.c_str()
        );

        // Update tracked state
        m_currentState = targetState;
    }

    // ========================================================================
    // CPU data staging helpers (Milestone 2.7)
    // ========================================================================

    /**
     * @brief Cache CPU-side initialization data for a later Upload().
     */
    void D12Resource::SetInitialData(const void* data, size_t dataSize)
    {
        if (!data || dataSize == 0)
        {
            LogDebug(LogRenderer, "SetInitialData: Invalid data (data=%p, size=%zu)", data, dataSize);
            return;
        }

        m_cpuData.resize(dataSize);
        memcpy(m_cpuData.data(), data, dataSize);

        LogDebug(LogRenderer, "SetInitialData: Cached %zu bytes for '%s'", dataSize, m_debugName.c_str());
    }

    /**
     * @brief Execute the selected upload contract for this resource.
     */
    bool D12Resource::Upload(ID3D12GraphicsCommandList* providedCommandList)
    {
        UNUSED(providedCommandList)

        if (RequiresCPUData() && !HasCPUData())
        {
            LogError(LogRenderer,
                     "Upload: No CPU data for '%s'. Call SetInitialData() first.",
                     m_debugName.c_str());
            return false;
        }

        if (!IsValid())
        {
            LogError(LogRenderer, "Upload: Resource '%s' is invalid", m_debugName.c_str());
            return false;
        }

        const UploadContract        uploadContract = GetUploadContract();
        const D3D12_RESOURCE_STATES targetState    = GetUploadDestinationState();
        const char*                 uploadPathName = uploadContract == UploadContract::CopyReadyUpload
                                                         ? "CopyReadyUpload"
                                                         : "GraphicsStatefulUpload";

        if (uploadContract == UploadContract::NoGpuUpload)
        {
            m_isUploaded = true;
            LogDebug(LogRenderer,
                     "Upload: Marked '%s' as uploaded without GPU copy work",
                     m_debugName.c_str());
            return true;
        }

        auto* cmdListManager = D3D12RenderSystem::GetCommandListManager();
        if (!cmdListManager)
        {
            LogError(LogRenderer, "Upload: CommandListManager not available");
            return false;
        }

        auto* device = D3D12RenderSystem::GetDevice();
        if (!device)
        {
            LogError(LogRenderer, "Upload: Device not available");
            return false;
        }

        UploadContext uploadContext(device, GetCPUDataSize());
        if (!uploadContext.IsValid())
        {
            LogError(LogRenderer, "Upload: Failed to create UploadContext for '%s'", m_debugName.c_str());
            return false;
        }

        QueueRouteContext routeContext = {};
        routeContext.workload = uploadContract == UploadContract::CopyReadyUpload
                                    ? QueueWorkloadClass::CopyReadyUpload
                                    : QueueWorkloadClass::GraphicsStatefulUpload;
        routeContext.isCopySafe            = uploadContract == UploadContract::CopyReadyUpload;
        routeContext.allowGraphicsFallback = true;

        const QueueRouteDecision routeDecision = D3D12RenderSystem::ResolveQueueRoute(routeContext);

        auto acquireCommandList = [&](CommandQueueType queueType, const char* debugName) -> ID3D12GraphicsCommandList*
        {
            return cmdListManager->AcquireCommandList(queueType, debugName);
        };

        auto submitAndTrack = [&](ID3D12GraphicsCommandList* commandList) -> QueueSubmissionToken
        {
            QueueSubmissionToken submissionToken = cmdListManager->SubmitCommandList(commandList);
            if (submissionToken.IsValid())
            {
                D3D12RenderSystem::RecordQueueSubmission(submissionToken, routeContext.workload);
            }

            return submissionToken;
        };

        auto waitAndRecycle = [&](const QueueSubmissionToken& token) -> bool
        {
            if (!cmdListManager->WaitForSubmission(token))
            {
                return false;
            }

            cmdListManager->UpdateCompletedCommandLists();
            return true;
        };

        auto transitionToCopyDest = [&](ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES sourceState)
        {
            if (sourceState == D3D12_RESOURCE_STATE_COPY_DEST)
            {
                return;
            }

            D3D12RenderSystem::TransitionResource(commandList,
                                                  m_resource,
                                                  sourceState,
                                                  D3D12_RESOURCE_STATE_COPY_DEST,
                                                  m_debugName.c_str());
        };

        auto transitionToTargetState = [&](ID3D12GraphicsCommandList* commandList)
        {
            if (targetState == D3D12_RESOURCE_STATE_COPY_DEST)
            {
                return;
            }

            D3D12RenderSystem::TransitionResource(commandList,
                                                  m_resource,
                                                  D3D12_RESOURCE_STATE_COPY_DEST,
                                                  targetState,
                                                  m_debugName.c_str());
        };

        auto completeUpload = [&]() -> bool
        {
            m_currentState = targetState;
            m_isUploaded   = true;
            LogDebug(LogRenderer,
                     "Upload: Completed '%s' via %s",
                     m_debugName.c_str(),
                     uploadPathName);
            return true;
        };

        if (routeDecision.activeQueue == CommandQueueType::Copy)
        {
            auto* copyCommandList = acquireCommandList(CommandQueueType::Copy, "CopyReadyUpload");
            if (!copyCommandList)
            {
                LogError(LogRenderer,
                         "Upload: Failed to acquire Copy command list for '%s'",
                         m_debugName.c_str());
                return false;
            }

            transitionToCopyDest(copyCommandList, m_currentState);
            if (!UploadToGPU(copyCommandList, uploadContext))
            {
                LogError(LogRenderer, "Upload: UploadToGPU failed for '%s'", m_debugName.c_str());
                return false;
            }

            QueueSubmissionToken copyToken = submitAndTrack(copyCommandList);
            if (!copyToken.IsValid())
            {
                LogError(LogRenderer,
                         "Upload: Failed to submit copy upload for '%s'",
                         m_debugName.c_str());
                return false;
            }

            if (cmdListManager->InsertQueueWait(CommandQueueType::Graphics, copyToken))
            {
                D3D12RenderSystem::RecordQueueWaitInsertion(copyToken.queueType, CommandQueueType::Graphics);
            }
            else
            {
                core::LogWarn(LogRenderer,
                              "Upload: Falling back to CPU wait for copy handoff of '%s'",
                              m_debugName.c_str());

                if (!waitAndRecycle(copyToken))
                {
                    return false;
                }
            }

            auto* graphicsCommandList = acquireCommandList(CommandQueueType::Graphics, "CopyReadyUploadFinalize");
            if (!graphicsCommandList)
            {
                LogError(LogRenderer,
                         "Upload: Failed to acquire Graphics finalize command list for '%s'",
                         m_debugName.c_str());
                return false;
            }

            transitionToTargetState(graphicsCommandList);

            QueueSubmissionToken finalizeToken = submitAndTrack(graphicsCommandList);
            if (!finalizeToken.IsValid())
            {
                LogError(LogRenderer,
                         "Upload: Failed to submit graphics finalize upload for '%s'",
                         m_debugName.c_str());
                return false;
            }

            if (!waitAndRecycle(finalizeToken))
            {
                return false;
            }

            return completeUpload();
        }

        auto* commandList = acquireCommandList(routeDecision.activeQueue, uploadPathName);
        if (!commandList)
        {
            LogError(LogRenderer,
                     "Upload: Failed to acquire routed command list for '%s'",
                     m_debugName.c_str());
            return false;
        }

        transitionToCopyDest(commandList, m_currentState);
        if (!UploadToGPU(commandList, uploadContext))
        {
            LogError(LogRenderer, "Upload: UploadToGPU failed for '%s'", m_debugName.c_str());
            return false;
        }

        transitionToTargetState(commandList);

        QueueSubmissionToken submissionToken = submitAndTrack(commandList);
        if (!submissionToken.IsValid())
        {
            LogError(LogRenderer, "Upload: SubmitCommandList failed for '%s'", m_debugName.c_str());
            return false;
        }

        if (!waitAndRecycle(submissionToken))
        {
            return false;
        }

        return completeUpload();
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
            core::LogWarn(LogRenderer,    "RegisterBindless: Resource '%s' is already registered (index=%u)", m_debugName.c_str(), m_bindlessIndex);
            return std::nullopt;
        }

        // 2. 安全检查: 必须先上传到GPU
        if (!IsUploaded())
        {
            LogError(LogRenderer,
                     "RegisterBindless: SAFETY VIOLATION - Resource '%s' not uploaded yet!\n"
                     "  True Bindless Flow: Create() → Upload() → RegisterBindless()\n"
                     "  Please call Upload() before RegisterBindless()",
                     m_debugName.c_str());
            return std::nullopt;
        }

        // 3. 检查资源有效性
        if (!IsValid())
        {
            LogError(LogRenderer,
                     "RegisterBindless: Resource '%s' is invalid",
                     m_debugName.c_str());
            return std::nullopt;
        }

        // 4. 获取Bindless系统组件
        auto* indexAllocator = D3D12RenderSystem::GetBindlessIndexAllocator();
        auto* heapManager    = D3D12RenderSystem::GetGlobalDescriptorHeapManager();
        auto* device         = D3D12RenderSystem::GetDevice();

        if (!indexAllocator || !heapManager || !device)
        {
            LogError(LogRenderer,
                     "RegisterBindless: Bindless system not initialized (allocator=%p, heap=%p, device=%p)",
                     indexAllocator, heapManager, device);
            return std::nullopt;
        }

        // 5. 调用子类实现的索引分配逻辑 (Template Method模式)
        uint32_t index = AllocateBindlessIndexInternal(indexAllocator);

        // 6. 检查索引分配是否成功
        if (index == INVALID_BINDLESS_INDEX)
        {
            LogError(LogRenderer,
                     "RegisterBindless: Failed to allocate index for '%s'",
                     m_debugName.c_str());
            return std::nullopt;
        }

        // 7. 存储索引
        SetBindlessIndex(index);

        // 8. 在全局描述符堆中创建描述符(子类实现)
        CreateDescriptorInGlobalHeap(device, heapManager);

        LogDebug(LogRenderer,
                 "RegisterBindless: Resource '%s' registered (index=%u)",
                 m_debugName.c_str(), index);

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
            LogError(LogRenderer,
                     "UnregisterBindless: BindlessIndexAllocator not available");
            return false;
        }

        // 3. 调用子类实现的索引释放逻辑 (Template Method模式)
        bool success = FreeBindlessIndexInternal(indexAllocator, m_bindlessIndex);

        // 4. 清除索引标记
        if (success)
        {
            LogDebug(LogRenderer,
                     "UnregisterBindless: Resource '%s' unregistered (index=%u)",
                     m_debugName.c_str(), m_bindlessIndex);
            ClearBindlessIndex();
        }
        else
        {
            LogError(LogRenderer,
                     "UnregisterBindless: Failed to free index %u for '%s'",
                     m_bindlessIndex, m_debugName.c_str());
        }

        return success;
    }
} // namespace enigma::graphic
