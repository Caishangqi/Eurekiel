#include "D3D12RenderSystem.hpp"
#include <cassert>
#include <algorithm>
#include <array>
#include <vector>

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Integration/RendererEvents.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Target/D12DepthTexture.hpp"
#include "Engine/Graphic/Target/RTTypes.hpp"
#include "Engine/Graphic/Resource/BindlessIndexAllocator.hpp"
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Engine/Resource/Atlas/ImageResource.hpp"
#include "Engine/Core/Image.hpp"

#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Graphic/Resource/Buffer/D12VertexBuffer.hpp"
#include "Engine/Graphic/Resource/Buffer/D12IndexBuffer.hpp"
#include "Engine/Graphic/Mipmap/MipmapConfig.hpp"
#include "Engine/Graphic/Resource/CommandQueueException.hpp"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"dxguid.lib")

namespace enigma::graphic
{
    namespace
    {
        D3D12_COMMAND_LIST_TYPE ToNativeCommandListType(CommandQueueType queueType)
        {
            switch (queueType)
            {
            case CommandQueueType::Graphics:
                return D3D12_COMMAND_LIST_TYPE_DIRECT;
            case CommandQueueType::Compute:
                return D3D12_COMMAND_LIST_TYPE_COMPUTE;
            case CommandQueueType::Copy:
                return D3D12_COMMAND_LIST_TYPE_COPY;
            }

            return D3D12_COMMAND_LIST_TYPE_DIRECT;
        }

        const char* GetQueueTypeName(CommandQueueType queueType)
        {
            switch (queueType)
            {
            case CommandQueueType::Graphics:
                return "Graphics";
            case CommandQueueType::Compute:
                return "Compute";
            case CommandQueueType::Copy:
                return "Copy";
            }

            return "Unknown";
        }

        const char* GetQueueWorkloadName(QueueWorkloadClass workload)
        {
            switch (workload)
            {
            case QueueWorkloadClass::Unknown:
                return "Unknown";
            case QueueWorkloadClass::FrameGraphics:
                return "FrameGraphics";
            case QueueWorkloadClass::ImmediateGraphics:
                return "ImmediateGraphics";
            case QueueWorkloadClass::Present:
                return "Present";
            case QueueWorkloadClass::ImGui:
                return "ImGui";
            case QueueWorkloadClass::GraphicsStatefulUpload:
                return "GraphicsStatefulUpload";
            case QueueWorkloadClass::CopyReadyUpload:
                return "CopyReadyUpload";
            case QueueWorkloadClass::MipmapGeneration:
                return "MipmapGeneration";
            case QueueWorkloadClass::ChunkGeometryUpload:
                return "ChunkGeometryUpload";
            case QueueWorkloadClass::ChunkArenaRelocation:
                return "ChunkArenaRelocation";
            }

            return "Unknown";
        }

        const char* GetQueueFallbackReasonName(QueueFallbackReason reason)
        {
            switch (reason)
            {
            case QueueFallbackReason::None:
                return "None";
            case QueueFallbackReason::GraphicsOnlyMode:
                return "GraphicsOnlyMode";
            case QueueFallbackReason::RouteNotValidated:
                return "RouteNotValidated";
            case QueueFallbackReason::UnsupportedWorkload:
                return "UnsupportedWorkload";
            case QueueFallbackReason::RequiresGraphicsStateTransition:
                return "RequiresGraphicsStateTransition";
            case QueueFallbackReason::DedicatedQueueUnavailable:
                return "DedicatedQueueUnavailable";
            case QueueFallbackReason::QueueTypeUnavailable:
                return "QueueTypeUnavailable";
            case QueueFallbackReason::ResourceStateNotSupported:
                return "ResourceStateNotSupported";
            }

            return "Unknown";
        }

        uint32_t GetRequestedFramesInFlightDepthSnapshot()
        {
            if (g_theRendererSubsystem)
            {
                return g_theRendererSubsystem->GetConfiguration().maxFramesInFlight;
            }

            return MAX_FRAMES_IN_FLIGHT;
        }

        uint32_t GetActiveFramesInFlightDepthSnapshot()
        {
            return MAX_FRAMES_IN_FLIGHT;
        }

        const char* GetFrameLifecyclePhaseName(FrameLifecyclePhase phase)
        {
            switch (phase)
            {
            case FrameLifecyclePhase::Idle:
                return "Idle";
            case FrameLifecyclePhase::PreFrameBegin:
                return "PreFrameBegin";
            case FrameLifecyclePhase::RetiringFrameSlot:
                return "RetiringFrameSlot";
            case FrameLifecyclePhase::FrameSlotAcquired:
                return "FrameSlotAcquired";
            case FrameLifecyclePhase::RecordingFrame:
                return "RecordingFrame";
            case FrameLifecyclePhase::SubmittingFrame:
                return "SubmittingFrame";
            }

            return "Unknown";
        }

        bool WaitForQueueDrainOnShutdown(ID3D12Device*       device,
                                         ID3D12CommandQueue* queue,
                                         CommandQueueType    queueType)
        {
            if (!device || !queue)
            {
                return true;
            }

            Microsoft::WRL::ComPtr<ID3D12Fence> shutdownFence;
            HRESULT                             hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&shutdownFence));
            if (FAILED(hr))
            {
                LogWarn(LogRenderer,
                        "Shutdown: Failed to create temporary %s queue drain fence (HRESULT=0x%08X)",
                        GetQueueTypeName(queueType),
                        hr);
                return false;
            }

            constexpr uint64_t kShutdownFenceValue = 1;
            hr                                     = queue->Signal(shutdownFence.Get(), kShutdownFenceValue);
            if (FAILED(hr))
            {
                LogWarn(LogRenderer,
                        "Shutdown: Failed to signal %s queue drain fence (HRESULT=0x%08X)",
                        GetQueueTypeName(queueType),
                        hr);
                return false;
            }

            if (shutdownFence->GetCompletedValue() >= kShutdownFenceValue)
            {
                return true;
            }

            HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (!fenceEvent)
            {
                LogWarn(LogRenderer,
                        "Shutdown: Failed to create wait event for %s queue drain",
                        GetQueueTypeName(queueType));
                return false;
            }

            hr = shutdownFence->SetEventOnCompletion(kShutdownFenceValue, fenceEvent);
            if (FAILED(hr))
            {
                CloseHandle(fenceEvent);
                LogWarn(LogRenderer,
                        "Shutdown: Failed to arm %s queue drain fence event (HRESULT=0x%08X)",
                        GetQueueTypeName(queueType),
                        hr);
                return false;
            }

            const DWORD waitResult = WaitForSingleObject(fenceEvent, INFINITE);
            CloseHandle(fenceEvent);

            if (waitResult != WAIT_OBJECT_0)
            {
                LogWarn(LogRenderer,
                        "Shutdown: Waiting for %s queue drain returned unexpected result (%lu)",
                        GetQueueTypeName(queueType),
                        static_cast<unsigned long>(waitResult));
                return false;
            }

            return true;
        }

        constexpr size_t                                                                       kQueueWorkloadClassCountLocal = enigma::graphic::kQueueWorkloadClassCount;
        constexpr size_t                                                                       kQueueFallbackReasonCount     = 8;
        std::array<std::array<bool, kQueueFallbackReasonCount>, kQueueWorkloadClassCountLocal> g_reportedMixedQueueFallbacks = {};

        bool HasDedicatedQueuesAvailable(const CommandListManager* commandListManager)
        {
            if (!commandListManager)
            {
                return false;
            }

            return commandListManager->GetCommandQueue(CommandQueueType::Compute) != nullptr &&
                commandListManager->GetCommandQueue(CommandQueueType::Copy) != nullptr;
        }

        [[noreturn]] void ReportFatalQueueException(const CommandQueueException& exception)
        {
            ERROR_AND_DIE(Stringf("%s", exception.what()))
        }

        void ReportRecoverableQueueException(const CommandQueueException& exception)
        {
            ERROR_RECOVERABLE(Stringf("%s", exception.what()))
        }

        struct DeferredResourceReleaseEntry
        {
            Microsoft::WRL::ComPtr<ID3D12Resource> resource               = nullptr;
            QueueSubmittedFenceSnapshot            submittedFenceSnapshot = {};
            std::string                            debugName;
        };

        std::mutex                                g_deferredResourceReleaseMutex;
        std::vector<DeferredResourceReleaseEntry> g_deferredResourceReleases;
    }

    // ===== Static member initialization =====
    Microsoft::WRL::ComPtr<ID3D12Device>  D3D12RenderSystem::s_device      = nullptr;
    Microsoft::WRL::ComPtr<IDXGIFactory4> D3D12RenderSystem::s_dxgiFactory = nullptr;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> D3D12RenderSystem::s_adapter     = nullptr;

    // SwapChain state
    Microsoft::WRL::ComPtr<IDXGISwapChain3> D3D12RenderSystem::s_swapChain              = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource>  D3D12RenderSystem::s_swapChainBuffers[3]    = {nullptr};
    D3D12_CPU_DESCRIPTOR_HANDLE             D3D12RenderSystem::s_swapChainRTVs[3]       = {};
    uint32_t                                D3D12RenderSystem::s_currentBackBufferIndex = 0;
    uint32_t                                D3D12RenderSystem::s_swapChainBufferCount   = 3;
    uint32_t                                D3D12RenderSystem::s_swapChainWidth         = 0;
    uint32_t                                D3D12RenderSystem::s_swapChainHeight        = 0;
    DXGI_FORMAT                             D3D12RenderSystem::s_backbufferFormat       = DXGI_FORMAT_R8G8B8A8_UNORM;

    // Command system state
    std::unique_ptr<CommandListManager> D3D12RenderSystem::s_commandListManager         = nullptr;
    ID3D12GraphicsCommandList*          D3D12RenderSystem::s_currentGraphicsCommandList = nullptr;

    // Bindless infrastructure
    std::unique_ptr<BindlessIndexAllocator>      D3D12RenderSystem::s_bindlessIndexAllocator      = nullptr;
    std::unique_ptr<GlobalDescriptorHeapManager> D3D12RenderSystem::s_globalDescriptorHeapManager = nullptr;
    std::unique_ptr<BindlessRootSignature>       D3D12RenderSystem::s_bindlessRootSignature       = nullptr;

    // Texture cache
    std::unordered_map<ResourceLocation, std::weak_ptr<D12Texture>> D3D12RenderSystem::s_textureCache;
    std::mutex                                                      D3D12RenderSystem::s_textureCacheMutex;

    // Fallback textures
    std::shared_ptr<D12Texture> D3D12RenderSystem::s_defaultWhiteTexture  = nullptr;
    std::shared_ptr<D12Texture> D3D12RenderSystem::s_defaultBlackTexture  = nullptr;
    std::shared_ptr<D12Texture> D3D12RenderSystem::s_defaultNormalTexture = nullptr;

    bool D3D12RenderSystem::s_isInitialized  = false;
    bool D3D12RenderSystem::s_isShuttingDown = false;

    // Frame execution state
    FrameContext               D3D12RenderSystem::s_frameContexts[4]               = {};
    uint32_t                   D3D12RenderSystem::s_frameIndex                     = 0;
    uint64_t                   D3D12RenderSystem::s_globalFrameCount               = 0;
    FrameLifecyclePhase        D3D12RenderSystem::s_frameLifecyclePhase            = FrameLifecyclePhase::Idle;
    FrameSlotAcquisitionResult D3D12RenderSystem::s_lastFrameSlotAcquisitionResult = {};
    QueueRoutingPolicy         D3D12RenderSystem::s_queueRoutingPolicy             = {};
    QueueExecutionMode         D3D12RenderSystem::s_requestedQueueExecutionMode    = QueueExecutionMode::MixedQueueExperimental;
    QueueExecutionMode         D3D12RenderSystem::s_activeQueueExecutionMode       = QueueExecutionMode::MixedQueueExperimental;
    QueueExecutionDiagnostics  D3D12RenderSystem::s_queueExecutionDiagnostics      = {};

    // ===== Public API implementation =====

    /**
     * Initialize DirectX 12 rendering system (device, command system, and SwapChain)
     * The IrisRenderSystem.initialize() method corresponding to Iris
     * Note: Now includes automatic SwapChain creation for complete engine-layer initialization
     */
    bool D3D12RenderSystem::Initialize(bool        enableDebugLayer, bool enableGPUValidation,
                                       HWND        hwnd, uint32_t         renderWidth, uint32_t renderHeight,
                                       DXGI_FORMAT backbufferFormat)
    {
        if (s_isInitialized)
        {
            return true; // has been initialized
        }

        // Store configured backbuffer format
        s_backbufferFormat = backbufferFormat;
        s_isShuttingDown   = false;

        // 1. Enable debug layer (if requested)
        if (enableDebugLayer)
        {
            EnableDebugLayer();
        }

        // 2. Create DXGI factories and equipment
        if (!CreateDevice(enableGPUValidation))
        {
            return false;
        }

        // 3. Create and initialize CommandListManager (command command management corresponding to IrisRenderSystem)
        s_commandListManager = std::make_unique<CommandListManager>();
        if (!s_commandListManager->Initialize())
        {
            s_commandListManager.reset();
            return false;
        }

        s_queueExecutionDiagnostics.ClearCounters();
        s_queueExecutionDiagnostics.requestedMode = s_requestedQueueExecutionMode;
        SetActiveQueueExecutionModeInternal(s_requestedQueueExecutionMode);
        g_reportedMixedQueueFallbacks = {};

        // 4. Create per-frame allocators for each queue.
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            FrameContext& frameContext = s_frameContexts[i];

            auto createFrameAllocator = [&](CommandQueueType queueType, Microsoft::WRL::ComPtr<ID3D12CommandAllocator>& allocator) -> bool
            {
                HRESULT hr = s_device->CreateCommandAllocator(
                    ToNativeCommandListType(queueType),
                    IID_PPV_ARGS(&allocator)
                );
                if (FAILED(hr))
                {
                    core::LogError(LogRenderer,
                                   "Failed to create %s allocator for FrameContext[%u], HRESULT=0x%08X",
                                   GetQueueTypeName(queueType), i, hr);
                    return false;
                }

                return true;
            };

            if (!createFrameAllocator(CommandQueueType::Graphics, frameContext.graphicsCommandAllocator) ||
                !createFrameAllocator(CommandQueueType::Compute, frameContext.computeCommandAllocator) ||
                !createFrameAllocator(CommandQueueType::Copy, frameContext.copyCommandAllocator))
            {
                return false;
            }

            frameContext.ClearRetirement();
        }
        s_frameIndex                     = 0;
        s_globalFrameCount               = 0;
        s_frameLifecyclePhase            = FrameLifecyclePhase::Idle;
        s_lastFrameSlotAcquisitionResult = {};
        s_currentGraphicsCommandList     = nullptr;

        core::LogInfo(LogRenderer,
                      "Created %u frame slots with Graphics/Compute/Copy allocators",
                      MAX_FRAMES_IN_FLIGHT);

        // 5. Initialize SM6.6 Bindless components (Milestone 2.7)

        // 5.1 Create the bindless index allocator.
        s_bindlessIndexAllocator = std::make_unique<BindlessIndexAllocator>();
        // The constructor performs all required initialization.

        // 5.2 Create the global descriptor heaps.
        s_globalDescriptorHeapManager = std::make_unique<GlobalDescriptorHeapManager>();
        if (!s_globalDescriptorHeapManager->Initialize())
        {
            core::LogError(LogRenderer,
                           "Failed to initialize GlobalDescriptorHeapManager");
            s_globalDescriptorHeapManager.reset();
            s_bindlessIndexAllocator.reset();
            s_commandListManager.reset();
            return false;
        }

        // 5.3 Create the shared bindless root signature.
        s_bindlessRootSignature = std::make_unique<BindlessRootSignature>();
        if (!s_bindlessRootSignature->Initialize())
        {
            core::LogError(LogRenderer,
                           "Failed to initialize BindlessRootSignature");
            s_bindlessRootSignature.reset();
            s_globalDescriptorHeapManager->Shutdown();
            s_globalDescriptorHeapManager.reset();
            s_bindlessIndexAllocator.reset();
            s_commandListManager.reset();
            return false;
        }

        // 6. Create SwapChain (if window handle is provided)
        if (hwnd)
        {
            if (!CreateSwapChain(hwnd, renderWidth, renderHeight))
            {
                core::LogError(LogRenderer,
                               "Failed to create SwapChain during D3D12RenderSystem initialization");
                // SwapChain creation failure is not fatal - continue initialization
                // This allows headless rendering or manual SwapChain creation later
            }
            else
            {
                core::LogInfo(LogRenderer, "D3D12RenderSystem initialized successfully with SwapChain (%dx%d)", renderWidth, renderHeight);
            }
        }
        else
        {
            core::LogInfo(LogRenderer, "D3D12RenderSystem initialized successfully (no SwapChain - headless mode)");
        }

        s_isInitialized = true;
        return true;
    }

    bool D3D12RenderSystem::PrepareDefaultTextures()
    {
        // Create default fallback textures.
        // RGBA values are stored in little-endian ABGR order.

        // White fallback texture for missing albedo maps.
        uint32_t whitePixel   = 0xFFFFFFFF; // ABGR: A=255, B=255, G=255, R=255
        s_defaultWhiteTexture = std::shared_ptr<D12Texture>(
            CreateTexture2D(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, TextureUsage::ShaderResource,
                            &whitePixel, "DefaultWhiteTexture").release());
        if (!s_defaultWhiteTexture)
        {
            core::LogError(LogRenderer, "Failed to create default white texture");
            ERROR_AND_DIE("Failed to create default white texture")
        }
        else
        {
            core::LogInfo(LogRenderer, "Default white texture created (Bindless index=%u)",
                          s_defaultWhiteTexture->GetBindlessIndex());
        }

        // Black fallback texture for emissive and AO maps.
        uint32_t blackPixel   = 0xFF000000; // ABGR: A=255, B=0, G=0, R=0
        s_defaultBlackTexture = std::shared_ptr<D12Texture>(
            CreateTexture2D(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, TextureUsage::ShaderResource,
                            &blackPixel, "DefaultBlackTexture").release());
        if (!s_defaultBlackTexture)
        {
            core::LogError(LogRenderer, "Failed to create default black texture");
            ERROR_AND_DIE("Failed to create default black texture")
        }
        else
        {
            core::LogInfo(LogRenderer, "Default black texture created (Bindless index=%u)",
                          s_defaultBlackTexture->GetBindlessIndex());
        }

        // Flat normal fallback texture for missing normal maps.
        // RGB(128,128,255) decodes to the +Z normal.
        uint32_t normalPixel   = 0xFFFF8080; // ABGR: A=255, B=255, G=128, R=128
        s_defaultNormalTexture = std::shared_ptr<D12Texture>(
            CreateTexture2D(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, TextureUsage::ShaderResource,
                            &normalPixel, "DefaultNormalTexture").release());
        if (!s_defaultNormalTexture)
        {
            core::LogError(LogRenderer, "Failed to create default normal texture");
            ERROR_AND_DIE("Failed to create default normal texture")
        }
        else
        {
            core::LogInfo(LogRenderer, "Default normal texture created (Bindless index=%u)",
                          s_defaultNormalTexture->GetBindlessIndex());
        }

        return true;
    }

    /**
     * Turn off the rendering system (release all resources)
     * Includes CommandListManager, Bindless systems, SwapChain, Devices and DXGI Objects
     * Complete cleaning logic corresponding to IrisRenderSystem
     */
    void D3D12RenderSystem::Shutdown()
    {
        if (!s_isInitialized)
        {
            return;
        }

        s_isShuttingDown = true;

        DrainQueuesForShutdown();
        ProcessDeferredResourceReleases(true);
        ResetFrameExecutionState();
        ReleaseFallbackTextures();
        ReleaseBindlessInfrastructure();
        ReleaseSwapChainResources();
        ReportLiveObjectsBeforeDeviceRelease();
        ReleaseDeviceObjects();

        s_isInitialized  = false;
        s_isShuttingDown = false;

        core::LogInfo(LogRenderer, "D3D12RenderSystem shutdown completed");
    }

    void D3D12RenderSystem::SetViewport(int width, int height)
    {
        auto           commandList = GetCurrentCommandList();
        D3D12_VIEWPORT viewport    = {};
        viewport.TopLeftX          = 0.0f;
        viewport.TopLeftY          = 0.0f;
        viewport.Width             = static_cast<float>(width);
        viewport.Height            = static_cast<float>(height);
        viewport.MinDepth          = 0.0f;
        viewport.MaxDepth          = 1.0f;
        commandList->RSSetViewports(1, &viewport);

        D3D12_RECT scissorRect = {};
        scissorRect.left       = 0;
        scissorRect.top        = 0;
        scissorRect.right      = static_cast<LONG>(width);
        scissorRect.bottom     = static_cast<LONG>(height);
        commandList->RSSetScissorRects(1, &scissorRect);
    }

    // ===== Buffer creation API implementation =====

    /**
     * Main buffer creation method
     * The IrisRenderSystem.createBuffers() method corresponding to Iris
     *
     * Teaching focus:
     * 1. Parameter verification and error handling
     * 2. Use RAII for resource management
     * 3. The complete process of DirectX 12 resource creation
     */
    std::unique_ptr<D12Buffer> D3D12RenderSystem::CreateBuffer(const BufferCreateInfo& createInfo)
    {
        // Verify that the system has been initialized
        if (!s_isInitialized || !s_device)
        {
            core::LogError(LogRenderer, "D3D12RenderSystem not initialized");
            assert(false && "D3D12RenderSystem not initialized");
            return nullptr;
        }

        // Verify input parameters
        if (createInfo.size == 0)
        {
            assert(false && "Buffer size must be greater than 0");
            return nullptr;
        }

        // 256 byte alignment of the constant buffer
        // DirectX 12 requirements: Constant buffers must be aligned 256 bytes
        BufferCreateInfo alignedCreateInfo = createInfo;
        if (HasFlag(createInfo.usage, BufferUsage::ConstantBuffer))
        {
            alignedCreateInfo.size = AlignConstantBufferSize(createInfo.size);
        }

        // Create a D12Buffer object
        // Use std::make_unique to ensure exception security
        try
        {
            auto buffer = std::make_unique<D12Buffer>(alignedCreateInfo);

            // Verify that the buffer is created successfully
            if (!buffer->IsValid())
            {
                return nullptr;
            }

            return buffer;
        }
        catch (const std::exception&)
        {
            // Log Error
            core::LogError(LogRenderer, "Fail to create D12Buffer");
            assert(false && "Failed to create D12Buffer");
            return nullptr;
        }
    }

    std::unique_ptr<D12VertexBuffer> D3D12RenderSystem::CreateVertexBuffer(const BufferCreateInfo& createInfo)
    {
        if (!s_isInitialized || !s_device)
        {
            core::LogError(LogRenderer, "D3D12RenderSystem not initialized");
            assert(false && "D3D12RenderSystem not initialized");
            return nullptr;
        }

        if (createInfo.size == 0)
        {
            assert(false && "Vertex buffer size must be greater than 0");
            return nullptr;
        }

        if (createInfo.byteStride == 0)
        {
            ERROR_RECOVERABLE("D3D12RenderSystem::CreateVertexBuffer: byteStride must be greater than 0");
            return nullptr;
        }

        if (createInfo.size % createInfo.byteStride != 0)
        {
            ERROR_RECOVERABLE("D3D12RenderSystem::CreateVertexBuffer: size must be a multiple of byteStride");
            return nullptr;
        }

        BufferCreateInfo normalizedCreateInfo = createInfo;
        normalizedCreateInfo.usage            = BufferUsage::VertexBuffer;

        try
        {
            return std::make_unique<D12VertexBuffer>(normalizedCreateInfo);
        }
        catch (const std::exception& e)
        {
            core::LogError(LogRenderer,
                           "Failed to create D12VertexBuffer: %s", e.what());
            return nullptr;
        }
    }

    std::unique_ptr<D12VertexBuffer> D3D12RenderSystem::CreateVertexBuffer(
        size_t size, size_t stride, const void* initialData, const char* debugName)
    {
        BufferCreateInfo createInfo;
        createInfo.size         = size;
        createInfo.usage        = BufferUsage::VertexBuffer;
        createInfo.memoryAccess = MemoryAccess::CPUWritable;
        createInfo.initialData  = initialData;
        createInfo.debugName    = debugName;
        createInfo.byteStride   = stride;
        createInfo.usagePolicy  = BufferUsagePolicy::DynamicUpload;
        return CreateVertexBuffer(createInfo);
    }

    std::unique_ptr<D12IndexBuffer> D3D12RenderSystem::CreateIndexBuffer(const BufferCreateInfo& createInfo)
    {
        if (!s_isInitialized || !s_device)
        {
            core::LogError(LogRenderer, "D3D12RenderSystem not initialized");
            assert(false && "D3D12RenderSystem not initialized");
            return nullptr;
        }

        if (createInfo.size == 0)
        {
            assert(false && "Index buffer size must be greater than 0");
            return nullptr;
        }

        if (createInfo.size % D12IndexBuffer::INDEX_SIZE != 0)
        {
            ERROR_RECOVERABLE("D3D12RenderSystem::CreateIndexBuffer: size must be a multiple of INDEX_SIZE");
            return nullptr;
        }

        BufferCreateInfo normalizedCreateInfo = createInfo;
        normalizedCreateInfo.usage            = BufferUsage::IndexBuffer;

        try
        {
            return std::make_unique<D12IndexBuffer>(normalizedCreateInfo);
        }
        catch (const std::exception& e)
        {
            core::LogError(LogRenderer,
                           "Failed to create D12IndexBuffer: %s", e.what());
            return nullptr;
        }
    }

    std::unique_ptr<D12IndexBuffer> D3D12RenderSystem::CreateIndexBuffer(
        size_t size, const void* initialData, const char* debugName) // [SIMPLIFIED] Always Uint32
    {
        BufferCreateInfo createInfo;
        createInfo.size         = size;
        createInfo.usage        = BufferUsage::IndexBuffer;
        createInfo.memoryAccess = MemoryAccess::CPUWritable;
        createInfo.initialData  = initialData;
        createInfo.debugName    = debugName;
        createInfo.usagePolicy  = BufferUsagePolicy::DynamicUpload;
        return CreateIndexBuffer(createInfo);
    }

    /**
     * @brief 绑定VertexBuffer（重载：接受D12VertexBuffer*）
     *
     * 教学要点:
     * 1. 便捷接口，自动调用GetView()
     * 2. 空指针检查，避免崩溃
     * 3. 委托给现有的BindVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW&)实现
     */
    void D3D12RenderSystem::BindVertexBuffer(const D12VertexBuffer* vertexBuffer, UINT slot)
    {
        if (!vertexBuffer)
        {
            core::LogError(LogRenderer,
                           "BindVertexBuffer: VertexBuffer is nullptr");
            return;
        }

        if (!vertexBuffer->IsValid())
        {
            core::LogError(LogRenderer,
                           "BindVertexBuffer: VertexBuffer is invalid");
            return;
        }

        // 委托给现有的BindVertexBuffer(view, slot)实现
        BindVertexBuffer(vertexBuffer->GetView(), slot);
    }

    /**
     * @brief 绑定IndexBuffer（重载：接受D12IndexBuffer*）
     *
     * 教学要点:
     * 1. 便捷接口，自动调用GetView()
     * 2. 空指针检查，避免崩溃
     * 3. 委托给现有的BindIndexBuffer(const D3D12_INDEX_BUFFER_VIEW&)实现
     */
    void D3D12RenderSystem::BindIndexBuffer(const D12IndexBuffer* indexBuffer)
    {
        if (!indexBuffer)
        {
            core::LogError(LogRenderer,
                           "BindIndexBuffer: IndexBuffer is nullptr");
            return;
        }

        if (!indexBuffer->IsValid())
        {
            core::LogError(LogRenderer,
                           "BindIndexBuffer: IndexBuffer is invalid");
            return;
        }

        // 委托给现有的BindIndexBuffer(view)实现
        BindIndexBuffer(indexBuffer->GetView());
    }

    /**
      * Simplified constant buffer creation
      * DirectX 12 requirements: Constant buffers must be aligned 256 bytes
      */
    std::unique_ptr<D12Buffer> D3D12RenderSystem::CreateConstantBuffer(
        size_t      size,
        const void* initialData,
        const char* debugName)
    {
        BufferCreateInfo createInfo;
        createInfo.size         = size; // AlignConstantBufferSize会在CreateBuffer中调用
        createInfo.usage        = BufferUsage::ConstantBuffer;
        createInfo.memoryAccess = MemoryAccess::CPUWritable; // Constant buffers usually need to be updated frequently
        createInfo.initialData  = initialData;
        createInfo.debugName    = debugName;

        return CreateBuffer(createInfo);
    }

    /**
     * Create a structured buffer (corresponding to Iris' SSBO)
     * Used to store a large amount of structured data, such as particle system, instance data, etc.
     */
    std::unique_ptr<D12Buffer> D3D12RenderSystem::CreateStructuredBuffer(size_t elementCount, size_t elementSize, const void* initialData, const char* debugName)
    {
        BufferCreateInfo createInfo;
        createInfo.size         = elementCount * elementSize;
        createInfo.usage        = BufferUsage::StructuredBuffer;
        createInfo.memoryAccess = initialData ? MemoryAccess::CPUToGPU : MemoryAccess::GPUOnly;
        createInfo.initialData  = initialData;
        createInfo.debugName    = debugName;
        createInfo.byteStride   = elementSize;

        return CreateBuffer(createInfo);
    }

    // ===== Texture creation API implementation =====

    /**
     * 主要纹理创建方法
     * 对应Iris的IrisRenderSystem.createTexture()，支持Bindless纹理架构
     *
     * 教学重点:
     * 1. 参数验证和错误处理
     * 2. 使用RAII进行资源管理
     * 3. DirectX 12纹理创建的完整流程
     *
     * DirectX 12 API调用链：
     * 1. ID3D12Device::CreateCommittedResource() - 创建纹理资源
     * 2. ID3D12Device::CreateShaderResourceView() - 创建SRV
     * 3. ID3D12Device::CreateUnorderedAccessView() - 创建UAV (如果需要)
     * 4. ID3D12Resource::SetName() - 设置调试名称
     */
    std::unique_ptr<D12Texture> D3D12RenderSystem::CreateTexture(TextureCreateInfo& createInfo)
    {
        // 验证系统已初始化
        if (!s_isInitialized || !s_device)
        {
            core::LogError(LogRenderer, "D3D12RenderSystem not initialized");
            assert(false && "D3D12RenderSystem not initialized");
            return nullptr;
        }

        // 验证输入参数
        if (createInfo.width == 0 || createInfo.height == 0)
        {
            assert(false && "Texture dimensions must be greater than 0");
            return nullptr;
        }

        // 验证纹理格式
        if (createInfo.format == DXGI_FORMAT_UNKNOWN)
        {
            assert(false && "Texture format cannot be UNKNOWN");
            return nullptr;
        }

        // 验证使用标志
        if (createInfo.usage == static_cast<TextureUsage>(0))
        {
            assert(false && "Texture usage must be specified");
            return nullptr;
        }


        // 创建D12Texture对象
        // 使用std::make_unique确保异常安全
        try
        {
            auto texture = std::make_unique<D12Texture>(createInfo);

            // 验证纹理创建成功
            if (!texture->IsValid())
            {
                core::LogError(LogRenderer, "Failed to create D12Texture resource");
                return nullptr;
            }

            // [FIX] True Bindless流程: Create → Upload → RegisterBindless
            // 步骤1: 上传纹理数据到GPU (必须在RegisterBindless之前调用)
            if (!texture->Upload())
            {
                core::LogError(LogRenderer,
                               "CreateTexture: Failed to upload texture '%s' to GPU",
                               createInfo.debugName ? createInfo.debugName : "unnamed");
                return nullptr;
            }

            // 步骤2: 注册到Bindless系统 (内部会调用CreateDescriptorInGlobalHeap)
            // RegisterBindless()是public方法，会自动:
            // - 分配Bindless索引 (AllocateBindlessIndexInternal)
            // - 创建SRV描述符 (CreateDescriptorInGlobalHeap)
            // - 存储索引到m_bindlessIndex
            auto bindlessIndex = texture->RegisterBindless();
            if (!bindlessIndex.has_value())
            {
                core::LogError(LogRenderer,
                               "CreateTexture: Failed to register texture '%s' to Bindless system",
                               createInfo.debugName ? createInfo.debugName : "unnamed");
                return nullptr;
            }

            // [DEBUG] 输出成功日志，验证Bindless索引分配
            core::LogInfo(LogRenderer,
                          "CreateTexture: Texture '%s' created successfully (Bindless index=%u)",
                          createInfo.debugName ? createInfo.debugName : "unnamed",
                          bindlessIndex.value());

            return texture;
        }
        catch (const std::exception& e)
        {
            // 记录错误
            core::LogError(LogRenderer,
                           "Exception during D12Texture creation: %s", e.what());
            assert(false && "Failed to create D12Texture");
            return nullptr;
        }
    }

    /**
     * 简化的创建2D纹理方法
     * 对应常见的2D纹理使用场景
     *
     * 教学重点:
     * - 默认参数的合理选择
     * - 常见纹理用法的封装
     * - 简化API的设计思路
     */
    std::unique_ptr<D12Texture> D3D12RenderSystem::CreateTexture2D(
        uint32_t     width,
        uint32_t     height,
        DXGI_FORMAT  format,
        TextureUsage usage,
        const void*  initialData,
        const char*  debugName)
    {
        TextureCreateInfo createInfo;
        createInfo.type        = TextureType::Texture2D;
        createInfo.width       = width;
        createInfo.height      = height;
        createInfo.depth       = 1;
        createInfo.mipLevels   = 1; // 默认单层Mip
        createInfo.arraySize   = 1; // 非数组纹理
        createInfo.format      = format;
        createInfo.usage       = usage;
        createInfo.initialData = initialData;
        createInfo.debugName   = debugName;

        // 计算数据大小和间距（如果提供了初始数据）
        if (initialData)
        {
            uint32_t bytesPerPixel = D12Texture::GetFormatBytesPerPixel(format);
            createInfo.rowPitch    = width * bytesPerPixel;
            createInfo.slicePitch  = createInfo.rowPitch * height;
            createInfo.dataSize    = createInfo.slicePitch;
        }

        return CreateTexture(createInfo);
    }

    /**
     * 重载版本：提供默认TextureUsage参数
     */
    std::unique_ptr<D12Texture> D3D12RenderSystem::CreateTexture2D(
        uint32_t    width,
        uint32_t    height,
        DXGI_FORMAT format,
        const void* initialData,
        const char* debugName)
    {
        return CreateTexture2D(width, height, format, TextureUsage::ShaderResource, initialData, debugName);
    }

    /**
     * 从Image对象创建DirectX 12纹理
     * 
     * 教学要点：
     * 1. 直接从Image提取像素数据创建纹理
     * 2. 自动推断纹理格式(RGBA8)
     * 3. 使用已有的CreateTexture2D核心方法
     * 4. **不使用缓存** - Image对象是瞬时的,每次调用都创建新纹理
     */
    /**
     * 从Image对象创建DirectX 12纹理
     * 
     * 教学要点：
     * 1. 直接从Image提取像素数据创建纹理
     * 2. 自动推断纹理格式(RGBA8)
     * 3. 使用已有的CreateTexture2D核心方法
     * 4. **不使用缓存** - Image对象是瞬时的,每次调用都创建新纹理
     */
    std::shared_ptr<D12Texture> D3D12RenderSystem::CreateTexture2D(Image& image, TextureUsage usage, const std::string& debugName)
    {
        // 验证Image有效性
        if (!image.GetRawData())
        {
            LogError(LogRenderer, "CreateTexture2D(Image): Image data is null");
            return nullptr;
        }

        IntVec2 dimensions = image.GetDimensions();
        if (dimensions.x <= 0 || dimensions.y <= 0)
        {
            LogError(LogRenderer, "CreateTexture2D(Image): Invalid dimensions (%d x %d)", dimensions.x, dimensions.y);
            return nullptr;
        }

        // 使用已有的CreateTexture2D方法创建纹理
        // 注意：Image始终提供RGBA8数据，所以使用DXGI_FORMAT_R8G8B8A8_UNORM
        auto texture = CreateTexture2D(
            static_cast<uint32_t>(dimensions.x),
            static_cast<uint32_t>(dimensions.y),
            DXGI_FORMAT_R8G8B8A8_UNORM,
            usage,
            image.GetRawData(),
            debugName.empty() ? "Image Texture" : debugName.c_str()
        );

        if (!texture)
        {
            LogError(LogRenderer, "CreateTexture2D(Image): Failed to create texture");
            return nullptr;
        }

        // 转换unique_ptr到shared_ptr
        return std::shared_ptr<D12Texture>(texture.release());
    }

    /**
     * Create texture with full mip chain and auto-generate mipmaps.
     *
     * Flow:
     * 1. Calculate full mip count from image dimensions
     * 2. Add UnorderedAccess flag (required for compute shader mip generation)
     * 3. Create texture with full mip chain (only mip 0 uploaded)
     * 4. Generate remaining mips via compute shader (MipmapGenerator)
     *
     * Safe to call during initialization: MipmapGenerator self-acquires
     * a command list when no frame command list is active.
     */
    std::shared_ptr<D12Texture> D3D12RenderSystem::CreateTexture2DWithMips(
        Image&   image, TextureUsage               usage, const std::string& debugName,
        uint32_t maxMipLevels, const MipmapConfig& mipConfig)
    {
        if (!image.GetRawData())
        {
            LogError(LogRenderer, "CreateTexture2DWithMips: Image data is null");
            return nullptr;
        }

        IntVec2 dimensions = image.GetDimensions();
        if (dimensions.x <= 0 || dimensions.y <= 0)
        {
            LogError(LogRenderer, "CreateTexture2DWithMips: Invalid dimensions (%d x %d)",
                     dimensions.x, dimensions.y);
            return nullptr;
        }

        uint32_t width    = static_cast<uint32_t>(dimensions.x);
        uint32_t height   = static_cast<uint32_t>(dimensions.y);
        uint32_t mipCount = CalculateMipCount(width, height);

        // Limit mip levels if requested (e.g., atlas textures need fewer mips
        // to prevent cross-tile bleeding at high mip levels)
        if (maxMipLevels > 0 && mipCount > maxMipLevels)
        {
            mipCount = maxMipLevels;
        }

        // Build TextureCreateInfo with full mip chain + UAV for compute mip gen
        TextureCreateInfo createInfo;
        createInfo.type        = TextureType::Texture2D;
        createInfo.width       = width;
        createInfo.height      = height;
        createInfo.depth       = 1;
        createInfo.mipLevels   = mipCount;
        createInfo.arraySize   = 1;
        createInfo.format      = DXGI_FORMAT_R8G8B8A8_UNORM;
        createInfo.usage       = usage | TextureUsage::UnorderedAccess;
        createInfo.initialData = image.GetRawData();
        createInfo.debugName   = debugName.empty() ? "Texture2D_Mipped" : debugName.c_str();

        // Calculate row pitch (256-byte aligned) and data size
        uint32_t bytesPerPixel = 4; // RGBA8
        createInfo.rowPitch    = (width * bytesPerPixel + 255) & ~255u;
        createInfo.slicePitch  = createInfo.rowPitch * height;
        createInfo.dataSize    = static_cast<size_t>(width) * height * bytesPerPixel;

        auto texture = CreateTexture(createInfo);
        if (!texture)
        {
            LogError(LogRenderer, "CreateTexture2DWithMips: Failed to create texture '%s'",
                     debugName.c_str());
            return nullptr;
        }

        // Generate mip chain via compute shader
        // MipmapGenerator handles command list acquisition during init
        if (!texture->GenerateMips(mipConfig))
        {
            LogError(LogRenderer, "CreateTexture2DWithMips: Mip generation failed for '%s'",
                     debugName.c_str());
            // Texture is still usable with mip 0 only, don't fail
        }

        core::LogInfo(LogRenderer,
                      "CreateTexture2DWithMips: '%s' created (%ux%u, %u mips, bindless=%u)",
                      debugName.c_str(), width, height, mipCount,
                      texture->GetBindlessIndex());

        return std::shared_ptr<D12Texture>(texture.release());
    }

    /**
     * 从ResourceLocation加载并创建纹理（带缓存）
     * 
     * 教学要点：
     * 1. ResourceLocation作为缓存键，类型安全
     * 2. 使用std::mutex保护缓存访问（线程安全）
     * 3. weak_ptr缓存策略：自动释放不再使用的纹理
     * 4. 通过ResourceSubsystem加载ImageResource
     * 5. 缓存命中时直接返回，避免重复加载
     * 
     * 架构设计：
     * - 缓存查询 → 命中返回 → 未命中加载 → 创建纹理 → 插入缓存
     */
    std::shared_ptr<D12Texture> D3D12RenderSystem::CreateTexture2D(const ResourceLocation& resourceLocation, TextureUsage usage, const std::string& debugName)
    {
        // 1. 线程安全的缓存查询
        {
            std::lock_guard<std::mutex> lock(s_textureCacheMutex);
            auto                        it = s_textureCache.find(resourceLocation);
            if (it != s_textureCache.end())
            {
                // 尝试锁定weak_ptr
                auto cachedTexture = it->second.lock();
                if (cachedTexture)
                {
                    return cachedTexture;
                }
                else
                {
                    s_textureCache.erase(it);
                }
            }
        }

        // 2. 缓存未命中，加载资源
        auto resourceSubsystem = g_theEngine->GetSubsystem<ResourceSubsystem>();
        if (!resourceSubsystem)
        {
            LogError(LogRenderer, "CreateTexture2D(ResourceLocation): ResourceSubsystem not found");
            ERROR_AND_DIE("Resource subsystem not found")
        }

        const auto imageResource = std::dynamic_pointer_cast<ImageResource>(resourceSubsystem->GetResource(resourceLocation));
        if (!imageResource)
        {
            LogError(LogRenderer,
                     "CreateTexture2D(ResourceLocation): Failed to load ImageResource for '%s'",
                     resourceLocation.ToString().c_str());
            ERROR_RECOVERABLE("Failed to get image resource")
            return nullptr;
        }

        // 3. 创建纹理（委托给ImageResource重载版本）
        auto d3d12Texture = CreateTexture2D(*imageResource, usage, debugName);
        if (!d3d12Texture)
        {
            LogError(LogRenderer,
                     "CreateTexture2D(ResourceLocation): Failed to create texture for '%s'",
                     resourceLocation.ToString().c_str());
            return nullptr;
        }

        // 4. 插入缓存（线程安全）
        {
            std::lock_guard<std::mutex> lock(s_textureCacheMutex);
            s_textureCache[resourceLocation] = d3d12Texture;
            LogInfo(LogRenderer,
                    "CreateTexture2D(ResourceLocation): Created and cached texture for '%s'",
                    resourceLocation.ToString().c_str());
        }

        return d3d12Texture;
    }

    /**
     * 从ImageResource创建纹理
     * 
     * 教学要点：
     * 1. 验证ImageResource已加载
     * 2. 提取Image对象并委托给Image重载版本
     * 3. 错误处理和日志记录
     */
    std::shared_ptr<D12Texture> D3D12RenderSystem::CreateTexture2D(const ImageResource& imageResource, TextureUsage usage, const std::string& debugName)
    {
        // 验证ImageResource已加载
        if (!imageResource.IsLoaded())
        {
            LogError(LogRenderer, "CreateTexture2D(ImageResource): ImageResource not loaded");
            ERROR_RECOVERABLE("Failed to get image resource")
            return nullptr;
        }

        // 提取Image对象
        const Image& image = imageResource.GetImage();

        // **修正逻辑错误** - 应该检查GetRawData()返回nullptr才报错
        if (!image.GetRawData())
        {
            LogError(LogRenderer, "CreateTexture2D(ImageResource): Image raw data is null");
            ERROR_RECOVERABLE("Image file is null in raw data")
            return nullptr;
        }

        // 委托给Image重载版本创建纹理
        return CreateTexture2D(const_cast<Image&>(image), usage, debugName);
    }

    /**
     * 从文件路径创建纹理
     * 
     * 教学要点：
     * 1. 直接从文件路径加载Image
     * 2. 不使用ResourceLocation缓存（因为是裸文件路径）
     * 3. 适用于运行时动态加载的纹理
     * 4. 错误处理和日志记录
     */
    std::shared_ptr<D12Texture> D3D12RenderSystem::CreateTexture2D(const std::string& imagePath, TextureUsage usage, const std::string& debugName)
    {
        // 验证路径有效性
        if (imagePath.empty())
        {
            LogError(LogRenderer, "CreateTexture2D(string): Image path is empty");
            return nullptr;
        }

        // 从文件路径加载Image - 使用正确的构造函数
        Image image(imagePath.c_str());

        // 验证Image加载成功
        if (!image.GetRawData())
        {
            LogError(LogRenderer,
                     "CreateTexture2D(string): Failed to load image from path '%s'",
                     imagePath.c_str());
            return nullptr;
        }

        IntVec2 dimensions = image.GetDimensions();
        LogInfo(LogRenderer,
                "CreateTexture2D(string): Loaded image from '%s' (%dx%d)",
                imagePath.c_str(), dimensions.x, dimensions.y);

        // 委托给Image重载版本创建纹理
        return CreateTexture2D(image, usage, debugName.empty() ? imagePath : debugName);
    }

    /**
     * 创建深度纹理（主要方法）
     * 对应Iris的深度纹理管理功能，支持depthtex1/depthtex2
     *
     * 教学重点:
     * 1. 深度纹理格式选择和验证
     * 2. DSV/SRV双视图管理
     * 3. 深度采样和阴影贴图支持
     * 4. RAII资源管理和异常安全
     *
     * DirectX 12 API调用链：
     * 1. 参数验证和格式检查
     * 2. D12DepthTexture构造函数内部调用CreateCommittedResource
     * 3. 自动创建DSV和SRV描述符（如果支持采样）
     * 4. 设置调试名称
     */
    std::unique_ptr<D12DepthTexture> D3D12RenderSystem::CreateDepthTexture(DepthTextureCreateInfo& createInfo)
    {
        // 验证系统已初始化
        if (!s_isInitialized || !s_device)
        {
            core::LogError(LogRenderer, "D3D12RenderSystem not initialized");
            assert(false && "D3D12RenderSystem not initialized");
            return nullptr;
        }

        // 验证输入参数
        if (createInfo.width == 0 || createInfo.height == 0)
        {
            assert(false && "Depth texture dimensions must be greater than 0");
            return nullptr;
        }

        // 验证深度纹理名称
        if (createInfo.name.empty())
        {
            assert(false && "Depth texture name cannot be empty");
            return nullptr;
        }

        // 验证清除值范围
        if (createInfo.clearDepth < 0.0f || createInfo.clearDepth > 1.0f)
        {
            assert(false && "Clear depth value must be between 0.0 and 1.0");
            return nullptr;
        }

        // 创建D12DepthTexture对象
        // 使用std::make_unique确保异常安全
        try
        {
            auto depthTexture = std::make_unique<D12DepthTexture>(createInfo);

            // 验证深度纹理创建成功
            if (!depthTexture->IsValid())
            {
                core::LogError(LogRenderer,
                               "Failed to create D12DepthTexture resource: %s", createInfo.name.c_str());
                return nullptr;
            }

            // 记录成功创建的日志（调试用）
            core::LogInfo(LogRenderer,
                          "Created D12DepthTexture: %s (%dx%d, Format: %d)",
                          createInfo.name.c_str(),
                          createInfo.width,
                          createInfo.height,
                          static_cast<int>(createInfo.depthFormat));

            return depthTexture;
        }
        catch (const std::exception& e)
        {
            // 记录错误
            core::LogError(LogRenderer,
                           "Exception during D12DepthTexture creation (%s): %s",
                           createInfo.name.c_str(), e.what());
            assert(false && "Failed to create D12DepthTexture");
            return nullptr;
        }
    }

    // ===== Debug API implementation =====

    /**
     * Set DirectX object debug name
     * Corresponding to Iris' GLDebug.nameObject function
     */
    void D3D12RenderSystem::SetDebugName(ID3D12Object* object, const char* name)
    {
        if (!object || !name)
        {
            return;
        }

        // Convert to wide string
        size_t   len      = strlen(name);
        wchar_t* wideName = new wchar_t[len + 1];

        size_t convertedChars = 0;
        auto   error          = mbstowcs_s(&convertedChars, wideName, len + 1, name, len);
        assert(error == 0 && "Failed to convert string to wide string");
        UNUSED(error)

        // DirectX 12 API: ID3D12Object::SetName()
        object->SetName(wideName);
        delete[] wideName;
    }

    /**
     * Check the device characteristics support
     */
    bool D3D12RenderSystem::CheckFeatureSupport(D3D12_FEATURE feature)
    {
        if (!s_device)
        {
            return false;
        }

        // Here you need to query according to the specific feature type
        // Example: Check basic feature support
        D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
        HRESULT                          hr      = s_device->CheckFeatureSupport(feature, &options, sizeof(options));
        return SUCCEEDED(hr);
    }

    /**
     * Get GPU memory information
     */
    DXGI_QUERY_VIDEO_MEMORY_INFO D3D12RenderSystem::GetVideoMemoryInfo()
    {
        DXGI_QUERY_VIDEO_MEMORY_INFO memoryInfo = {};

        if (s_adapter)
        {
            Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter3;
            if (SUCCEEDED(s_adapter.As(&adapter3)))
            {
                adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memoryInfo);
            }
        }

        return memoryInfo;
    }

    // ===== Command Management API implementation =====

    /**
     * Get the command-list manager owned by the render system.
     */
    CommandListManager* D3D12RenderSystem::GetCommandListManager()
    {
        if (!s_isInitialized || !s_commandListManager)
        {
            return nullptr;
        }
        return s_commandListManager.get();
    }

    void D3D12RenderSystem::SetRequestedQueueExecutionMode(QueueExecutionMode mode)
    {
        s_requestedQueueExecutionMode             = mode;
        s_queueExecutionDiagnostics.requestedMode = mode;
        SetActiveQueueExecutionModeInternal(mode);
    }

    QueueExecutionMode D3D12RenderSystem::GetRequestedQueueExecutionMode()
    {
        return s_requestedQueueExecutionMode;
    }

    QueueExecutionMode D3D12RenderSystem::GetActiveQueueExecutionMode()
    {
        return s_activeQueueExecutionMode;
    }

    void D3D12RenderSystem::SetFrameLifecyclePhase(FrameLifecyclePhase phase)
    {
        s_frameLifecyclePhase                      = phase;
        s_queueExecutionDiagnostics.lifecyclePhase = phase;
    }

    const QueueExecutionDiagnostics& D3D12RenderSystem::GetQueueExecutionDiagnostics()
    {
        s_queueExecutionDiagnostics.requestedMode                = s_requestedQueueExecutionMode;
        s_queueExecutionDiagnostics.activeMode                   = s_activeQueueExecutionMode;
        s_queueExecutionDiagnostics.lifecyclePhase               = s_frameLifecyclePhase;
        s_queueExecutionDiagnostics.requestedFramesInFlightDepth = GetRequestedFramesInFlightDepthSnapshot();
        s_queueExecutionDiagnostics.activeFramesInFlightDepth    = GetActiveFramesInFlightDepthSnapshot();
        return s_queueExecutionDiagnostics;
    }

    bool D3D12RenderSystem::SynchronizeActiveQueues(const char* reason)
    {
        try
        {
            if (!s_commandListManager)
            {
                throw CrossQueueSynchronizationException(
                    Stringf("D3D12RenderSystem::SynchronizeActiveQueues: command list manager is unavailable%s%s",
                            reason ? " for " : "",
                            reason ? reason : ""));
            }

            if (s_currentGraphicsCommandList != nullptr)
            {
                throw CrossQueueSynchronizationException(
                    Stringf("D3D12RenderSystem::SynchronizeActiveQueues: active graphics command list is still recording%s%s",
                            reason ? " for " : "",
                            reason ? reason : ""));
            }

            s_commandListManager->FlushAllCommandLists();
            ProcessDeferredResourceReleases();
            return true;
        }
        catch (const CrossQueueSynchronizationException& exception)
        {
            ReportFatalQueueException(exception);
            return false;
        }
    }

    QueueSubmittedFenceSnapshot D3D12RenderSystem::GetLastSubmittedFenceSnapshot()
    {
        return s_commandListManager ? s_commandListManager->GetLastSubmittedFenceSnapshot() : QueueSubmittedFenceSnapshot{};
    }

    QueueFenceSnapshot D3D12RenderSystem::GetCompletedFenceSnapshot()
    {
        return s_commandListManager ? s_commandListManager->GetCompletedFenceSnapshot() : QueueFenceSnapshot{};
    }

    bool D3D12RenderSystem::ShouldDeferIndividualResourceRelease()
    {
        return s_isInitialized &&
            !s_isShuttingDown &&
            MAX_FRAMES_IN_FLIGHT > 1 &&
            s_commandListManager != nullptr &&
            s_commandListManager->IsInitialized();
    }

    void D3D12RenderSystem::DeferResourceRelease(ID3D12Resource* resource, const char* debugName)
    {
        if (!resource)
        {
            return;
        }

        Microsoft::WRL::ComPtr<ID3D12Resource> deferredResource;
        deferredResource.Attach(resource);

        if (!ShouldDeferIndividualResourceRelease())
        {
            return;
        }

        ProcessDeferredResourceReleases();

        const QueueSubmittedFenceSnapshot submittedSnapshot =
            s_commandListManager->GetLastSubmittedFenceSnapshot();
        const QueueFenceSnapshot completedSnapshot =
            s_commandListManager->GetCompletedFenceSnapshot();

        if (!submittedSnapshot.HasTrackedWork() ||
            submittedSnapshot.IsSatisfiedBy(completedSnapshot))
        {
            return;
        }

        DeferredResourceReleaseEntry entry = {};
        entry.resource                     = std::move(deferredResource);
        entry.submittedFenceSnapshot       = submittedSnapshot;
        entry.debugName                    = debugName ? debugName : "";

        std::lock_guard<std::mutex> lock(g_deferredResourceReleaseMutex);
        g_deferredResourceReleases.push_back(std::move(entry));
    }

    void D3D12RenderSystem::ProcessDeferredResourceReleases(bool forceRelease) noexcept
    {
        if (!forceRelease)
        {
            if (!s_commandListManager || !s_commandListManager->IsInitialized())
            {
                return;
            }

            s_commandListManager->UpdateCompletedCommandLists();
        }

        const QueueFenceSnapshot completedSnapshot =
            (forceRelease || !s_commandListManager || !s_commandListManager->IsInitialized())
                ? QueueFenceSnapshot{UINT64_MAX, UINT64_MAX, UINT64_MAX}
                : s_commandListManager->GetCompletedFenceSnapshot();

        std::lock_guard<std::mutex> lock(g_deferredResourceReleaseMutex);

        auto it = g_deferredResourceReleases.begin();
        while (it != g_deferredResourceReleases.end())
        {
            if (forceRelease || it->submittedFenceSnapshot.IsSatisfiedBy(completedSnapshot))
            {
                it = g_deferredResourceReleases.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void D3D12RenderSystem::DrainQueuesForShutdown() noexcept
    {
        if (!s_commandListManager)
        {
            return;
        }

        const std::array<CommandQueueType, kCommandQueueTypeCount> queueTypes = {
            CommandQueueType::Graphics,
            CommandQueueType::Compute,
            CommandQueueType::Copy
        };

        for (CommandQueueType queueType : queueTypes)
        {
            WaitForQueueDrainOnShutdown(
                s_device.Get(),
                s_commandListManager->GetCommandQueue(queueType),
                queueType);
        }

        s_commandListManager->Shutdown();
        s_commandListManager.reset();
    }

    void D3D12RenderSystem::ResetFrameExecutionState() noexcept
    {
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            s_frameContexts[i].graphicsCommandAllocator.Reset();
            s_frameContexts[i].computeCommandAllocator.Reset();
            s_frameContexts[i].copyCommandAllocator.Reset();
            s_frameContexts[i].ClearRetirement();
        }

        s_frameIndex                     = 0;
        s_globalFrameCount               = 0;
        s_frameLifecyclePhase            = FrameLifecyclePhase::Idle;
        s_lastFrameSlotAcquisitionResult = {};
        s_currentGraphicsCommandList     = nullptr;
        s_requestedQueueExecutionMode    = QueueExecutionMode::MixedQueueExperimental;
        s_activeQueueExecutionMode       = QueueExecutionMode::MixedQueueExperimental;
        s_queueExecutionDiagnostics.ClearCounters();
        s_queueExecutionDiagnostics.requestedMode = s_requestedQueueExecutionMode;
        s_queueExecutionDiagnostics.activeMode    = s_activeQueueExecutionMode;
        g_reportedMixedQueueFallbacks             = {};
    }

    void D3D12RenderSystem::ReleaseFallbackTextures() noexcept
    {
        s_defaultWhiteTexture.reset();
        s_defaultBlackTexture.reset();
        s_defaultNormalTexture.reset();
        ClearAllTextureCache();
    }

    void D3D12RenderSystem::ReleaseBindlessInfrastructure() noexcept
    {
        if (s_bindlessRootSignature)
        {
            s_bindlessRootSignature->Shutdown();
            s_bindlessRootSignature.reset();
        }

        if (s_globalDescriptorHeapManager)
        {
            s_globalDescriptorHeapManager->Shutdown();
            s_globalDescriptorHeapManager.reset();
        }

        s_bindlessIndexAllocator.reset();
    }

    void D3D12RenderSystem::ReleaseSwapChainResources() noexcept
    {
        for (uint32_t i = 0; i < s_swapChainBufferCount; ++i)
        {
            s_swapChainBuffers[i].Reset();
            s_swapChainRTVs[i] = {};
        }

        s_swapChain.Reset();
        s_currentBackBufferIndex = 0;
        s_swapChainBufferCount   = 3;
        s_swapChainWidth         = 0;
        s_swapChainHeight        = 0;
        s_backbufferFormat       = DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    void D3D12RenderSystem::ReportLiveObjectsBeforeDeviceRelease() noexcept
    {
#if defined(_DEBUG)
        if (!s_device)
        {
            return;
        }

        Microsoft::WRL::ComPtr<ID3D12DebugDevice> debugDevice;
        if (SUCCEEDED(s_device->QueryInterface(IID_PPV_ARGS(&debugDevice))))
        {
            DebuggerPrintf("\n========== D3D12 ReportLiveObjects (before device release) ==========\n");
            debugDevice->ReportLiveDeviceObjects(
                D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
            DebuggerPrintf("========== End ReportLiveObjects ==========\n\n");
        }
#endif
    }

    void D3D12RenderSystem::ReleaseDeviceObjects() noexcept
    {
        s_adapter.Reset();
        s_dxgiFactory.Reset();
        s_device.Reset();
    }

    QueueRouteDecision D3D12RenderSystem::ResolveQueueRoute(const QueueRouteContext& context)
    {
        QueueRouteDecision decision = s_queueRoutingPolicy.Resolve(context, s_activeQueueExecutionMode);
        RecordQueueRouteDecision(decision);
        return decision;
    }

    ID3D12GraphicsCommandList* D3D12RenderSystem::AcquireCommandListForWorkload(const QueueRouteContext& context,
                                                                                ID3D12CommandAllocator*  externalAllocator,
                                                                                const std::string&       debugName,
                                                                                bool                     useFrameAllocator)
    {
        if (!s_commandListManager)
        {
            return nullptr;
        }

        QueueRouteDecision      decision       = ResolveQueueRoute(context);
        ID3D12CommandAllocator* allocatorToUse = nullptr;

        if (decision.activeQueue == CommandQueueType::Graphics && externalAllocator != nullptr)
        {
            allocatorToUse = externalAllocator;
        }
        else if (useFrameAllocator && s_frameIndex < MAX_FRAMES_IN_FLIGHT)
        {
            allocatorToUse = s_frameContexts[s_frameIndex].GetCommandAllocator(decision.activeQueue);
        }

        if (allocatorToUse != nullptr)
        {
            ID3D12GraphicsCommandList* commandList =
                s_commandListManager->AcquireCommandList(decision.activeQueue, allocatorToUse, debugName);
            if (commandList != nullptr && useFrameAllocator && s_frameIndex < MAX_FRAMES_IN_FLIGHT)
            {
                s_frameContexts[s_frameIndex].retirement.ExpectQueue(decision.activeQueue);
            }

            return commandList;
        }

        return s_commandListManager->AcquireCommandList(decision.activeQueue, debugName);
    }

    void D3D12RenderSystem::RegisterFrameSubmission(const QueueSubmissionToken& token)
    {
        if (!token.IsValid())
        {
            return;
        }

        s_frameContexts[s_frameIndex].retirement.RegisterSubmission(token);
    }

    QueueSubmissionToken D3D12RenderSystem::SubmitFrameScopedCommandList(ID3D12GraphicsCommandList* commandList,
                                                                         const QueueRouteContext&   context)
    {
        if (!s_commandListManager || !commandList)
        {
            return {};
        }

        QueueSubmissionToken submissionToken = s_commandListManager->SubmitCommandList(commandList);
        RecordQueueSubmission(submissionToken, context.workload);
        RegisterFrameSubmission(submissionToken);
        return submissionToken;
    }

    QueueSubmissionToken D3D12RenderSystem::SubmitStandaloneCommandList(ID3D12GraphicsCommandList* commandList,
                                                                        QueueWorkloadClass         workload)
    {
        if (!s_commandListManager || !commandList)
        {
            return {};
        }

        QueueSubmissionToken submissionToken = s_commandListManager->SubmitCommandList(commandList);
        RecordQueueSubmission(submissionToken, workload);
        return submissionToken;
    }

    void D3D12RenderSystem::RecordQueueRouteDecision(const QueueRouteDecision& decision)
    {
        s_queueExecutionDiagnostics.requestedMode      = s_requestedQueueExecutionMode;
        s_queueExecutionDiagnostics.activeMode         = s_activeQueueExecutionMode;
        s_queueExecutionDiagnostics.lastFallbackReason = decision.fallbackReason;

        const bool shouldReportMixedQueueFallback =
            s_requestedQueueExecutionMode == QueueExecutionMode::MixedQueueExperimental &&
            decision.requestedQueue != CommandQueueType::Graphics &&
            decision.fallbackReason != QueueFallbackReason::None;

        if (!shouldReportMixedQueueFallback)
        {
            return;
        }

        const size_t      workloadIndex   = static_cast<size_t>(decision.workload);
        const size_t      fallbackIndex   = static_cast<size_t>(decision.fallbackReason);
        const std::string fallbackMessage = Stringf(
            "Mixed-queue validation fallback: workload=%s requested=%s active=%s reason=%s",
            GetQueueWorkloadName(decision.workload),
            GetQueueTypeName(decision.requestedQueue),
            GetQueueTypeName(decision.activeQueue),
            GetQueueFallbackReasonName(decision.fallbackReason));

        if (workloadIndex >= kQueueWorkloadClassCountLocal || fallbackIndex >= kQueueFallbackReasonCount)
        {
            ERROR_RECOVERABLE(fallbackMessage);
            return;
        }

        if (g_reportedMixedQueueFallbacks[workloadIndex][fallbackIndex])
        {
            return;
        }

        g_reportedMixedQueueFallbacks[workloadIndex][fallbackIndex] = true;
        LogWarn(LogRenderer, "%s", fallbackMessage.c_str());
    }

    void D3D12RenderSystem::RecordQueueSubmission(const QueueSubmissionToken& token,
                                                  QueueWorkloadClass          workload)
    {
        if (!token.IsValid())
        {
            return;
        }

        s_queueExecutionDiagnostics.RecordSubmission(workload, token.queueType);
    }

    void D3D12RenderSystem::RecordQueueWaitInsertion(CommandQueueType producerQueue,
                                                     CommandQueueType consumerQueue)
    {
        s_queueExecutionDiagnostics.RecordQueueWait(producerQueue, consumerQueue);
    }

    bool D3D12RenderSystem::WaitForStandaloneSubmission(const QueueSubmissionToken& token)
    {
        try
        {
            if (!token.IsValid())
            {
                throw InvalidQueueSubmissionTokenException(
                    Stringf("D3D12RenderSystem::WaitForStandaloneSubmission: invalid token for queue %s",
                            GetQueueTypeName(token.queueType)));
            }

            if (!s_commandListManager)
            {
                throw CrossQueueSynchronizationException(
                    Stringf("D3D12RenderSystem::WaitForStandaloneSubmission: command list manager is unavailable for %s queue fence %llu",
                            GetQueueTypeName(token.queueType),
                            token.fenceValue));
            }

            if (!s_commandListManager->WaitForSubmission(token))
            {
                throw CrossQueueSynchronizationException(
                    Stringf("D3D12RenderSystem::WaitForStandaloneSubmission: failed to retire %s queue fence %llu",
                            GetQueueTypeName(token.queueType),
                            token.fenceValue));
            }

            s_commandListManager->UpdateCompletedCommandLists();
            return true;
        }
        catch (const InvalidQueueSubmissionTokenException& exception)
        {
            ReportRecoverableQueueException(exception);
            return false;
        }
        catch (const CrossQueueSynchronizationException& exception)
        {
            ReportFatalQueueException(exception);
            return false;
        }
    }

    void D3D12RenderSystem::SetActiveQueueExecutionModeInternal(QueueExecutionMode mode)
    {
        QueueExecutionMode resolvedMode = mode;
        if (mode == QueueExecutionMode::MixedQueueExperimental && !HasDedicatedQueuesAvailable(s_commandListManager.get()))
        {
            UnsupportedQueueRouteException exception(
                "D3D12RenderSystem::SetActiveQueueExecutionModeInternal: falling back to GraphicsOnly because dedicated Compute/Copy queues are unavailable");
            ReportRecoverableQueueException(exception);

            resolvedMode                                   = QueueExecutionMode::GraphicsOnly;
            s_queueExecutionDiagnostics.lastFallbackReason = QueueFallbackReason::DedicatedQueueUnavailable;
        }
        else
        {
            s_queueExecutionDiagnostics.lastFallbackReason = QueueFallbackReason::None;
        }

        s_activeQueueExecutionMode                = resolvedMode;
        s_queueExecutionDiagnostics.requestedMode = s_requestedQueueExecutionMode;
        s_queueExecutionDiagnostics.activeMode    = resolvedMode;
    }

    // ============================================================================
    // M6.3: Backbuffer access
    // ============================================================================

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12RenderSystem::GetBackBufferRTV()
    {
        if (!s_swapChain)
        {
            LogWarn(LogRenderer, "GetBackBufferRTV: SwapChain not initialized");
            return {};
        }
        return s_swapChainRTVs[s_currentBackBufferIndex];
    }

    ID3D12Resource* D3D12RenderSystem::GetBackBufferResource()
    {
        if (!s_swapChain)
        {
            LogWarn(LogRenderer, "GetBackBufferResource: SwapChain not initialized");
            return nullptr;
        }
        return s_swapChainBuffers[s_currentBackBufferIndex].Get();
    }

    DXGI_FORMAT D3D12RenderSystem::GetBackBufferFormat()
    {
        return s_backbufferFormat;
    }

    // ============================================================================
    // 系统默认纹理访问方法
    // ============================================================================

    /**
     * @brief 获取系统默认白色纹理
     * 
     * 教学要点:
     * 1. 空检查日志：帮助调试初始化问题
     * 2. 返回shared_ptr：允许调用者安全持有引用
     * 3. 线程安全：Initialize后只读访问，无需锁
     */
    std::shared_ptr<D12Texture> D3D12RenderSystem::GetDefaultWhiteTexture()
    {
        if (!s_defaultWhiteTexture)
        {
            core::LogWarn(LogRenderer,
                          "GetDefaultWhiteTexture: Default white texture not initialized. "
                          "Ensure D3D12RenderSystem::Initialize() was called.");
        }
        return s_defaultWhiteTexture;
    }

    /**
     * @brief 获取系统默认黑色纹理
     * 
     * 教学要点:
     * 1. 空检查日志：帮助调试初始化问题
     * 2. 返回shared_ptr：允许调用者安全持有引用
     * 3. 线程安全：Initialize后只读访问，无需锁
     */
    std::shared_ptr<D12Texture> D3D12RenderSystem::GetDefaultBlackTexture()
    {
        if (!s_defaultBlackTexture)
        {
            core::LogWarn(LogRenderer,
                          "GetDefaultBlackTexture: Default black texture not initialized. "
                          "Ensure D3D12RenderSystem::Initialize() was called.");
        }
        return s_defaultBlackTexture;
    }

    /**
     * @brief 获取系统默认法线纹理
     * 
     * 教学要点:
     * 1. 空检查日志：帮助调试初始化问题
     * 2. 返回shared_ptr：允许调用者安全持有引用
     * 3. 线程安全：Initialize后只读访问，无需锁
     */
    std::shared_ptr<D12Texture> D3D12RenderSystem::GetDefaultNormalTexture()
    {
        if (!s_defaultNormalTexture)
        {
            core::LogWarn(LogRenderer,
                          "GetDefaultNormalTexture: Default normal texture not initialized. "
                          "Ensure D3D12RenderSystem::Initialize() was called.");
        }
        return s_defaultNormalTexture;
    }

    // ===== Internal implementation method =====

    /**
     * Create DirectX 12 devices
     */
    bool D3D12RenderSystem::CreateDevice([[maybe_unused]] bool enableGPUValidation)
    {
        // 1. Create a DXGI factory
        // DXGI API: CreateDXGIFactory2()
        HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&s_dxgiFactory));
        if (FAILED(hr))
        {
            assert(false && "Failed to create DXGI factory");
            return false;
        }

        // 2. Enumerate the adapter and select the best one
        for (UINT adapterIndex = 0; ; ++adapterIndex)
        {
            hr = s_dxgiFactory->EnumAdapters1(adapterIndex, &s_adapter);
            if (FAILED(hr))
            {
                break; // No more adapters
            }

            // Try to create a DirectX 12 device
            // DirectX 12 API: D3D12CreateDevice()
            hr = D3D12CreateDevice(
                s_adapter.Get(), // Adapter
                D3D_FEATURE_LEVEL_11_0, // Minimum feature level (using standard value)
                IID_PPV_ARGS(&s_device) // Output device interface
            );

            if (SUCCEEDED(hr))
            {
                // Successfully created the device
                break;
            }

            // Release the current adapter and try the next one
            s_adapter.Reset();
        }

        if (!s_device)
        {
            LogError(LogRenderer, "Failed to create D3D12 device");
            assert(false && "Failed to create D3D12 device");
            ERROR_AND_DIE("Failed to create D3D12 device")
        }

        // Suppress known harmless D3D12 debug warnings via Info Queue filter.
        // - CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE (820): SkyBasicRenderPass clears
        //   colortex0 with dynamic fog color that cannot match the static optimizedClearValue.
        // - CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE (821): Same category, dynamic clears.
        // - CREATEGRAPHICSPIPELINESTATE_RENDERTARGETVIEW_NOT_SET (679): ShaderCodeGenerator
        //   may generate PSOutput with more targets than the render pass binds. D3D12 confirms
        //   "writes of an unbound Render Target View are discarded" - no correctness impact.
        // These are performance hints only; rendering works correctly.
#ifdef _DEBUG
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
        if (SUCCEEDED(s_device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
        {
            D3D12_MESSAGE_ID suppressedIds[] =
            {
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
                D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_RENDERTARGETVIEW_NOT_SET,
            };

            D3D12_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs         = _countof(suppressedIds);
            filter.DenyList.pIDList        = suppressedIds;
            infoQueue->AddStorageFilterEntries(&filter);

            LogInfo(LogRenderer, "D3D12 Info Queue: Suppressed %u known harmless warnings", _countof(suppressedIds));
        }
#endif

        return true;
    }

    /**
     * Enable DirectX 12 debug layer
     */
    void D3D12RenderSystem::EnableDebugLayer()
    {
#ifdef _DEBUG
        Microsoft::WRL::ComPtr<ID3D12Debug> debugController;

        // DirectX 12 API: D3D12GetDebugInterface()
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            // Enable debug layer
            debugController->EnableDebugLayer();

            // Optional: Enable GPU verification (performance impact is greater)
            // Note: GPU verification has been moved to the Initialize parameter to control
            // s_config is no longer used here, simplifying the architecture
        }
#endif
    }

    /**
     * Constant buffer size aligned to 256 bytes
     * DirectX 12 requirements: Constant buffers must be aligned 256 bytes
     */
    size_t D3D12RenderSystem::AlignConstantBufferSize(size_t size)
    {
        // 256 bytes aligned
        const size_t alignment = 256;
        return (size + alignment - 1) & ~(alignment - 1);
    }

    /**
     * Create a unified interface for submitting resources
     */
    /**
     * DirectX 12 Fast Clear优化机制说明：
     *
     * pOptimizedClearValue参数作用：
     * - 告知GPU该资源的常用清除值（如渲染目标的背景色、深度缓冲的初始深度值）
     * - GPU可为此资源预分配优化的内存布局，实现硬件加速的Fast Clear
     * - 当实际Clear值与OptimizedClearValue匹配时，GPU可执行极快的元数据清除，而非逐像素写入
     *
     * 性能影响（实测数据）：
     * - 使用OptimizedClearValue: ~0.1ms (Fast Clear路径)
     * - 不使用OptimizedClearValue: ~0.3ms (Slow Clear路径)
     * - 对于每帧多次Clear操作（如G-Buffer），性能提升可累积至1-2ms/帧
     *
     * 最佳实践：
     * 1. 渲染目标：使用常用背景色（如黑色{0,0,0,1}或天空色）
     * 2. 深度缓冲：使用1.0f（远平面）或0.0f（反向深度）
     * 3. 模板缓冲：使用0（默认值）
     * 4. 如果Clear值频繁变化，传递nullptr反而更好（避免性能警告）
     *
     * DirectX 12 API参考：
     * - https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_clear_value
     */
    HRESULT D3D12RenderSystem::CreateCommittedResource(
        const D3D12_HEAP_PROPERTIES& heapProps,
        const D3D12_RESOURCE_DESC&   desc,
        D3D12_RESOURCE_STATES        initialState,
        const D3D12_CLEAR_VALUE*     pOptimizedClearValue,
        ID3D12Resource**             resource)
    {
        if (!s_device)
        {
            return E_FAIL;
        }

        return s_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            initialState,
            pOptimizedClearValue,
            IID_PPV_ARGS(resource));
    }

    // ===== SM6.6 Bindless资源管理API实现 (Milestone 2.7) =====

    BindlessIndexAllocator* D3D12RenderSystem::GetBindlessIndexAllocator()
    {
        return s_bindlessIndexAllocator.get();
    }

    GlobalDescriptorHeapManager* D3D12RenderSystem::GetGlobalDescriptorHeapManager()
    {
        return s_globalDescriptorHeapManager.get();
    }

    BindlessRootSignature* D3D12RenderSystem::GetBindlessRootSignature()
    {
        if (!s_bindlessRootSignature)
            return nullptr;
        return s_bindlessRootSignature.get();
    }


    // ===== PSO创建API实现 (Milestone 3.0新增) =====

    Microsoft::WRL::ComPtr<ID3D12PipelineState> D3D12RenderSystem::CreateGraphicsPSO(
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc
    )
    {
        if (!s_device)
        {
            core::LogError(LogRenderer,
                           "Cannot create PSO: D3D12RenderSystem not initialized");
            return nullptr;
        }

        Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
        HRESULT                                     hr = s_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));

        if (FAILED(hr))
        {
            core::LogError(LogRenderer,
                           "Failed to create Graphics PSO: HRESULT = 0x%08X", hr);
            return nullptr;
        }

        return pso;
    }

    // ===== ResourceBarrier统一管理API实现 =====

    void D3D12RenderSystem::TransitionResource(
        ID3D12GraphicsCommandList* cmdList,
        ID3D12Resource*            resource,
        D3D12_RESOURCE_STATES      stateBefore,
        D3D12_RESOURCE_STATES      stateAfter,
        const char*                debugName
    )
    {
        if (!resource)
        {
            ERROR_AND_DIE("ResourceBarrier:: TransitionResource failed: resource is nullptr")
        }

        if (stateBefore == stateAfter)
        {
            core::LogWarn(LogRenderer, "ResourceBarrier:: TransitionResource skipped: stateBefore == stateAfter (0x%X), resource: %s", stateBefore, debugName ? debugName : "Unknown");
            return;
        }

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource   = resource;
        barrier.Transition.StateBefore = stateBefore;
        barrier.Transition.StateAfter  = stateAfter;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        cmdList->ResourceBarrier(1, &barrier);
    }

    void D3D12RenderSystem::UAVBarrier(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* resource)
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource          = resource;
        cmdList->ResourceBarrier(1, &barrier);
    }

    uint32_t D3D12RenderSystem::CreateUAV(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc)
    {
        if (!resource)
        {
            ERROR_RECOVERABLE("D3D12RenderSystem::CreateUAV: resource is nullptr");
            return BindlessIndexAllocator::INVALID_INDEX;
        }

        auto* allocator = GetBindlessIndexAllocator();
        auto* heapMgr   = GetGlobalDescriptorHeapManager();
        auto* device    = GetDevice();

        uint32_t index = allocator->AllocateTextureIndex();
        if (index == BindlessIndexAllocator::INVALID_INDEX)
        {
            ERROR_RECOVERABLE("D3D12RenderSystem::CreateUAV: failed to allocate bindless index");
            return BindlessIndexAllocator::INVALID_INDEX;
        }

        heapMgr->CreateUnorderedAccessView(device, resource, nullptr, &desc, index);
        return index;
    }

    void D3D12RenderSystem::BindVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW& bufferView, UINT slot)
    {
        if (!s_currentGraphicsCommandList)
        {
            core::LogError(LogRenderer,
                           "BindVertexBuffer: No active graphics command list (call BeginFrame first)");
            return;
        }
        s_currentGraphicsCommandList->IASetVertexBuffers(slot, 1, &bufferView);
    }

    void D3D12RenderSystem::BindIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& bufferView)
    {
        if (!s_currentGraphicsCommandList)
        {
            core::LogError(LogRenderer,
                           "BindIndexBuffer: No active graphics command list (call BeginFrame first)");
            return;
        }

        // Bind the cached index-buffer view.
        s_currentGraphicsCommandList->IASetIndexBuffer(&bufferView);
    }

    void D3D12RenderSystem::Draw(UINT vertexCount, UINT startVertex)
    {
        if (!s_currentGraphicsCommandList)
        {
            core::LogError(LogRenderer,
                           "Draw: No active graphics command list (call BeginFrame first)");
            return;
        }

        // Submit a single-instance draw.
        s_currentGraphicsCommandList->DrawInstanced(vertexCount, 1, startVertex, 0);
    }

    void D3D12RenderSystem::DrawIndexed(UINT indexCount, UINT startIndex, INT baseVertex)
    {
        if (!s_currentGraphicsCommandList)
        {
            core::LogError(LogRenderer,
                           "DrawIndexed: No active graphics command list (call BeginFrame first)");
            return;
        }

        // Submit a single-instance indexed draw.
        s_currentGraphicsCommandList->DrawIndexedInstanced(indexCount, 1, startIndex, baseVertex, 0);
    }

    void D3D12RenderSystem::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology)
    {
        if (!s_currentGraphicsCommandList)
        {
            core::LogError(LogRenderer,
                           "SetPrimitiveTopology: No active graphics command list (call BeginFrame first)");
            return;
        }

        // Update the IA primitive topology.
        s_currentGraphicsCommandList->IASetPrimitiveTopology(topology);
    }

    // ===== Frame execution API =====

    /**
     * Begin the current frame slot.
     *
     * Responsibilities:
     * - retire the slot by waiting on every tracked queue submission
     * - reset per-queue allocators for the slot
     * - acquire and initialize the main graphics command list
     *
     * It intentionally does not bind PSOs, render targets, or material resources.
     */
    bool D3D12RenderSystem::BeginFrame(const Rgba8& clearColor, float clearDepth, uint8_t clearStencil)
    {
        UNUSED(clearStencil)
        UNUSED(clearDepth)
        if (!s_isInitialized || !s_commandListManager)
        {
            LogError(LogRenderer, "D3D12RenderSystem not initialized for BeginFrame");
            return false;
        }

        ProcessDeferredResourceReleases();
        PrepareNextFrame();

        FrameContext&              ctx                 = s_frameContexts[s_frameIndex];
        const auto                 trackedTokens       = ctx.retirement.GetTrackedTokens();
        const auto                 expectedQueues      = ctx.retirement.expectedQueues;
        const auto                 participatingQueues = ctx.retirement.GetTrackedQueues();
        const auto                 missingQueues       = ctx.retirement.GetMissingQueues();
        FrameSlotAcquisitionResult acquisitionResult   = {};
        acquisitionResult.frameIndex                   = s_frameIndex;
        acquisitionResult.waitedFrameSlot              = s_frameIndex;
        acquisitionResult.hasTrackedRetirement         = ctx.retirement.HasTrackedSubmissions();
        acquisitionResult.requestedFramesInFlightDepth = GetRequestedFramesInFlightDepthSnapshot();
        acquisitionResult.activeFramesInFlightDepth    = GetActiveFramesInFlightDepthSnapshot();
        acquisitionResult.expectedQueues               = expectedQueues;
        acquisitionResult.participatingQueues          = participatingQueues;
        acquisitionResult.missingQueues                = missingQueues;
        acquisitionResult.graphicsToken                = trackedTokens[0];
        acquisitionResult.computeToken                 = trackedTokens[1];
        acquisitionResult.copyToken                    = trackedTokens[2];
        if (s_commandListManager)
        {
            acquisitionResult.completedFenceSnapshotBeforeWait = s_commandListManager->GetCompletedFenceSnapshot();
        }

        auto publishAcquisitionResult = [&](FrameLifecyclePhase phase)
        {
            acquisitionResult.lifecyclePhase                                 = phase;
            s_lastFrameSlotAcquisitionResult                                 = acquisitionResult;
            s_queueExecutionDiagnostics.lifecyclePhase                       = phase;
            s_queueExecutionDiagnostics.requestedFramesInFlightDepth         = acquisitionResult.requestedFramesInFlightDepth;
            s_queueExecutionDiagnostics.activeFramesInFlightDepth            = acquisitionResult.activeFramesInFlightDepth;
            s_queueExecutionDiagnostics.lastBeginFrameHadTrackedRetirement   = acquisitionResult.hasTrackedRetirement;
            s_queueExecutionDiagnostics.lastBeginFrameWaitedOnRetirement     = acquisitionResult.waitedOnRetirement;
            s_queueExecutionDiagnostics.lastWaitedFrameSlot                  = acquisitionResult.waitedFrameSlot;
            s_queueExecutionDiagnostics.lastCompletedFenceSnapshotBeforeWait =
                acquisitionResult.completedFenceSnapshotBeforeWait;
            s_queueExecutionDiagnostics.lastRetirementGraphicsToken       = acquisitionResult.graphicsToken;
            s_queueExecutionDiagnostics.lastRetirementComputeToken        = acquisitionResult.computeToken;
            s_queueExecutionDiagnostics.lastRetirementCopyToken           = acquisitionResult.copyToken;
            s_queueExecutionDiagnostics.lastRetirementParticipatingQueues = acquisitionResult.participatingQueues;
            s_queueExecutionDiagnostics.lastRetirementExpectedQueues      = acquisitionResult.expectedQueues;
            s_queueExecutionDiagnostics.lastRetirementMissingQueues       = acquisitionResult.missingQueues;
            s_queueExecutionDiagnostics.lastRetirementWaitedQueues        = acquisitionResult.waitedQueues;
        };

        publishAcquisitionResult(FrameLifecyclePhase::RetiringFrameSlot);
        SetFrameLifecyclePhase(FrameLifecyclePhase::RetiringFrameSlot);

        if (ctx.retirement.HasMissingRegistrations())
        {
            ERROR_RECOVERABLE(Stringf(
                "D3D12RenderSystem::BeginFrame detected missing frame retirement registration for slot %u while phase=%s (expected G=%u C=%u Copy=%u, tracked G=%u C=%u Copy=%u)",
                s_frameIndex,
                GetFrameLifecyclePhaseName(FrameLifecyclePhase::RetiringFrameSlot),
                expectedQueues[0] ? 1 : 0,
                expectedQueues[1] ? 1 : 0,
                expectedQueues[2] ? 1 : 0,
                participatingQueues[0] ? 1 : 0,
                participatingQueues[1] ? 1 : 0,
                participatingQueues[2] ? 1 : 0
            ));
        }

        try
        {
            for (const QueueSubmissionToken& token : trackedTokens)
            {
                if (!token.IsValid())
                {
                    continue;
                }

                const size_t   queueIndex          = GetCommandQueueTypeIndex(token.queueType);
                const uint64_t completedFenceValue =
                    acquisitionResult.completedFenceSnapshotBeforeWait.GetCompletedFenceValue(token.queueType);
                if (completedFenceValue < token.fenceValue)
                {
                    acquisitionResult.waitedQueues[queueIndex] = true;
                    acquisitionResult.waitedOnRetirement       = true;
                }

                if (!s_commandListManager->WaitForSubmission(token))
                {
                    throw FrameQueueRetirementException(
                        Stringf("D3D12RenderSystem::BeginFrame: failed to retire frame slot %u on %s queue at fence %llu",
                                s_frameIndex,
                                GetQueueTypeName(token.queueType),
                                token.fenceValue));
                }
            }
        }
        catch (const FrameQueueRetirementException& exception)
        {
            publishAcquisitionResult(FrameLifecyclePhase::Idle);
            SetFrameLifecyclePhase(FrameLifecyclePhase::Idle);
            ReportFatalQueueException(exception);
            return false;
        }

        ctx.ClearRetirement();

        auto resetFrameAllocator = [&](CommandQueueType queueType) -> bool
        {
            ID3D12CommandAllocator* allocator = ctx.GetCommandAllocator(queueType);
            if (!allocator)
            {
                LogError(LogRenderer,
                         "BeginFrame: FrameContext[%u] has no %s allocator",
                         s_frameIndex,
                         GetQueueTypeName(queueType));
                return false;
            }

            HRESULT hr = allocator->Reset();
            if (FAILED(hr))
            {
                LogError(LogRenderer,
                         "BeginFrame: Failed to reset %s allocator for frame slot %u, HRESULT=0x%08X",
                         GetQueueTypeName(queueType),
                         s_frameIndex,
                         hr);
                return false;
            }

            return true;
        };

        if (!resetFrameAllocator(CommandQueueType::Graphics) ||
            !resetFrameAllocator(CommandQueueType::Compute) ||
            !resetFrameAllocator(CommandQueueType::Copy))
        {
            publishAcquisitionResult(FrameLifecyclePhase::Idle);
            SetFrameLifecyclePhase(FrameLifecyclePhase::Idle);
            return false;
        }

        QueueRouteContext frameAcquireContext = {};
        frameAcquireContext.workload          = QueueWorkloadClass::FrameGraphics;

        s_currentGraphicsCommandList = AcquireCommandListForWorkload(
            frameAcquireContext,
            ctx.GetCommandAllocator(CommandQueueType::Graphics),
            "MainFrame Graphics Commands",
            true
        );

        if (!s_currentGraphicsCommandList)
        {
            LogError(LogRenderer, "Failed to acquire command list for BeginFrame");
            publishAcquisitionResult(FrameLifecyclePhase::Idle);
            SetFrameLifecyclePhase(FrameLifecyclePhase::Idle);
            return false;
        }

        if (s_globalDescriptorHeapManager)
        {
            s_globalDescriptorHeapManager->SetDescriptorHeaps(s_currentGraphicsCommandList);
        }
        else
        {
            LogWarn(LogRenderer,
                    "BeginFrame: GlobalDescriptorHeapManager is null - Bindless features may not work");
        }

        acquisitionResult.success = true;
        publishAcquisitionResult(FrameLifecyclePhase::FrameSlotAcquired);
        SetFrameLifecyclePhase(FrameLifecyclePhase::FrameSlotAcquired);
        RendererEvents::OnFrameSlotAcquired.Broadcast();
        SetFrameLifecyclePhase(FrameLifecyclePhase::RecordingFrame);

        ID3D12Resource* currentBackBuffer = GetCurrentSwapChainBuffer();
        if (!currentBackBuffer)
        {
            LogError(LogRenderer, "BeginFrame: Failed to get current SwapChain buffer");
            SetFrameLifecyclePhase(FrameLifecyclePhase::Idle);
            return false;
        }

        TransitionResource(s_currentGraphicsCommandList, currentBackBuffer,
                           D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET,
                           "SwapChain BackBuffer");

        auto currentRTV = GetCurrentSwapChainRTV();

        if (!ClearRenderTarget(s_currentGraphicsCommandList, &currentRTV, clearColor))
        {
            LogError(LogRenderer, "Failed to clear render target in BeginFrame");
            SetFrameLifecyclePhase(FrameLifecyclePhase::Idle);
            return false;
        }

        D3D12_VIEWPORT viewport = {};
        viewport.TopLeftX       = 0.0f;
        viewport.TopLeftY       = 0.0f;
        viewport.Width          = static_cast<float>(s_swapChainWidth);
        viewport.Height         = static_cast<float>(s_swapChainHeight);
        viewport.MinDepth       = 0.0f;
        viewport.MaxDepth       = 1.0f;
        s_currentGraphicsCommandList->RSSetViewports(1, &viewport);

        D3D12_RECT scissorRect = {};
        scissorRect.left       = 0;
        scissorRect.top        = 0;
        scissorRect.right      = static_cast<LONG>(s_swapChainWidth);
        scissorRect.bottom     = static_cast<LONG>(s_swapChainHeight);
        s_currentGraphicsCommandList->RSSetScissorRects(1, &scissorRect);

        /*LogInfo(LogRenderer,
                "BeginFrame: Viewport and ScissorRect set (%ux%u)",
                s_swapChainWidth, s_swapChainHeight);*/
        return true;
    }

    /**
     * End the frame, submit the main graphics list, and retire the slot by token.
     */
    bool D3D12RenderSystem::EndFrame()
    {
        if (!s_currentGraphicsCommandList)
        {
            LogError(LogRenderer,
                     "EndFrame: No active command list (BeginFrame not called?)");
            return false;
        }

        if (!s_swapChain)
        {
            LogError(LogRenderer,
                     "EndFrame: SwapChain not initialized");
            SetFrameLifecyclePhase(FrameLifecyclePhase::Idle);
            return false;
        }

        SetFrameLifecyclePhase(FrameLifecyclePhase::SubmittingFrame);

        ID3D12Resource* currentBackBuffer = GetCurrentSwapChainBuffer();
        if (!currentBackBuffer)
        {
            LogError(LogRenderer, "EndFrame: Failed to get current SwapChain buffer");
            SetFrameLifecyclePhase(FrameLifecyclePhase::Idle);
            return false;
        }
        TransitionResource(s_currentGraphicsCommandList, currentBackBuffer,
                           D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT,
                           "SwapChain BackBuffer");

        QueueRouteContext frameSubmitContext = {};
        frameSubmitContext.workload          = QueueWorkloadClass::FrameGraphics;

        QueueSubmissionToken submissionToken = SubmitFrameScopedCommandList(s_currentGraphicsCommandList, frameSubmitContext);
        if (!submissionToken.IsValid())
        {
            LogError(LogRenderer,
                     "EndFrame: Failed to execute command list");
            SetFrameLifecyclePhase(FrameLifecyclePhase::Idle);
            return false;
        }

        // 3. Present SwapChain
        if (!Present(g_theRendererSubsystem->GetConfiguration().enableVSync))
        {
            LogError(LogRenderer,
                     "EndFrame: Failed to present frame");
            SetFrameLifecyclePhase(FrameLifecyclePhase::Idle);
            return false;
        }

        // Slot retirement happens in BeginFrame, preserving CPU/GPU overlap.
        s_frameIndex = (s_frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
        s_globalFrameCount++;

        // 5. Recycle completed command list wrappers + release deferred resources
        // ProcessDeferredResourceReleases already calls UpdateCompletedCommandLists internally.
        ProcessDeferredResourceReleases();

        // 6. Clear current command list reference for next frame
        s_currentGraphicsCommandList = nullptr;
        SetFrameLifecyclePhase(FrameLifecyclePhase::Idle);

        return true;
    }

    /**
     * Clear a render target with either a caller-owned command list or a standalone submission.
     */
    bool D3D12RenderSystem::ClearRenderTarget(ID3D12GraphicsCommandList*         commandList,
                                              const D3D12_CPU_DESCRIPTOR_HANDLE* rtvHandle,
                                              const Rgba8&                       clearColor)
    {
        if (!s_isInitialized || !s_commandListManager)
        {
            core::LogError(LogRenderer, "D3D12RenderSystem not initialized for ClearRenderTarget");
            return false;
        }

        float clearColorAsFloats[4];
        clearColor.GetAsFloats(clearColorAsFloats);

        ID3D12GraphicsCommandList* actualCommandList = commandList;
        bool                       needToExecute     = false;

        if (!actualCommandList)
        {
            QueueRouteContext clearContext = {};
            clearContext.workload          = QueueWorkloadClass::ImmediateGraphics;

            actualCommandList = AcquireCommandListForWorkload(
                clearContext,
                nullptr,
                "ClearRenderTarget Command List",
                false
            );

            if (!actualCommandList)
            {
                core::LogError(LogRenderer, "Failed to acquire command list for ClearRenderTarget");
                return false;
            }
            needToExecute = true;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE actualRtvHandle = {};

        if (rtvHandle)
        {
            actualRtvHandle = *rtvHandle;
        }
        else
        {
            actualRtvHandle = GetCurrentSwapChainRTV();
            if (!GetCurrentSwapChainBuffer())
            {
                core::LogError(LogRenderer, "No valid swap-chain target for ClearRenderTarget");
                return false;
            }
        }

        // Resource-state transitions are caller-owned.
        // Callers must ensure the target is already in RENDER_TARGET state.
        actualCommandList->OMSetRenderTargets(1, &actualRtvHandle, FALSE, nullptr);

        actualCommandList->ClearRenderTargetView(actualRtvHandle, clearColorAsFloats, 0, nullptr);

        if (needToExecute)
        {
            QueueSubmissionToken submissionToken = SubmitStandaloneCommandList(actualCommandList,
                                                                               QueueWorkloadClass::ImmediateGraphics);
            if (!submissionToken.IsValid())
            {
                core::LogError(LogRenderer, "Failed to submit ClearRenderTarget command list");
                return false;
            }

            if (!WaitForStandaloneSubmission(submissionToken))
            {
                core::LogError(LogRenderer, "Failed to retire ClearRenderTarget command list");
                return false;
            }
        }

        return true;
    }

    // ===== Swap-chain management =====

    /**
     * Swap-chain buffers start in COMMON/PRESENT because both states resolve to zero.
     * We follow the standard DX12 flow:
     * CreateSwapChain -> BeginFrame transition to RENDER_TARGET -> EndFrame transition to PRESENT.
     * There is no explicit initialization barrier because a COMMON -> PRESENT barrier is a no-op.
     */

    /**
     * Create the swap chain and its RTV descriptors.
     */
    bool D3D12RenderSystem::CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height, uint32_t bufferCount)
    {
        if (!s_device || !s_dxgiFactory || !s_commandListManager)
        {
            LogError("D3D12RenderSystem", "Device or DXGI Factory not initialized for SwapChain creation");
            return false;
        }
#undef min
        s_swapChainBufferCount = std::min(bufferCount, 3u); // Clamp to the supported back-buffer count.

        // Cache swap-chain dimensions for BeginFrame viewport and scissor setup.
        s_swapChainWidth  = width;
        s_swapChainHeight = height;

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.BufferCount           = s_swapChainBufferCount;
        swapChainDesc.Width                 = width;
        swapChainDesc.Height                = height;
        swapChainDesc.Format                = s_backbufferFormat;
        swapChainDesc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.SampleDesc.Count      = 1;
        swapChainDesc.SampleDesc.Quality    = 0;
        swapChainDesc.Flags                 = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
        HRESULT                                 hr = s_dxgiFactory->CreateSwapChainForHwnd(
            s_commandListManager->GetCommandQueue(CommandListManager::Type::Graphics), // Swap-chain present remains graphics-queue owned.
            hwnd,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1
        );

        if (FAILED(hr))
        {
            LogError("D3D12RenderSystem", "Failed to create SwapChain");
            return false;
        }

        hr = swapChain1.As(&s_swapChain);
        if (FAILED(hr))
        {
            LogError("D3D12RenderSystem", "Failed to get SwapChain3 interface");
            return false;
        }

        // Disable the default Alt+Enter fullscreen toggle.
        s_dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

        if (!s_globalDescriptorHeapManager)
        {
            LogError("D3D12RenderSystem", "GlobalDescriptorHeapManager not available for RTV creation");
            return false;
        }

        for (uint32_t i = 0; i < s_swapChainBufferCount; ++i)
        {
            hr = s_swapChain->GetBuffer(i, IID_PPV_ARGS(&s_swapChainBuffers[i]));
            if (FAILED(hr))
            {
                LogError("D3D12RenderSystem", "Failed to get SwapChain buffer %d", i);
                return false;
            }

            auto rtvAllocation = s_globalDescriptorHeapManager->AllocateRtv();
            if (!rtvAllocation.isValid)
            {
                LogError("D3D12RenderSystem", "Failed to allocate RTV descriptor for SwapChain buffer %d", i);
                return false;
            }

            s_swapChainRTVs[i] = rtvAllocation.cpuHandle;

            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.Format                        = DXGI_FORMAT_R8G8B8A8_UNORM;
            rtvDesc.ViewDimension                 = D3D12_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice            = 0;

            s_device->CreateRenderTargetView(
                s_swapChainBuffers[i].Get(),
                &rtvDesc,
                s_swapChainRTVs[i]
            );

            std::string debugName = "SwapChain Buffer " + std::to_string(i);
            SetDebugName(s_swapChainBuffers[i].Get(), debugName.c_str());
        }

        s_currentBackBufferIndex = s_swapChain->GetCurrentBackBufferIndex();

        LogInfo("D3D12RenderSystem", "SwapChain created successfully: %dx%d, %d buffers", width, height, s_swapChainBufferCount);

        // No explicit swap-chain state bootstrap is required.
        // BeginFrame and EndFrame own the only meaningful transitions.

        return true;
    }

    /**
     * Get the RTV handle for the active swap-chain back buffer.
     */
    D3D12_CPU_DESCRIPTOR_HANDLE D3D12RenderSystem::GetCurrentSwapChainRTV()
    {
        return s_swapChainRTVs[s_currentBackBufferIndex];
    }

    /**
     * Get the resource for the active swap-chain back buffer.
     */
    ID3D12Resource* D3D12RenderSystem::GetCurrentSwapChainBuffer()
    {
        return s_swapChainBuffers[s_currentBackBufferIndex].Get();
    }

    /**
     * Present the current frame.
     */
    bool D3D12RenderSystem::Present(bool vsync)
    {
        if (!s_swapChain)
        {
            LogError("D3D12RenderSystem", "SwapChain not initialized");
            return false;
        }

        UINT syncInterval = vsync ? 1 : 0;
        UINT presentFlags = 0;

        HRESULT hr = s_swapChain->Present(syncInterval, presentFlags);
        if (FAILED(hr))
        {
            LogError("D3D12RenderSystem", "Failed to present frame");
            return false;
        }

        return true;
    }

    /**
     * Refresh the active back-buffer index before frame setup.
     */
    void D3D12RenderSystem::PrepareNextFrame()
    {
        if (s_swapChain)
        {
            s_currentBackBufferIndex = s_swapChain->GetCurrentBackBufferIndex();
        }
    }


    // ===== Texture-cache management =====

    /**
     * Remove expired entries from the texture cache.
     */
    void D3D12RenderSystem::ClearUnusedTextures()
    {
        std::lock_guard<std::mutex> lock(s_textureCacheMutex);

        size_t initialSize = s_textureCache.size();

        for (auto it = s_textureCache.begin(); it != s_textureCache.end();)
        {
            if (it->second.expired())
            {
                it = s_textureCache.erase(it);
            }
            else
            {
                ++it;
            }
        }

        size_t finalSize    = s_textureCache.size();
        size_t removedCount = initialSize - finalSize;

        if (removedCount > 0)
        {
            LogInfo(LogRenderer,
                    "ClearUnusedTextures: Removed %zu expired cache entries (%zu remaining)",
                    removedCount, finalSize);
        }
        else
        {
            LogDebug(LogRenderer,
                     "ClearUnusedTextures: No expired entries found (%zu total)",
                     finalSize);
        }
    }

    /**
     * Return the current texture-cache entry count.
     */
    size_t D3D12RenderSystem::GetTextureCacheSize()
    {
        std::lock_guard<std::mutex> lock(s_textureCacheMutex);
        return s_textureCache.size();
    }

    /**
     * Clear every texture-cache entry without forcing resource destruction.
     */
    void D3D12RenderSystem::ClearAllTextureCache()
    {
        std::lock_guard<std::mutex> lock(s_textureCacheMutex);

        size_t clearedCount = s_textureCache.size();
        s_textureCache.clear();

        if (clearedCount > 0)
        {
            LogInfo(LogRenderer,
                    "ClearAllTextureCache: Cleared entire cache (%zu entries removed)",
                    clearedCount);
        }
        else
        {
            LogDebug(LogRenderer,
                     "ClearAllTextureCache: Cache was already empty");
        }
    }

    // ============================================================================
    // Config-based Clear API - RenderTarget Format Refactor
    // ============================================================================

    void D3D12RenderSystem::ClearRenderTargetByConfig(
        ID3D12GraphicsCommandList*  commandList,
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
        const RenderTargetConfig&   config)
    {
        if (!commandList)
        {
            LogError(LogRenderer, "ClearRenderTargetByConfig: commandList is null");
            return;
        }

        // Only clear if loadAction is Clear
        if (config.loadAction != LoadAction::Clear)
        {
            return;
        }

        // Get clear color from config
        float clearColor[4];
        config.clearValue.GetColorAsFloats(clearColor);

        commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    }

    void D3D12RenderSystem::ClearDepthStencilByConfig(
        ID3D12GraphicsCommandList*  commandList,
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle,
        const RenderTargetConfig&   config)
    {
        if (!commandList)
        {
            LogError(LogRenderer, "ClearDepthStencilByConfig: commandList is null");
            return;
        }

        // Only clear if loadAction is Clear
        if (config.loadAction != LoadAction::Clear)
        {
            return;
        }

        // Get clear values from config
        float   clearDepth   = config.clearValue.depthStencil.depth;
        uint8_t clearStencil = config.clearValue.depthStencil.stencil;

        commandList->ClearDepthStencilView(
            dsvHandle,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            clearDepth,
            clearStencil,
            0,
            nullptr
        );
    }
} // namespace enigma::graphic
