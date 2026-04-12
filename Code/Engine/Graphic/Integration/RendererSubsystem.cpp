#include "RendererSubsystem.hpp"

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Core/ImGui/ImGuiSubsystem.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Integration/RendererEvents.hpp"
#include "Engine/Graphic/Mipmap/MipmapGenerator.hpp"
#include "Engine/Graphic/Resource/Buffer/D12VertexBuffer.hpp"
#include "Engine/Graphic/Resource/Buffer/D12IndexBuffer.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Target/RTTypes.hpp"
#include "Engine/Graphic/Target/RenderTargetHelper.hpp" // Render-target helper utilities
#include "Engine/Graphic/Target/RenderTargetBinder.hpp" // Unified RT binding
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformCommon.hpp" // Uniform exception hierarchy
#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp" // ENGINE_BUFFER_RING_CAPACITY
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp" // MatricesUniforms
#include "Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp" // PerObjectUniforms
#include "Engine/Graphic/Shader/Uniform/CustomImageManager.hpp" // Custom image slot manager
#include "Engine/Graphic/Shader/PSO/PSOManager.hpp" // PSO manager
#include "Engine/Graphic/Shader/PSO/RenderStateValidator.hpp" // Render-state validation
#include "Engine/Graphic/Camera/ICamera.hpp" // ICamera interface
#include "Engine/Graphic/Shader/Program/ShaderProgram.hpp" // ShaderProgram
#include "Engine/Graphic/Shader/Program/ShaderSource.hpp" // ShaderSource
#include "Engine/Graphic/Shader/Program/ShaderProgramBuilder.hpp" // ShaderProgramBuilder
#include "Engine/Graphic/Shader/Common/ShaderCompilationHelper.hpp" // Compilation helpers
#include "Engine/Graphic/Shader/Common/ShaderIncludeHelper.hpp" // Include helpers
#include "Engine/Graphic/Shader/Program/Include/ShaderPath.hpp" // ShaderPath abstraction
#include "Engine/Graphic/Resource/BindlessRootSignature.hpp"
#include "Engine/Graphic/Integration/RingBuffer/VertexRingBuffer.hpp" // Ring-buffer wrapper
#include "Engine/Graphic/Integration/RingBuffer/IndexRingBuffer.hpp"  // Ring-buffer wrapper
#include "Engine/Graphic/Resource/VertexLayout/VertexLayoutCommon.hpp"
#include "Engine/Graphic/Resource/VertexLayout/VertexLayoutRegistry.hpp" // VertexLayout state management
#include "Engine/Graphic/Shader/Uniform/CameraUniforms.hpp"
#include "Engine/Graphic/Target/IRenderTargetProvider.hpp"
#include "Engine/Graphic/Integration/RendererEvents.hpp" // Decoupled frame lifecycle events
enigma::graphic::RendererSubsystem* g_theRendererSubsystem = nullptr;

#pragma region Lifecycle Management
void RendererSubsystem::RenderStatistics::Reset()
{
    drawCalls            = 0;
    trianglesRendered    = 0;
    activeShaderPrograms = 0;
}

RendererSubsystem::RendererSubsystem(RendererSubsystemConfig& config)
{
    m_configuration = config;
}

RendererSubsystem::~RendererSubsystem()
{
    if (!m_isShutdown)
    {
        Shutdown();
    }
}

void RendererSubsystem::Initialize()
{
    LogInfo(LogRenderer, "Initializing D3D12 rendering system...");

    // Configuration is injected through the constructor. Initialize() only consumes it.

    // Resolve the window handle from the subsystem configuration.
    HWND hwnd = nullptr;
    if (m_configuration.targetWindow)
    {
        hwnd = static_cast<HWND>(m_configuration.targetWindow->GetWindowHandle());
        LogInfo(LogRenderer, "Window handle obtained from configuration for SwapChain creation");
    }
    else
    {
        LogWarn(LogRenderer, "No window provided in configuration - initializing in headless mode");
    }

    // Call the complete initialization of D3D12RenderSystem, including SwapChain creation
    bool success = D3D12RenderSystem::Initialize(
        m_configuration.enableDebugLayer, // Debug layer
        m_configuration.enableGPUValidation, // GPU validation
        hwnd, // Window handle for SwapChain
        m_configuration.renderWidth, // Render width
        m_configuration.renderHeight, // Render height
        m_configuration.backbufferFormat // Backbuffer format (configurable via renderer.yml)
    );
    if (!success)
    {
        LogError(LogRenderer, "Failed to initialize D3D12RenderSystem");
        m_isInitialized = false;
        return;
    }

    // Create engine-owned fallback textures before frontend services start.
    D3D12RenderSystem::PrepareDefaultTextures();

    m_isInitialized = true;
    m_isShutdown    = false;
    LogInfo(LogRenderer, "D3D12RenderSystem initialized successfully through RendererSubsystem");

    // Create PSOManager
    m_psoManager = std::make_unique<PSOManager>();
    LogInfo(LogRenderer, "PSOManager created successfully");

    g_theRendererSubsystem = this;
}

void RendererSubsystem::Startup()
{
    LogInfo(LogRenderer, "Starting up...");
    try
    {
        m_uniformManager = std::make_unique<UniformManager>();
        m_uniformManager->RegisterBuffer<MatricesUniforms>(7, UpdateFrequency::PerPass, BufferSpace::Engine, ENGINE_BUFFER_RING_CAPACITY);
        m_uniformManager->RegisterBuffer<CameraUniforms>(9, UpdateFrequency::PerFrame, BufferSpace::Engine, ENGINE_BUFFER_RING_CAPACITY);
        m_uniformManager->RegisterBuffer<ViewportUniforms>(10, UpdateFrequency::PerFrame, BufferSpace::Engine, ENGINE_BUFFER_RING_CAPACITY);
        m_uniformManager->RegisterBuffer<PerObjectUniforms>(1, UpdateFrequency::PerObject, BufferSpace::Engine, ENGINE_BUFFER_RING_CAPACITY);
        m_uniformManager->RegisterBuffer<CustomImageUniforms>(2, UpdateFrequency::PerPass, BufferSpace::Engine, ENGINE_BUFFER_RING_CAPACITY);
    }
    catch (const UniformException& e)
    {
        LogError(LogRenderer, "UniformManager initialization failed: %s", e.what());
        ERROR_AND_DIE(Stringf("UniformManager initialization failed: %s", e.what()))
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "UniformManager unexpected error: %s", e.what());
        ERROR_AND_DIE(Stringf("UniformManager construction failed! Error: %s", e.what()))
    }

    // ==================== Create ColorTextureProvider (with UniformManager) ====================
    // Read color-target settings from RendererSubsystemConfig instead of hardcoding them.
    try
    {
        LogInfo(LogRenderer, "Creating ColorTextureProvider...");

        // Always create the full Iris color-target set with config-driven formats.
        std::vector<RenderTargetConfig> colorConfigs = m_configuration.GetColorTexConfigs();

        const int baseWidth  = m_configuration.renderWidth;
        const int baseHeight = m_configuration.renderHeight;

        LogInfo(LogRenderer, "ColorTextureProvider configuration: %dx%d, %zu colortex",
                baseWidth, baseHeight, colorConfigs.size());

        m_colorTextureProvider = std::make_unique<ColorTextureProvider>(
            baseWidth, baseHeight, colorConfigs, m_uniformManager.get()
        );

        LogInfo(LogRenderer, "ColorTextureProvider created successfully (%zu colortex)", colorConfigs.size());
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "Failed to create ColorTextureProvider: %s", e.what());
        ERROR_AND_DIE(Stringf("ColorTextureProvider initialization failed! Error: %s", e.what()))
    }

    // ==================== Create CustomImageManager ====================
    // Initialize CustomImage Manager - manage 16 CustomImage slots
    try
    {
        LogInfo(LogRenderer, "Creating CustomImageManager...");

        // Create CustomImageManager with UniformManager dependency injection
        m_customImageManager = std::make_unique<CustomImageManager>(
            m_uniformManager.get(),
            [this]() -> bool
            {
                if (!m_graphicsPassScopeState.hasActiveScope)
                {
                    return false;
                }

                AdvancePassScope("CustomImageManager snapshot conflict");
                return true;
            }
        );

        LogInfo(LogRenderer, "CustomImageManager created successfully");
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "Failed to create CustomImageManager: %s", e.what());
        ERROR_AND_DIE(Stringf("CustomImageManager initialization failed! Error: %s", e.what()))
    }

    // ==================== Initialize VertexLayoutRegistry ====================
    // Static registry for VertexLayout management
    // Teaching points:
    // 1. Static class pattern (like D3D12RenderSystem)
    // 2. Registers predefined layouts (Vertex_PCU, Vertex_PCUTBN)
    // 3. Safe to call multiple times (logs warning on subsequent calls)
    VertexLayoutRegistry::Initialize();
    LogInfo(LogRenderer, "VertexLayoutRegistry initialized with predefined layouts");

    // ==================== Create DepthTextureProvider (with UniformManager) ====================
    // [REFACTOR] Load RenderTargetConfig from RendererSubsystemConfig instead of hardcoding
    try
    {
        LogInfo(LogRenderer, "Creating DepthTextureProvider...");

        // [REFACTOR] Get RenderTargetConfig from configuration (YAML or defaults)
        // Always creates MAX_DEPTH_TEXTURES (3) depthtex, format from config
        std::vector<RenderTargetConfig> depthConfigs = m_configuration.GetDepthTexConfigs();

        // [RAII] Pass UniformManager to constructor for Shader RT Fetching
        m_depthTextureProvider = std::make_unique<DepthTextureProvider>(
            m_configuration.renderWidth, m_configuration.renderHeight, depthConfigs, m_uniformManager.get()
        );

        LogInfo(LogRenderer, "DepthTextureProvider created successfully (%zu depthtex)", depthConfigs.size());
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "Failed to create DepthTextureProvider: %s", e.what());
        ERROR_AND_DIE(Stringf("DepthTextureProvider initialization failed! Error: %s", e.what()))
    }

    // ==================== Create ShadowColorProvider (with UniformManager) ====================
    // [REFACTOR] Load RenderTargetConfig from RendererSubsystemConfig instead of hardcoding
    try
    {
        LogInfo(LogRenderer, "Creating ShadowColorProvider...");

        // [REFACTOR] Get RenderTargetConfig from configuration (YAML or defaults)
        // Always creates MAX_SHADOW_COLORS (8) shadowcolor, format from config
        std::vector<RenderTargetConfig> shadowColorConfigs = m_configuration.GetShadowColorConfigs();

        // [RAII] Pass UniformManager to constructor for Shader RT Fetching
        // [FIX] Pass 0, 0 to use absolute dimensions from config (e.g., 2048x2048)
        // Shadow textures should NOT use screen resolution override
        m_shadowColorProvider = std::make_unique<ShadowColorProvider>(
            0, 0, shadowColorConfigs, m_uniformManager.get()
        );

        LogInfo(LogRenderer, "ShadowColorProvider created successfully (%zu shadowcolor)", shadowColorConfigs.size());
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "Failed to create ShadowColorProvider: %s", e.what());
        ERROR_AND_DIE(Stringf("ShadowColorProvider initialization failed! Error: %s", e.what()))
    }

    // ==================== Create ShadowTextureProvider (with UniformManager) ====================
    // [REFACTOR] Load RenderTargetConfig from RendererSubsystemConfig instead of hardcoding
    try
    {
        LogInfo(LogRenderer, "Creating ShadowTextureProvider...");

        // [REFACTOR] Get RenderTargetConfig from configuration (YAML or defaults)
        // Always creates MAX_SHADOW_TEXTURES (2) shadowtex, format from config
        std::vector<RenderTargetConfig> shadowTexConfigs = m_configuration.GetShadowTexConfigs();

        // [RAII] Pass UniformManager to constructor for Shader RT Fetching
        // [FIX] Pass 0, 0 to use absolute dimensions from config (e.g., 2048x2048)
        // Shadow textures should NOT use screen resolution override
        m_shadowTextureProvider = std::make_unique<ShadowTextureProvider>(
            0, 0, shadowTexConfigs, m_uniformManager.get()
        );

        LogInfo(LogRenderer, "ShadowTextureProvider created successfully (%zu shadowtex)", shadowTexConfigs.size());
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "Failed to create ShadowTextureProvider: %s", e.what());
        ERROR_AND_DIE(Stringf("ShadowTextureProvider initialization failed! Error: %s", e.what()))
    }

    try
    {
        LogInfo(LogRenderer, "Creating RenderTargetBinder...");

        m_renderTargetBinder = std::make_unique<RenderTargetBinder>(
            m_colorTextureProvider.get(), // IRenderTargetProvider* (ColorTexture)
            m_depthTextureProvider.get(), // IRenderTargetProvider* (DepthTexture)
            m_shadowColorProvider.get(), // IRenderTargetProvider* (ShadowColor)
            m_shadowTextureProvider.get() // IRenderTargetProvider* (ShadowTexture)
        );

        LogInfo(LogRenderer, "RenderTargetBinder created successfully");
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "Failed to create RenderTargetBinder: %s", e.what());
    }

    m_graphicsRootBinder = std::make_unique<GraphicsRootBinder>();

    // ==================== Create SamplerProvider (Dynamic Sampler System) ====================
    // Initialize SamplerProvider with 4 default samplers
    // Teaching points:
    // - sampler0: Linear (default texture sampling)
    // - sampler1: Point (pixel-perfect sampling)
    // - sampler2: Shadow (depth comparison)
    // - sampler3: PointWrap (tiled textures)
    try
    {
        LogInfo(LogRenderer, "Creating SamplerProvider...");

        // Get GlobalDescriptorHeapManager for sampler allocation
        auto* heapManager = D3D12RenderSystem::GetGlobalDescriptorHeapManager();
        if (!heapManager)
        {
            LogError(LogRenderer, "Failed to get GlobalDescriptorHeapManager for SamplerProvider");
            ERROR_AND_DIE("GlobalDescriptorHeapManager not available for SamplerProvider")
        }

        // Default sampler configurations (matches Static Sampler slots)
        std::vector<SamplerConfig> defaultSamplerConfigs = {
            SamplerConfig::Linear(), // sampler0: Linear filtering, wrap (tiling textures)
            SamplerConfig::Point(), // sampler1: Point filtering, clamp
            SamplerConfig::Shadow(), // sampler2: Shadow comparison
            SamplerConfig::PointWrap(), // sampler3: Point filtering, wrap
            SamplerConfig::LinearClamp(), // sampler4: Linear filtering, clamp (screen-space effects)
            SamplerConfig::PointMipLinearClamp() // sampler5: Point texel + linear mip (cutout LOD)
        };

        // [RAII] Create SamplerProvider with UniformManager dependency injection
        m_samplerProvider = std::make_unique<SamplerProvider>(
            *heapManager,
            defaultSamplerConfigs,
            m_uniformManager.get()
        );

        LogInfo(LogRenderer, "SamplerProvider created successfully (6 default samplers)");
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "Failed to create SamplerProvider: %s", e.what());
        ERROR_AND_DIE(Stringf("SamplerProvider initialization failed! Error: %s", e.what()))
    }

    // ==================== Create Immediate Mode RingBuffers ====================
    // RAII Ring Buffer wrappers encapsulate D12Buffer + offset state together
    // This fixes the mixed-stride issue by using BufferLocation byte offset instead of startVertex
    try
    {
        LogInfo(LogRenderer, "Creating Immediate Mode RingBuffers...");

        m_immediateVertexRingBuffer = std::make_unique<VertexRingBuffer>(
            RendererSubsystemConfig::INITIAL_IMMEDIATE_BUFFER_SIZE,
            sizeof(Vertex_PCU),
            "ImmediateVBO"
        );

        m_immediateIndexRingBuffer = std::make_unique<IndexRingBuffer>(
            RendererSubsystemConfig::INITIAL_IMMEDIATE_BUFFER_SIZE,
            "ImmediateIBO"
        );

        LogInfo(LogRenderer, "Immediate Mode RingBuffers created successfully");
    }
    catch (const RingBufferException& e)
    {
        LogError(LogRenderer, "Failed to create Immediate Mode RingBuffers: %s", e.what());
        ERROR_AND_DIE(Stringf("RingBuffer initialization failed! Error: %s", e.what()))
    }

    /// Full Screen Quads Renderer
    m_fullQuadsRenderer = std::make_unique<FullQuadsRenderer>();

    // Subscribe static services to pipeline events (before broadcast)
    MipmapGenerator::Initialize();

    // Broadcast: all core render systems are ready
    RendererEvents::OnPipelineReady.Broadcast();
}

void RendererSubsystem::Shutdown()
{
    if (m_isShutdown)
    {
        return;
    }

    m_isShutdown = true;
    LogInfo(LogRenderer, "Shutting down...");

    if (VertexLayoutRegistry::IsInitialized())
    {
        VertexLayoutRegistry::Shutdown();
        LogInfo(LogRenderer, "VertexLayoutRegistry shutdown complete");
    }

    ReleaseFrontendGpuResourcesBeforeBackendShutdown();

    LogInfo(LogRenderer, "All GPU resources released before D3D12 shutdown");

    if (D3D12RenderSystem::IsInitialized())
    {
        D3D12RenderSystem::Shutdown();
    }

    ResetFrontendShutdownState();
}

void RendererSubsystem::ReleaseFrontendGpuResourcesBeforeBackendShutdown() noexcept
{
    // Descriptor-owning frontend resources must die before backend teardown.
    m_samplerProvider.reset();
    m_customImageManager.reset();
    m_psoManager.reset();
    m_graphicsRootBinder.reset();
    m_fullQuadsRenderer.reset();
    m_blitProgram.reset();
    m_lastObservedGraphicsCommandList = nullptr;
    m_lastObservedSrvHeap             = nullptr;

    m_renderTargetBinder.reset();
    m_colorTextureProvider.reset();
    m_depthTextureProvider.reset();
    m_shadowColorProvider.reset();
    m_shadowTextureProvider.reset();

    m_immediateVertexRingBuffer.reset();
    m_immediateIndexRingBuffer.reset();

    // UniformManager stays last because providers may still reference it.
    m_uniformManager.reset();
}

void RendererSubsystem::ResetFrontendShutdownState() noexcept
{
    g_theRendererSubsystem = nullptr;

    m_currentShaderProgram       = nullptr;
    m_currentBlendMode           = BlendMode::Opaque;
    m_currentBlendConfig         = BlendConfig::Opaque();
    m_hasIndependentBlend        = false;
    m_perRTBlendConfigs.fill(BlendConfig::Opaque());
    m_currentDepthConfig         = DepthConfig::Enabled();
    m_currentStencilTest         = StencilTestDetail::Disabled();
    m_currentRasterizationConfig = RasterizationConfig::CullBack();
    m_currentVertexLayout        = nullptr;
    m_lastBoundPSO               = nullptr;
    m_currentStencilRef          = 0;
    m_graphicsPassScopeState.hasActiveScope = false;
    m_viewportUniforms           = {};
    m_renderStatistics.Reset();

    m_isInitialized = false;
    m_isStarted     = false;
}

// TODO: M2 - remove PreparePipeline and keep the flexible rendering API only.
#pragma endregion

#pragma region Shader Compilation
/**
 * @brief 检查渲染系统是否准备好进行渲染
 * @return 如果D3D12RenderSystem已初始化且设备可用则返回true
 *
 * 教学要点: 通过检查底层API封装层的状态确定渲染系统就绪状态
 */
bool RendererSubsystem::IsReadyForRendering() const noexcept
{
    // 通过D3D12RenderSystem静态API检查设备状态
    return D3D12RenderSystem::IsInitialized() && D3D12RenderSystem::GetDevice() != nullptr;
}

void RendererSubsystem::BeginFrame()
{
    // Broadcast the CPU-side pre-frame notification before the DX12 frame slot
    // is acquired. This is not a resource-ownership boundary.
    D3D12RenderSystem::SetFrameLifecyclePhase(FrameLifecyclePhase::PreFrameBegin);
    RendererEvents::OnBeginFrame.Broadcast();

    // Reset CPU-only binding and layout caches before the DX12 frame begins.
    m_currentVertexLayout = VertexLayoutRegistry::GetDefault();

    // Clear stale CPU-side PSO and render-target cache state at the frame boundary.
    m_lastBoundPSO = nullptr;
    if (m_renderTargetBinder)
    {
        m_renderTargetBinder->ClearBindings();
    }

    const bool success = D3D12RenderSystem::BeginFrame(
        m_configuration.defaultClearColor,
        m_configuration.defaultClearDepth,
        m_configuration.defaultClearStencil
    );

    if (!success)
    {
        LogWarn(LogRenderer, "BeginFrame - D3D12 frame setup failed");
        return;
    }

    // Reset frame-local resource state only after the active frame slot has
    // been retired, reset, and made safe for reuse by D3D12RenderSystem.
    if (m_immediateVertexRingBuffer)
    {
        m_immediateVertexRingBuffer->ResetForFrame();
    }
    if (m_immediateIndexRingBuffer)
    {
        m_immediateIndexRingBuffer->ResetForFrame();
    }

    if (m_uniformManager)
    {
        m_uniformManager->ResetDrawCount();
        m_uniformManager->ResetPassScopesForFrame();
    }

    m_graphicsPassScopeState.hasActiveScope = false;

    // Keep the remaining frame-local shader-visible activation work in one
    // post-acquire block so it is aligned with safe slot ownership.
    if (m_customImageManager)
    {
        m_customImageManager->OnFrameSlotAcquired();
    }

    m_colorTextureProvider->UpdateIndices();
    m_depthTextureProvider->UpdateIndices();
    m_shadowColorProvider->UpdateIndices();
    m_shadowTextureProvider->UpdateIndices();
    if (m_samplerProvider)
    {
        m_samplerProvider->UpdateIndices();
    }

    m_viewportUniforms.viewWidth   = (float)m_configuration.renderWidth;
    m_viewportUniforms.viewHeight  = (float)m_configuration.renderHeight;
    m_viewportUniforms.aspectRatio = m_viewportUniforms.viewWidth / m_viewportUniforms.viewHeight;
    m_uniformManager->UploadBuffer(m_viewportUniforms);

    if (m_configuration.enableAutoClearColor)
    {
        // Clear GBuffer render targets after the swap-chain back buffer has
        // already been prepared by D3D12RenderSystem::BeginFrame().
        ClearAllRenderTargets();
        LogDebug(LogRenderer, "BeginFrame - All render targets cleared");
    }

    SyncGraphicsRootBinderWithCommandList(D3D12RenderSystem::GetCurrentCommandList(), true);

    auto* heapManager = D3D12RenderSystem::GetGlobalDescriptorHeapManager();
    m_lastObservedSrvHeap = heapManager ? heapManager->GetCbvSrvUavHeap() : nullptr;
}

void RendererSubsystem::BeginPassScope(const char* debugName)
{
    if (!m_uniformManager)
    {
        ERROR_RECOVERABLE("RendererSubsystem::BeginPassScope called without UniformManager");
        return;
    }

    if (m_graphicsPassScopeState.hasActiveScope)
    {
        if (debugName)
        {
            ERROR_RECOVERABLE(Stringf("RendererSubsystem::BeginPassScope called while a scope is already active (%s)", debugName));
        }
        else
        {
            ERROR_RECOVERABLE("RendererSubsystem::BeginPassScope called while a scope is already active");
        }
        return;
    }

    m_uniformManager->AdvancePassScope();
    m_graphicsPassScopeState.hasActiveScope = true;

    if (m_customImageManager)
    {
        m_customImageManager->OnPassScopeChanged();
    }
}

void RendererSubsystem::AdvancePassScope(const char* debugName)
{
    if (!m_uniformManager)
    {
        ERROR_RECOVERABLE("RendererSubsystem::AdvancePassScope called without UniformManager");
        return;
    }

    if (!m_graphicsPassScopeState.hasActiveScope)
    {
        if (debugName)
        {
            ERROR_RECOVERABLE(Stringf("RendererSubsystem::AdvancePassScope called without an active scope (%s)", debugName));
        }
        else
        {
            ERROR_RECOVERABLE("RendererSubsystem::AdvancePassScope called without an active scope");
        }
        return;
    }

    m_uniformManager->AdvancePassScope();

    if (m_customImageManager)
    {
        m_customImageManager->OnPassScopeChanged();
    }
}

void RendererSubsystem::EndPassScope()
{
    if (!m_graphicsPassScopeState.hasActiveScope)
    {
        ERROR_RECOVERABLE("RendererSubsystem::EndPassScope called without an active scope");
        return;
    }

    m_graphicsPassScopeState.hasActiveScope = false;
}

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
void RendererSubsystem::EndFrame()
{
    D3D12RenderSystem::EndFrame();
}
#pragma endregion

#pragma region Shader Program

std::shared_ptr<ShaderProgram> RendererSubsystem::CreateShaderProgramFromFiles(
    const std::filesystem::path& vsPath,
    const std::filesystem::path& psPath,
    const std::string&           programName
)
{
    ShaderCompileOptions options = ShaderCompileOptions::WithCommonInclude();
    options.entryPoint           = m_configuration.shaderEntryPoint;

    LogDebug(LogRenderer, "Using configured entry point: {}",
             options.entryPoint.c_str());

    return CreateShaderProgramFromFiles(
        vsPath,
        psPath,
        programName,
        options
    );
}

std::shared_ptr<ShaderProgram> RendererSubsystem::CreateShaderProgramFromSource(
    const std::string&          vsSource,
    const std::string&          psSource,
    const std::string&          programName,
    const ShaderCompileOptions& options
)
{
    LogInfo(LogRenderer, "Compiling shader program from source: %s", programName.c_str());

    // ========================================================================
    // Step 1: 创建 ShaderSource（自动解析 ProgramDirectives）
    // ========================================================================
    ShaderSource shaderSource(
        programName,
        vsSource,
        psSource,
        "", // geometrySource（可选）
        "", // hullSource（可选）
        "", // domainSource（可选）
        "", // computeSource（可选）
        nullptr // parent（可为 nullptr）
    );

    // ========================================================================
    // Step 2: 验证 ShaderSource
    // ========================================================================
    if (!shaderSource.IsValid())
    {
        LogError(LogRenderer, "Invalid ShaderSource (missing VS or PS): %s",
                 programName.c_str());
        ERROR_AND_DIE(Stringf("Invalid ShaderSource (missing VS or PS): %s",
            programName.c_str()))
    }

    if (!shaderSource.HasNonEmptySource())
    {
        LogError(LogRenderer, "ShaderSource contains only whitespace: %s",
                 programName.c_str());
        ERROR_AND_DIE(Stringf("ShaderSource contains only whitespace: %s",
            programName.c_str()))
    }

    // ========================================================================
    // Step 3: 使用 ShaderProgramBuilder 编译（Shrimp Task 3: 支持自定义编译选项）
    // ========================================================================
    auto buildResult = ShaderProgramBuilder::BuildProgram(
        shaderSource,
        ShaderType::GBuffers_Terrain,
        options //  传递用户自定义编译选项
    );

    if (!buildResult.success)
    {
        LogError(LogRenderer, "Failed to build shader program: %s\nError: %s",
                 programName.c_str(),
                 buildResult.errorMessage.c_str());
        ERROR_AND_DIE(Stringf("Failed to build shader program: %s\nError: %s",
            programName.c_str(),
            buildResult.errorMessage.c_str()))
    }


    // ========================================================================
    // Step 5: 创建 ShaderProgram
    // ========================================================================
    auto program = std::make_shared<ShaderProgram>();
    program->Create(
        std::move(*buildResult.vertexShader),
        std::move(*buildResult.pixelShader),
        buildResult.geometryShader ? std::make_optional(std::move(*buildResult.geometryShader)) : std::nullopt,
        buildResult.computeShader ? std::make_optional(std::move(*buildResult.computeShader)) : std::nullopt,
        ShaderType::GBuffers_Terrain,
        buildResult.directives // Use parsed Directives
    );

    LogInfo(LogRenderer, "Successfully compiled shader program from source: %s", programName.c_str());
    return program;
}

std::shared_ptr<ShaderProgram> RendererSubsystem::CreateShaderProgramFromFiles(
    const std::filesystem::path& vsPath,
    const std::filesystem::path& psPath,
    const std::string&           programName,
    const ShaderCompileOptions&  options
)
{
    // ========================================================================
    // Step 1: 读取着色器源码
    // ========================================================================
    auto vsSourceOpt = ShaderCompilationHelper::ReadShaderSourceFromFile(vsPath);
    if (!vsSourceOpt)
    {
        LogError(LogRenderer, "Failed to read vertex shader file: %s",
                 vsPath.string().c_str());
        ERROR_AND_DIE(Stringf("Failed to read vertex shader file: %s",
            vsPath.string().c_str()))
    }

    auto psSourceOpt = ShaderCompilationHelper::ReadShaderSourceFromFile(psPath);
    if (!psSourceOpt)
    {
        LogError(LogRenderer, "Failed to read pixel shader file: %s",
                 psPath.string().c_str());
        ERROR_AND_DIE(Stringf("Failed to read pixel shader file: %s",
            psPath.string().c_str()))
    }

    std::string vsSource = *vsSourceOpt;
    std::string psSource = *psSourceOpt;

    // ========================================================================
    // Step 2: 自动检测是否包含#include指令
    // ========================================================================
    bool hasIncludes = (vsSource.find("#include") != std::string::npos) ||
        (psSource.find("#include") != std::string::npos);

    // ========================================================================
    // Step 3: 如果包含#include，使用Include系统展开
    // ========================================================================
    if (hasIncludes)
    {
        LogInfo(LogRenderer, "Detected #include directives in shader files, using Include system");

        try
        {
            // 3.1 确定根目录（从shader文件路径推断）
            std::filesystem::path rootPath = ShaderIncludeHelper::DetermineRootPath(vsPath);
            LogDebug(LogRenderer, "Include system root path: {}", rootPath.string().c_str());

            // 3.2 将相对路径转换为绝对路径
            std::filesystem::path vsAbsPath = std::filesystem::absolute(vsPath);
            std::filesystem::path psAbsPath = std::filesystem::absolute(psPath);

            // 3.3 计算相对于根目录的路径
            std::filesystem::path vsRelPath = std::filesystem::relative(vsAbsPath, rootPath);
            std::filesystem::path psRelPath = std::filesystem::relative(psAbsPath, rootPath);

            // 3.4 构建IncludeGraph（使用完整相对路径，添加前导斜杠）
            std::vector<std::string> shaderFiles = {
                "/" + vsRelPath.generic_string(),
                "/" + psRelPath.generic_string()
            };

            auto graph = ShaderIncludeHelper::BuildFromFiles(rootPath, shaderFiles);
            if (!graph)
            {
                LogError(LogRenderer, "Failed to build IncludeGraph for shader files");
                ERROR_AND_DIE("Failed to build IncludeGraph for shader files")
            }

            // 3.3 检查Include构建失败
            if (!graph->GetFailures().empty())
            {
                LogWarn(LogRenderer, "IncludeGraph has %d failures:", graph->GetFailures().size());
                for (const auto& [path, error] : graph->GetFailures())
                {
                    LogWarn(LogRenderer, "  - %s: %s", path.GetPathString().c_str(), error.c_str());
                }
            }

            // 3.4 展开VS源码
            ShaderPath vsShaderPath = ShaderPath::FromAbsolutePath("/" + vsRelPath.generic_string());
            if (graph->HasNode(vsShaderPath))
            {
                vsSource = ShaderIncludeHelper::ExpandShaderSource(
                    *graph,
                    vsShaderPath,
                    options.enableDebugInfo // 调试模式使用LineDirectives
                );
                LogDebug(LogRenderer, "Expanded VS source with Include system ({} bytes)", vsSource.size());
            }
            else
            {
                LogWarn(LogRenderer, "VS file not found in IncludeGraph, using original source");
            }

            // 3.5 展开PS源码
            ShaderPath psShaderPath = ShaderPath::FromAbsolutePath("/" + psRelPath.generic_string());
            if (graph->HasNode(psShaderPath))
            {
                psSource = ShaderIncludeHelper::ExpandShaderSource(
                    *graph,
                    psShaderPath,
                    options.enableDebugInfo // 调试模式使用LineDirectives
                );
                LogDebug(LogRenderer, "Expanded PS source with Include system ({} bytes)", psSource.size());
            }
            else
            {
                LogWarn(LogRenderer, "PS file not found in IncludeGraph, using original source");
            }
        }
        catch (const std::exception& e)
        {
            LogError(LogRenderer, "Include system expansion failed: ", e.what());
            ERROR_AND_DIE(Stringf("Include system expansion failed: %s", e.what()))
        }
    }
    else
    {
        LogDebug(LogRenderer, "No #include directives detected, using original shader source");
    }

    std::string finalProgramName = programName.empty() ? ShaderCompilationHelper::ExtractProgramNameFromPath(vsPath) : programName;

    // ========================================================================
    // Step 5: 调用CreateShaderProgramFromSource编译
    // ========================================================================
    return CreateShaderProgramFromSource(
        vsSource,
        psSource,
        finalProgramName,
        options
    );
}

#pragma endregion

#pragma region Rendering API
// TODO: M2 - 实现60+ API灵活渲染接口
// BeginCamera/EndCamera, UseProgram, DrawVertexBuffer, etc.
// 替代旧的Phase系统（已删除IWorldRenderingPipeline/PipelineManager）
#pragma endregion

#pragma region RenderTarget Management
// TODO: M2 - 实现RenderTarget管理接口
// ConfigureGBuffer, FlipRenderTarget, SwitchDepthBuffer等
#pragma endregion

#pragma region State Management

ID3D12CommandQueue* RendererSubsystem::GetCommandQueue() const noexcept
{
    auto* cmdMgr = D3D12RenderSystem::GetCommandListManager();
    return cmdMgr ? cmdMgr->GetCommandQueue(CommandListManager::Type::Graphics) : nullptr;
}

void RendererSubsystem::SetRequestedQueueExecutionMode(QueueExecutionMode mode) noexcept
{
    D3D12RenderSystem::SetRequestedQueueExecutionMode(mode);
}

QueueExecutionMode RendererSubsystem::GetRequestedQueueExecutionMode() const noexcept
{
    return D3D12RenderSystem::GetRequestedQueueExecutionMode();
}

QueueExecutionMode RendererSubsystem::GetActiveQueueExecutionMode() const noexcept
{
    return D3D12RenderSystem::GetActiveQueueExecutionMode();
}

const QueueExecutionDiagnostics& RendererSubsystem::GetQueueExecutionDiagnostics() const noexcept
{
    return D3D12RenderSystem::GetQueueExecutionDiagnostics();
}

//-----------------------------------------------------------------------------------------------
// ImGui Integration Support (7 getter methods for IImGuiRenderContext)
//-----------------------------------------------------------------------------------------------

ID3D12GraphicsCommandList* RendererSubsystem::GetCurrentCommandList() const noexcept
{
    ID3D12GraphicsCommandList* commandList = D3D12RenderSystem::GetCurrentCommandList();
    SyncGraphicsRootBinderWithCommandList(commandList, true);
    return commandList;
}

ID3D12DescriptorHeap* RendererSubsystem::GetSRVHeap() const noexcept
{
    auto* heapManager = D3D12RenderSystem::GetGlobalDescriptorHeapManager();
    ID3D12DescriptorHeap* srvHeap = heapManager ? heapManager->GetCbvSrvUavHeap() : nullptr;
    SyncGraphicsRootBinderWithSrvHeap(srvHeap);
    return srvHeap;
}

void RendererSubsystem::NotifyGraphicsDescriptorHeapContextChanged() noexcept
{
    auto* heapManager = D3D12RenderSystem::GetGlobalDescriptorHeapManager();
    m_lastObservedSrvHeap = heapManager ? heapManager->GetCbvSrvUavHeap() : nullptr;

    if (m_graphicsRootBinder)
    {
        m_graphicsRootBinder->InvalidateDescriptorTables();
    }
}

void RendererSubsystem::InvalidateGraphicsRootBindings() noexcept
{
    if (m_graphicsRootBinder)
    {
        m_graphicsRootBinder->InvalidateAll();
    }
}

DXGI_FORMAT RendererSubsystem::GetRTVFormat() const noexcept
{
    // SwapChain后台缓冲区固定使用RGBA8格式
    return DXGI_FORMAT_R8G8B8A8_UNORM;
}

uint32_t RendererSubsystem::GetFramesInFlight() const noexcept
{
    // 返回SwapChain缓冲区数量（通常为2或3）
    // 默认为2（双缓冲），可从配置读取
    return 2;
}

bool RendererSubsystem::IsInitialized() const noexcept
{
    return m_isInitialized;
}

void RendererSubsystem::SetCustomImage(int slotIndex, D12Texture* texture)
{
    // Delegate slot updates to the dedicated custom image manager.

    if (m_customImageManager)
    {
        m_customImageManager->SetCustomImage(slotIndex, texture);
    }
    else
    {
        LogWarn(LogRenderer, "SetCustomImage: CustomImageManager is not initialized");
    }
}

D12Texture* RendererSubsystem::GetCustomImage(int slotIndex) const
{
    // Delegate slot queries to the dedicated custom image manager.

    if (m_customImageManager)
    {
        return m_customImageManager->GetCustomImage(slotIndex);
    }

    LogWarn(LogRenderer, "GetCustomImage: CustomImageManager is not initialized");
    return nullptr;
}

void RendererSubsystem::ClearCustomImage(int slotIndex)
{
    // Delegate slot clearing to the dedicated custom image manager.

    if (m_customImageManager)
    {
        m_customImageManager->ClearCustomImage(slotIndex);
    }
    else
    {
        LogWarn(LogRenderer, "ClearCustomImage: CustomImageManager is not initialized");
    }
}

std::shared_ptr<D12Texture> RendererSubsystem::CreateTexture2D(int width, int height, DXGI_FORMAT format, const void* initialData)
{
    // [DELEGATION] 委托给D3D12RenderSystem创建纹理
    // 教学要点：
    // - 使用unique_ptr::release()转移所有权
    // - 返回裸指针，调用者负责管理生命周期
    // - 建议调用者使用智能指针管理返回的纹理

    // 1. 调用D3D12RenderSystem创建纹理（返回unique_ptr）
    return D3D12RenderSystem::CreateTexture2D(width, height, format, initialData);
}

std::shared_ptr<D12Texture> RendererSubsystem::CreateTexture2D(const class ResourceLocation& resourceLocation, TextureUsage usage, const std::string& debugName)
{
    return D3D12RenderSystem::CreateTexture2D(resourceLocation, usage, debugName);
}

std::shared_ptr<D12Texture> RendererSubsystem::CreateTexture2D(const std::string& imagePath, TextureUsage usage, const std::string& debugName)
{
    auto texture = D3D12RenderSystem::CreateTexture2D(imagePath, usage, debugName);
    return texture;
}

//-----------------------------------------------------------------------------------------------
// M2 灵活渲染接口实现 (Milestone 2)
// Teaching Note: 提供高层API，封装DirectX 12复杂性
// Reference: DX11Renderer.cpp中的类似方法
//-----------------------------------------------------------------------------------------------
// M6.2.1: UseProgram RT binding (pair-based API)
//-----------------------------------------------------------------------------------------------

void RendererSubsystem::UseProgram(
    std::shared_ptr<ShaderProgram>                       shaderProgram,
    const std::vector<std::pair<RenderTargetType, int>>& targets
)
{
    if (!shaderProgram)
    {
        LogError(LogRenderer, "UseProgram: shaderProgram is nullptr");
        return;
    }

    // Cache current ShaderProgram for subsequent Draw() calls
    m_currentShaderProgram = shaderProgram.get();

    auto cmdList = GetCurrentCommandList();
    if (!cmdList)
    {
        LogError(LogRenderer, "UseProgram: CommandList is nullptr");
        return;
    }

    if (!targets.empty())
    {
        // Bind specified RenderTargets using pair-based API
        if (m_renderTargetBinder)
        {
            m_renderTargetBinder->BindRenderTargets(targets);
            m_renderTargetBinder->FlushBindings(cmdList);
        }
    }
    else
    {
        LogDebug(LogRenderer, "UseProgram: Bound SwapChain as default RT");
    }

    LogDebug(LogRenderer, "UseProgram: ShaderProgram cached, RenderTargets bound (PSO deferred to Draw)");
}

//-----------------------------------------------------------------------------------------------
// GetProvider - Access RT provider for dynamic configuration
//-----------------------------------------------------------------------------------------------

IRenderTargetProvider* RendererSubsystem::GetRenderTargetProvider(RenderTargetType rtType)
{
    if (!m_renderTargetBinder)
    {
        LogError(LogRenderer, "GetProvider: RenderTargetBinder is nullptr");
        return nullptr;
    }

    return m_renderTargetBinder->GetProvider(rtType);
}

// BeginCamera - ICamera interface version
void RendererSubsystem::BeginCamera(const ICamera& camera)
{
    MatricesUniforms matricesUniforms;
    camera.UpdateMatrixUniforms(matricesUniforms);
    m_uniformManager->UploadBuffer<MatricesUniforms>(matricesUniforms);
    m_uniformManager->UploadBuffer<CameraUniforms>(camera.GetCameraUniforms());
}

void RendererSubsystem::EndCamera(const ICamera& camera)
{
    UNUSED(camera)
    LogWarn(LogRenderer, "EndCamera:: Not implemented yet");
}

D12VertexBuffer* RendererSubsystem::CreateVertexBuffer(size_t size, unsigned stride)
{
    if (size == 0 || stride == 0)
    {
        LogError(LogRenderer,
                 "CreateVertexBuffer: Invalid parameters (size: {}, stride: {})",
                 size, stride);
        return nullptr;
    }

    BufferCreateInfo createInfo;
    createInfo.size         = size;
    createInfo.usage        = BufferUsage::VertexBuffer;
    createInfo.memoryAccess = MemoryAccess::CPUWritable;
    createInfo.initialData  = nullptr;
    createInfo.debugName    = "AppVertexBuffer";
    createInfo.byteStride   = stride;
    createInfo.usagePolicy  = BufferUsagePolicy::DynamicUpload;

    auto vertexBuffer = D3D12RenderSystem::CreateVertexBuffer(createInfo);

    if (!vertexBuffer)
    {
        LogError(LogRenderer,
                 "CreateVertexBuffer: Failed to create D12VertexBuffer (size: {}, stride: {})",
                 size, stride);
        return nullptr;
    }

    LogInfo(LogRenderer,
            "CreateVertexBuffer: Successfully created D12VertexBuffer (size: {}, stride: {}, count: {})",
            size, stride, size / stride);

    // 返回裸指针，转移所有权给调用者
    return vertexBuffer.release();
}

void RendererSubsystem::SetVertexBuffer(D12VertexBuffer* buffer, uint32_t slot)
{
    if (!buffer)
    {
        LogError(LogRenderer, "SetVertexBuffer: D12VertexBuffer is nullptr");
        return;
    }

    // 教学要点：
    // - 直接调用D3D12RenderSystem::BindVertexBuffer()的D12VertexBuffer重载
    // - D3D12RenderSystem内部会提取D3D12_VERTEX_BUFFER_VIEW并绑定
    // - 无需手动访问View（封装性更好）

    // 委托给D3D12RenderSystem的底层API（D12VertexBuffer重载）
    D3D12RenderSystem::BindVertexBuffer(buffer, slot);

    LogDebug(LogRenderer,
             "SetVertexBuffer: Bound D12VertexBuffer to slot {} (size: {}, stride: {}, count: {})",
             slot, buffer->GetSize(), buffer->GetStride(), buffer->GetVertexCount());
}

void RendererSubsystem::SetIndexBuffer(D12IndexBuffer* buffer)
{
    if (!buffer)
    {
        LogError(LogRenderer, "SetIndexBuffer: D12IndexBuffer is nullptr");
        return;
    }

    // 教学要点：
    // - 直接调用D3D12RenderSystem::BindIndexBuffer()的D12IndexBuffer重载
    // - D3D12RenderSystem内部会提取D3D12_INDEX_BUFFER_VIEW并绑定
    // - IndexBuffer只有一个槽位（与VertexBuffer不同）
    // - 无需手动访问View（封装性更好）

    // 委托给D3D12RenderSystem的底层API（D12IndexBuffer重载）
    D3D12RenderSystem::BindIndexBuffer(buffer);

    LogDebug(LogRenderer,
             "SetIndexBuffer: Bound D12IndexBuffer (size: {}, count: {}, format: {})",
             buffer->GetSize(),
             buffer->GetIndexCount());
}

void RendererSubsystem::UpdateBuffer(D12VertexBuffer* buffer, const void* data, size_t size, size_t offset)
{
    if (!data || !buffer)
    {
        LogError(LogRenderer, "UpdateBuffer: Invalid parameters (data or buffer is nullptr)");
        return;
    }

    if (size == 0)
    {
        LogWarn(LogRenderer, "UpdateBuffer: Size is 0, nothing to update");
        return;
    }

    if (offset + size > buffer->GetSize())
    {
        LogError(LogRenderer,
                 "UpdateBuffer: Data exceeds buffer size (offset: {}, size: {}, buffer size: {})",
                 offset, size, buffer->GetSize());
        return;
    }

    if (!buffer->SupportsCpuMapping())
    {
        LogError(LogRenderer,
                 "UpdateBuffer: Buffer '%s' does not support CPU mapping",
                 buffer->GetDebugName().c_str());
        return;
    }

    void* mappedPtr = buffer->Map(nullptr);
    if (!mappedPtr)
    {
        LogError(LogRenderer, "UpdateBuffer: Failed to map D12VertexBuffer");
        return;
    }

    memcpy(static_cast<uint8_t*>(mappedPtr) + offset, data, size);

    D3D12_RANGE writtenRange = {offset, offset + size};
    buffer->Unmap(&writtenRange);

    LogDebug(LogRenderer,
             "UpdateBuffer: Updated {} bytes at offset {} (total size: {})",
             size, offset, buffer->GetSize());
}

void RendererSubsystem::SyncGraphicsRootBinderWithCommandList(
    ID3D12GraphicsCommandList* commandList,
    bool                       resetDiagnostics) const noexcept
{
    if (commandList == m_lastObservedGraphicsCommandList)
    {
        return;
    }

    m_lastObservedGraphicsCommandList = commandList;

    if (!m_graphicsRootBinder)
    {
        return;
    }

    if (resetDiagnostics)
    {
        m_graphicsRootBinder->ResetDiagnostics();
    }

    m_graphicsRootBinder->InvalidateAll();
}

void RendererSubsystem::SyncGraphicsRootBinderWithSrvHeap(ID3D12DescriptorHeap* srvHeap) const noexcept
{
    if (srvHeap == m_lastObservedSrvHeap)
    {
        return;
    }

    m_lastObservedSrvHeap = srvHeap;

    if (m_graphicsRootBinder)
    {
        m_graphicsRootBinder->InvalidateDescriptorTables();
    }
}

bool RendererSubsystem::PreparePSOAndBindings(ID3D12GraphicsCommandList* cmdList)
{
    // Step 1: Prepare custom images (before draw)
    if (m_customImageManager)
    {
        m_customImageManager->PrepareCustomImagesForDraw();
    }

    // Step 2: Update Ring Buffer offsets (Delayed Fill pattern)
    if (m_uniformManager)
    {
        m_uniformManager->UpdateRingBufferOffsets(UpdateFrequency::PerObject);
    }

    // Step 3: Get layout from state with fallback to default
    const VertexLayout* layout = m_currentVertexLayout;
    if (!layout)
    {
        layout = VertexLayoutRegistry::GetDefault();
        LogWarn(LogVertexLayout, "PreparePSOAndBindings: layout not set, using default");
    }

    // Step 4: Inline PSO state construction
    RenderStateValidator::DrawState state{};
    state.program             = const_cast<ShaderProgram*>(m_currentShaderProgram);
    state.blendConfig         = m_currentBlendConfig; // [REFACTORED] Use BlendConfig
    state.depthConfig         = m_currentDepthConfig; // [REFACTORED] Use DepthConfig
    state.stencilDetail       = m_currentStencilTest;
    state.rasterizationConfig = m_currentRasterizationConfig;
    state.topology            = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    m_renderTargetBinder->GetCurrentRTFormats(state.rtFormats.data());
    state.depthFormat = m_renderTargetBinder->GetCurrentDepthFormat();
    state.rtCount     = 8;

    // Step 5: Validate Draw state
    const char* errorMsg = nullptr;
    if (!RenderStateValidator::ValidateDrawState(state, &errorMsg))
    {
        LogError(LogRenderer, "PreparePSOAndBindings validation failed: %s", errorMsg ? errorMsg : "Unknown error");
        return false;
    }

    // Step 6: Build PSOKey and get or create PSO
    PSOKey psoKey;
    psoKey.shaderProgram       = state.program;
    psoKey.vertexLayout        = layout;
    psoKey.depthFormat         = state.depthFormat;
    psoKey.blendConfig         = state.blendConfig;
    psoKey.depthConfig         = state.depthConfig;
    psoKey.stencilDetail       = state.stencilDetail;
    psoKey.rasterizationConfig = state.rasterizationConfig;
    for (int i = 0; i < 8; ++i)
        psoKey.rtFormats[i] = state.rtFormats[i];

    // Per-RT blend from shaders.properties (injected via ProgramDirectives)
    psoKey.hasIndependentBlend = m_hasIndependentBlend;
    if (m_hasIndependentBlend)
        psoKey.perRTBlendConfigs = m_perRTBlendConfigs;

    ID3D12PipelineState* pso = m_psoManager->GetOrCreatePSO(psoKey);

    // Step 7: Bind PSO (avoid redundant binding)
    if (pso != m_lastBoundPSO)
    {
        cmdList->SetPipelineState(pso);
        m_lastBoundPSO = pso;
    }

    if (!m_graphicsRootBinder)
    {
        ERROR_RECOVERABLE("PreparePSOAndBindings: GraphicsRootBinder is not available");
        return false;
    }

    if (!m_uniformManager)
    {
        ERROR_RECOVERABLE("PreparePSOAndBindings: UniformManager is not available");
        return false;
    }

    if (!m_currentShaderProgram)
    {
        ERROR_RECOVERABLE("PreparePSOAndBindings: ShaderProgram is not set");
        return false;
    }

    // Step 8: Bind Root Signature
    m_currentShaderProgram->Use(cmdList, *m_graphicsRootBinder);

    // Step 9: Set Primitive Topology
    cmdList->IASetPrimitiveTopology(state.topology);

    // Step 10: Bind engine root CBVs through the dirty binder
    for (uint32_t slot = 0; slot < GraphicsRootBindingCache::ENGINE_ROOT_CBV_SLOT_COUNT; ++slot)
    {
        const D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = m_uniformManager->GetEngineBufferGPUAddress(slot);
        m_graphicsRootBinder->BindEngineCbvIfDirty(cmdList, slot, cbvAddress);
    }

    // Step 11: Bind the custom-buffer descriptor table through the dirty binder
    const D3D12_GPU_DESCRIPTOR_HANDLE customBufferTableHandle =
        m_uniformManager->PrepareCustomBufferDescriptorTableForCurrentState();
    m_graphicsRootBinder->BindDescriptorTableIfDirty(
        cmdList,
        BindlessRootSignature::ROOT_DESCRIPTOR_TABLE_CUSTOM,
        customBufferTableHandle
    );

    return true;
}

void RendererSubsystem::Draw(uint32_t vertexCount, uint32_t startVertex)
{
    if (vertexCount == 0)
    {
        LogWarn(LogRenderer, "Draw: vertexCount is 0, nothing to draw");
        return;
    }

    auto cmdList = GetCurrentCommandList();
    if (!cmdList)
    {
        LogError(LogRenderer, "Draw: CommandList is nullptr");
        return;
    }

    // [REFACTORED] Use public helper functions to prepare PSO and resource bindings
    if (!PreparePSOAndBindings(cmdList))
    {
        return;
    }

    // Issue Draw call
    cmdList->DrawInstanced(vertexCount, 1, startVertex, 0);

    // Increment draw count
    if (m_uniformManager)
    {
        m_uniformManager->IncrementDrawCount();
    }

    LogDebug(LogRenderer, "Draw: Drew %u vertices starting from vertex %u", vertexCount, startVertex);
}

void RendererSubsystem::DrawIndexed(uint32_t indexCount, uint32_t startIndex, int32_t baseVertex)
{
    if (indexCount == 0)
    {
        LogWarn(LogRenderer, "DrawIndexed: indexCount is 0, nothing to draw");
        return;
    }

    auto cmdList = GetCurrentCommandList();
    if (!cmdList)
    {
        LogError(LogRenderer, "DrawIndexed: CommandList is nullptr");
        return;
    }

    // [REFACTORED] Use public helper functions to prepare PSO and resource bindings
    if (!PreparePSOAndBindings(cmdList))
    {
        return;
    }

    // Issue DrawIndexed call
    D3D12RenderSystem::DrawIndexed(indexCount, startIndex, baseVertex);

    // Increment draw count
    if (m_uniformManager)
    {
        m_uniformManager->IncrementDrawCount();
    }

    LogDebug(LogRenderer, "DrawIndexed: Drew %u indices starting from index %u with base vertex %d",
             indexCount, startIndex, baseVertex);
}

void RendererSubsystem::DrawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex, uint32_t startInstance)
{
    if (vertexCount == 0 || instanceCount == 0)
    {
        LogWarn(LogRenderer, "DrawInstanced: vertexCount or instanceCount is 0, nothing to draw");
        return;
    }

    auto cmdList = GetCurrentCommandList();
    if (!cmdList)
    {
        LogError(LogRenderer, "DrawInstanced: CommandList is nullptr");
        return;
    }

    // [REFACTORED] 使用公共辅助函数准备PSO和资源绑定
    if (!PreparePSOAndBindings(cmdList))
    {
        return;
    }

    // Issue DrawInstanced call
    cmdList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);

    // Increment draw count
    if (m_uniformManager)
    {
        m_uniformManager->IncrementDrawCount();
    }

    LogDebug(LogRenderer, "DrawInstanced: Drew %u vertices x %u instances starting from vertex %u, instance %u",
             vertexCount, instanceCount, startVertex, startInstance);
}

// ============================================================================
// M6.3: Present RT输出API
// ============================================================================

void RendererSubsystem::PresentWithShader(std::shared_ptr<ShaderProgram> finalProgram,
                                          const std::vector<uint32_t>&   inputRTs)
{
    if (!finalProgram)
    {
        LogError(LogRenderer, "PresentWithShader: finalProgram is nullptr");
        return;
    }

    auto backBufferRTV = D3D12RenderSystem::GetBackBufferRTV();
    auto cmdList       = GetCurrentCommandList();

    if (!cmdList)
    {
        LogError(LogRenderer, "PresentWithShader: CommandList is nullptr");
        return;
    }

    // 核心：绑定BackBuffer为RTV（Shader输出目标）
    // 教学要点：OMSetRenderTargets决定Shader的SV_Target输出位置
    cmdList->OMSetRenderTargets(1, &backBufferRTV, FALSE, nullptr);

    // 输入纹理（colortex0-15）通过Bindless自动访问：
    // - ColorTargetsIndexBuffer已包含所有colortex的Bindless索引
    // - Shader通过索引直接访问：allTextures[colorTargets.readIndices[0]]
    // - 无需手动绑定SRV（Bindless架构优势）
    // inputRTs参数保留用于未来的验证或优化
    UNUSED(inputRTs);

    UseProgram(finalProgram);

    LogDebug(LogRenderer, "PresentWithShader: Rendered to BackBuffer");
}

void RendererSubsystem::PresentRenderTarget(int rtIndex, RenderTargetType rtType)
{
    // ========================================================================
    // [REFACTORED] Provider-based Architecture + Format Mismatch Fallback
    // ========================================================================

    // Step 1: Get provider
    IRenderTargetProvider* provider = GetRenderTargetProvider(rtType);
    if (!provider)
    {
        LogError(LogRenderer, "PresentRenderTarget: Provider is null for RenderTargetType %d", static_cast<int>(rtType));
        return;
    }

    // Step 2: Validate rtIndex range
    const int rtCount = provider->GetCount();
    if (rtIndex < 0 || rtIndex >= rtCount)
    {
        LogError(LogRenderer, "PresentRenderTarget: rtIndex %d out of range [0, %d) for RenderTargetType %d",
                 rtIndex, rtCount, static_cast<int>(rtType));
        return;
    }

    // Step 3: Select Main or Alt resource based on FlipState
    ID3D12Resource* sourceRT       = nullptr;
    bool            useAltResource = false;

    if (provider->SupportsFlipState())
    {
        sourceRT = provider->GetMainResource(rtIndex);
    }
    else
    {
        sourceRT = provider->GetMainResource(rtIndex);
    }

    if (!sourceRT)
    {
        LogError(LogRenderer, "PresentRenderTarget: Source resource is null (rtIndex=%d, rtType=%d, useAlt=%d)",
                 rtIndex, static_cast<int>(rtType), useAltResource ? 1 : 0);
        return;
    }

    // Step 4: Get BackBuffer resource
    ID3D12Resource* backBuffer = D3D12RenderSystem::GetBackBufferResource();
    if (!backBuffer)
    {
        LogError(LogRenderer, "PresentRenderTarget: BackBuffer resource is null");
        return;
    }

    // Step 4.1: Determine source initial state (needed by both paths)
    D3D12_RESOURCE_STATES sourceInitialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    if (rtType == RenderTargetType::DepthTex || rtType == RenderTargetType::ShadowTex)
    {
        sourceInitialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }

    // Step 4.5: Format compatibility check — fast path (CopyResource) or fallback (Draw)
    DXGI_FORMAT sourceFormat     = provider->GetFormat(rtIndex);
    DXGI_FORMAT backbufferFormat = D3D12RenderSystem::GetBackBufferFormat();

    if (sourceFormat != backbufferFormat)
    {
        // Fallback path: draw call with blit shader
        LogInfo(LogRenderer, "PresentRenderTarget: Format mismatch (source=%d vs backbuffer=%d), using draw fallback",
                static_cast<int>(sourceFormat), static_cast<int>(backbufferFormat));
        PresentRenderTargetWithDraw(rtIndex, rtType, sourceInitialState);
        g_theImGui->Render();
        return;
    }

    g_theImGui->Render();

    // ========================================================================
    // Fast path: CopyResource (formats match)
    // ========================================================================

    ID3D12GraphicsCommandList* cmdList = GetCurrentCommandList();
    if (!cmdList)
    {
        LogError(LogRenderer, "PresentRenderTarget: CommandList is null");
        return;
    }

    // Resource barriers for copy
    // NOTE: Raw barriers used here because backBuffer is a swap chain resource (not D12Resource).
    // This is the only legitimate use of raw ResourceBarrier in the codebase.
    D3D12_RESOURCE_BARRIER barriers[2];

    barriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barriers[0].Transition.pResource   = sourceRT;
    barriers[0].Transition.StateBefore = sourceInitialState;
    barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    barriers[1].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barriers[1].Transition.pResource   = backBuffer;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    cmdList->ResourceBarrier(2, barriers);

    cmdList->CopyResource(backBuffer, sourceRT);

    // Restore resource states
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.StateAfter  = sourceInitialState;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;

    cmdList->ResourceBarrier(2, barriers);
}

// ============================================================================
// InitializeBlitProgram - Lazy init blit shader for format mismatch fallback
// ============================================================================

void RendererSubsystem::InitializeBlitProgram()
{
    if (m_blitProgram)
        return;

    std::filesystem::path vsPath = ".enigma/assets/engine/shaders/program/final.vs.hlsl";
    std::filesystem::path psPath = ".enigma/assets/engine/shaders/program/final.ps.hlsl";

    LogInfo(LogRenderer, "InitializeBlitProgram: Compiling engine blit shader (final.vs + final.ps)");
    m_blitProgram = CreateShaderProgramFromFiles(vsPath, psPath, "engine_blit");

    if (!m_blitProgram)
    {
        LogError(LogRenderer, "InitializeBlitProgram: Failed to compile blit shader");
    }
}

// ============================================================================
// PresentRenderTargetWithDraw - Draw call fallback for format mismatch
// ============================================================================

void RendererSubsystem::PresentRenderTargetWithDraw(
    int                   rtIndex,
    RenderTargetType      rtType,
    D3D12_RESOURCE_STATES sourceInitialState)
{
    // Step 1: Lazy-init blit program
    InitializeBlitProgram();
    if (!m_blitProgram)
    {
        LogError(LogRenderer, "PresentRenderTargetWithDraw: Blit program unavailable, cannot present");
        return;
    }

    auto cmdList = GetCurrentCommandList();
    if (!cmdList)
    {
        LogError(LogRenderer, "PresentRenderTargetWithDraw: CommandList is null");
        return;
    }

    // Step 2: Transition source RT -> PIXEL_SHADER_RESOURCE
    IRenderTargetProvider* provider = GetRenderTargetProvider(rtType);
    ID3D12Resource*        sourceRT = provider->GetMainResource(rtIndex);

    D3D12RenderSystem::TransitionResource(
        cmdList, sourceRT,
        sourceInitialState,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        "PresentRenderTargetWithDraw::SourceToSRV"
    );

    // Step 3: Bind backbuffer as RTV
    auto backBufferRTV = D3D12RenderSystem::GetBackBufferRTV();
    cmdList->OMSetRenderTargets(1, &backBufferRTV, FALSE, nullptr);

    // Step 4: Set backbuffer format override for correct PSO creation
    DXGI_FORMAT backbufferFormat = D3D12RenderSystem::GetBackBufferFormat();
    m_renderTargetBinder->SetBackbufferOverride(backbufferFormat);

    // Step 5: Cache blit shader (PSO created at Draw time via PreparePSOAndBindings)
    m_currentShaderProgram = m_blitProgram.get();

    // Step 6: Draw full-screen quad
    FullQuadsRenderer::DrawFullQuads();

    // Step 7: Clear backbuffer override
    m_renderTargetBinder->ClearBackbufferOverride();

    // Step 8: Restore source RT state
    D3D12RenderSystem::TransitionResource(
        cmdList, sourceRT,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        sourceInitialState,
        "PresentRenderTargetWithDraw::RestoreSource"
    );
}

void RendererSubsystem::SetBlendConfig(const BlendConfig& config)
{
    if (m_currentBlendConfig == config && !m_hasIndependentBlend)
    {
        return;
    }
    m_currentBlendConfig  = config;
    m_hasIndependentBlend = false;
    m_perRTBlendConfigs   = {};
}

void RendererSubsystem::SetBlendConfig(const BlendConfig& config, int rtIndex)
{
    if (rtIndex < 0 || rtIndex >= 8)
        return;

    m_hasIndependentBlend        = true;
    m_perRTBlendConfigs[rtIndex] = config;
}

void RendererSubsystem::SetSamplerConfig(uint32_t index, const SamplerConfig& config)
{
    m_samplerProvider->SetSamplerConfig(index, config);
}

void RendererSubsystem::SetDepthConfig(const DepthConfig& config)
{
    if (m_currentDepthConfig == config)
    {
        return;
    }
    m_currentDepthConfig = config;
}

void RendererSubsystem::SetStencilTest(const StencilTestDetail& detail)
{
    m_currentStencilTest = detail;
}

void RendererSubsystem::SetStencilRefValue(uint8_t refValue)
{
    // Avoid redundant updates
    if (m_currentStencilRef == refValue)
    {
        return;
    }

    m_currentStencilRef = refValue;

    // [DYNAMIC STATE] Stencil reference value can be changed per draw call
    // without PSO rebuild. Applied via OMSetStencilRef on active CommandList.

    // Get active CommandList from D3D12RenderSystem
    auto* cmdList = GetCurrentCommandList();
    if (cmdList && m_currentStencilTest.enable)
    {
        cmdList->OMSetStencilRef(refValue);
        LogDebug(LogRenderer, "SetStencilRefValue: Updated to {} and applied to CommandList", refValue);
    }
    else
    {
        LogDebug(LogRenderer, "SetStencilRefValue: Updated to {} (will apply when stencil enabled)", refValue);
    }
}

void RendererSubsystem::SetRasterizationConfig(const RasterizationConfig& config)
{
    m_currentRasterizationConfig = config;

    // [IMPORTANT] Rasterization configuration is part of PSO (immutable state)
    // Changing it requires PSO rebuild. Next UseProgram() will create new PSO
    // with updated rasterization settings via PSOManager.
    // Configuration is stored in pending state until PSO is actually created/bound.

    LogDebug(LogRenderer, "SetRasterizationConfig: Rasterization state updated (CullMode={}, FillMode={})",
             static_cast<int>(config.cullMode), static_cast<int>(config.fillMode));
}

void RendererSubsystem::SetViewport(int width, int height)
{
    D3D12RenderSystem::SetViewport(width, height);
}

void RendererSubsystem::SetVertexLayout(const VertexLayout* layout)
{
    m_currentVertexLayout = layout;

    // [IMPORTANT] VertexLayout affects PSO (InputLayout is immutable state)
    // Next UseProgram() will use this layout for PSO creation/lookup.
    // If layout is nullptr, default layout (Vertex_PCUTBN) will be used.

    if (layout)
    {
        LogDebug(LogRenderer, "SetVertexLayout: Layout set to '%s' (stride=%zu, hash=%zu)",
                 layout->GetLayoutName(), layout->GetStride(), layout->GetLayoutHash());
    }
    else
    {
        LogDebug(LogRenderer, "SetVertexLayout: Layout set to nullptr (will use default)");
    }
}

const VertexLayout* RendererSubsystem::GetCurrentVertexLayout() const noexcept
{
    // Return current layout, or default if nullptr
    // Teaching points:
    // - nullptr indicates default layout should be used
    // - Caller can check against VertexLayoutRegistry::GetDefault() if needed
    return m_currentVertexLayout;
}

void RendererSubsystem::BindRenderTargets(const std::vector<std::pair<RenderTargetType, int>>& targets)
{
    m_renderTargetBinder->BindRenderTargets(targets);
}

size_t RendererSubsystem::GetCurrentVertexOffset() const noexcept
{
    // Delegate to VertexRingBuffer wrapper
    // Returns 0 if RingBuffer not yet initialized
    if (m_immediateVertexRingBuffer)
    {
        return m_immediateVertexRingBuffer->GetCurrentOffset();
    }
    return 0;
}

size_t RendererSubsystem::GetCurrentIndexOffset() const noexcept
{
    // Delegate to IndexRingBuffer wrapper
    // Returns 0 if RingBuffer not yet initialized
    if (m_immediateIndexRingBuffer)
    {
        return m_immediateIndexRingBuffer->GetCurrentOffset();
    }
    return 0;
}

// ========================================================================
// DrawVertexArray - instant data non-indexed drawing
// ========================================================================

void RendererSubsystem::DrawVertexArray(const std::vector<Vertex>& vertices)
{
    DrawVertexArray(vertices.data(), vertices.size());
}

void RendererSubsystem::DrawVertexArray(const Vertex* vertices, size_t count)
{
    if (!m_immediateVertexRingBuffer)
    {
        ERROR_RECOVERABLE("DrawVertexArray: ImmediateVBO not initialized");
        return;
    }

    LogDebug(LogRenderer, "DrawVertexArray:: called, count=%zu, offset=%zu",
             count, m_immediateVertexRingBuffer->GetCurrentOffset());

    // Use RingBuffer wrapper API (Option D Architecture)
    // Append returns VBV with correct BufferLocation for mixed-stride support
    auto result = m_immediateVertexRingBuffer->Append(vertices, count, sizeof(Vertex));

    D3D12RenderSystem::BindVertexBuffer(result.vbv, 0);
    Draw(static_cast<uint32_t>(count), 0);
}

// ========================================================================
// DrawVertexArray - instant data index drawing
// ========================================================================

void RendererSubsystem::DrawVertexArray(const std::vector<Vertex>& vertices, const std::vector<unsigned>& indices)
{
    DrawVertexArray(vertices.data(), vertices.size(), indices.data(), indices.size());
}

void RendererSubsystem::DrawVertexArray(const Vertex* vertices, size_t vertexCount, const unsigned* indices, size_t indexCount)
{
    if (!m_immediateVertexRingBuffer || !m_immediateIndexRingBuffer)
    {
        ERROR_RECOVERABLE("DrawVertexArray:: Immediate RingBuffers not initialized");
        return;
    }
    if (!vertices || vertexCount == 0 || !indices || indexCount == 0)
    {
        ERROR_RECOVERABLE("DrawVertexArray:: Invalid vertex or index data")
        return;
    }

    // Use RingBuffer wrapper API (Option D Architecture)
    // Append returns VBV with correct BufferLocation for mixed-stride support
    auto vbResult = m_immediateVertexRingBuffer->Append(vertices, vertexCount, sizeof(Vertex));

    // Append index data to Ring Buffer
    auto ibResult = m_immediateIndexRingBuffer->Append(indices, indexCount);

    D3D12RenderSystem::BindVertexBuffer(vbResult.vbv, 0);
    SetIndexBuffer(m_immediateIndexRingBuffer->GetBuffer());
    DrawIndexed(static_cast<uint32_t>(indexCount), ibResult.startIndex, 0);
}

void RendererSubsystem::DrawVertexBuffer(const std::shared_ptr<D12VertexBuffer>& vbo)
{
    // [VALIDATION] Null and count checks
    if (!vbo || vbo->GetVertexCount() == 0)
    {
        ERROR_RECOVERABLE("DrawVertexBuffer:: Invalid vertex buffer or count");
        return;
    }

    // [VALIDATION] Get layout from state for stride validation
    const VertexLayout* layout = m_currentVertexLayout;
    if (!layout)
    {
        layout = VertexLayoutRegistry::GetDefault();
    }

    // [VALIDATION] Stride validation (warning only - does not throw)
    if (vbo->GetStride() != layout->GetStride())
    {
        LogWarn(LogVertexLayout, "DrawVertexBuffer: stride mismatch - buffer=%zu, layout=%zu",
                vbo->GetStride(), layout->GetStride());
    }

    // [PERFORMANCE] Direct VBO binding - NO Ring Buffer copy!
    // VBO already in GPU memory, just bind directly via IA stage
    D3D12RenderSystem::BindVertexBuffer(vbo->GetView(), 0);
    Draw(static_cast<uint32_t>(vbo->GetVertexCount()), 0);
}

// ============================================================================
// Direct Vertex Buffer Drawing - Skip Ring Buffer (Static Geometry)
// ============================================================================

void RendererSubsystem::DrawVertexBuffer(const std::shared_ptr<D12VertexBuffer>& vbo, const std::shared_ptr<D12IndexBuffer>& ibo)
{
    // [VALIDATION] Null and count checks
    if (!vbo || !ibo || ibo->GetIndexCount() == 0 || vbo->GetVertexCount() == 0)
    {
        ERROR_RECOVERABLE("DrawVertexBuffer:: Invalid vertex buffer, index buffer or count");
        return;
    }

    // [VALIDATION] Get and validate layout
    const VertexLayout* layout = m_currentVertexLayout;
    if (!layout)
    {
        layout = VertexLayoutRegistry::GetDefault();
    }

    // [VALIDATION] Stride validation (warning only)
    if (vbo->GetStride() != layout->GetStride())
    {
        LogWarn(LogVertexLayout, "DrawVertexBuffer: stride mismatch - buffer=%zu, layout=%zu",
                vbo->GetStride(), layout->GetStride());
    }

    // [PERFORMANCE] Direct VBO/IBO binding - NO Ring Buffer copy!
    // Reference: DX12Renderer::DrawVertexIndexedInternal implementation
    // - Static geometry (Chunk Mesh) should NOT be copied every frame
    // - VBO/IBO already in GPU memory, just bind directly via IA stage
    D3D12RenderSystem::BindVertexBuffer(vbo->GetView(), 0);
    D3D12RenderSystem::BindIndexBuffer(ibo->GetView());

    // [DRAW] Direct indexed draw - startIndex=0 since using original IBO
    DrawIndexed(static_cast<uint32_t>(ibo->GetIndexCount()), 0, 0);
}

// ============================================================================
// Clear Operations - Flexible RT Management
// ============================================================================

void RendererSubsystem::ClearRenderTarget(RenderTargetType rtType, int rtIndex, const Rgba8& clearColor)
{
    // ========================================================================
    // [REFACTORED] Provider-based Architecture
    // ========================================================================
    // 重构说明：
    // - 使用 IRenderTargetProvider 统一接口替代废弃的 m_renderTargetManager
    // - 支持 ColorTex 和 ShadowColor 类型（RTV-based）
    // - DepthTex 和 ShadowTex 应使用 ClearDepthStencil 方法
    // - 符合 SOLID 原则（依赖倒置）
    // ========================================================================

    // Step 1: 验证 RenderTargetType 是否支持 RTV Clear
    if (rtType == RenderTargetType::DepthTex || rtType == RenderTargetType::ShadowTex)
    {
        LogError(LogRenderer, "ClearRenderTarget: RenderTargetType %d is depth-based, use ClearDepthStencil instead",
                 static_cast<int>(rtType));
        return;
    }

    // Step 2: 获取 CommandList
    auto* cmdList = GetCurrentCommandList();
    if (!cmdList)
    {
        LogError(LogRenderer, "ClearRenderTarget: No active CommandList");
        return;
    }

    // Step 3: 获取对应的 Provider
    IRenderTargetProvider* provider = GetRenderTargetProvider(rtType);
    if (!provider)
    {
        LogError(LogRenderer, "ClearRenderTarget: Provider is null for RenderTargetType %d", static_cast<int>(rtType));
        return;
    }

    // Step 4: 验证 rtIndex 范围
    const int rtCount = provider->GetCount();
    if (rtIndex < 0 || rtIndex >= rtCount)
    {
        LogError(LogRenderer, "ClearRenderTarget: rtIndex %d out of range [0, %d) for RenderTargetType %d",
                 rtIndex, rtCount, static_cast<int>(rtType));
        return;
    }

    // Step 5: 获取 RTV handle
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = provider->GetMainRTV(rtIndex);

    // Step 6: 转换 Rgba8 到 float 数组
    float clearColorFloat[4] = {
        clearColor.r / 255.0f,
        clearColor.g / 255.0f,
        clearColor.b / 255.0f,
        clearColor.a / 255.0f
    };

    // Step 7: 执行 Clear
    cmdList->ClearRenderTargetView(rtvHandle, clearColorFloat, 0, nullptr);

    // Step 8: 输出日志
    const char* rtTypeName = (rtType == RenderTargetType::ColorTex) ? "colortex" : "shadowcolor";
    LogDebug(LogRenderer, "ClearRenderTarget: Cleared %s%d to RGBA(%u,%u,%u,%u)",
             rtTypeName, rtIndex, clearColor.r, clearColor.g, clearColor.b, clearColor.a);
}

void RendererSubsystem::ClearDepthStencil(uint32_t depthIndex, float clearDepth, uint8_t clearStencil)
{
    // Get active CommandList
    auto* cmdList = GetCurrentCommandList();
    if (!cmdList)
    {
        LogError(LogRenderer, "ClearDepthStencil: No active CommandList");
        return;
    }

    // Validate depthIndex range [0, 2]
    if (depthIndex > 2)
    {
        LogError(LogRenderer, "ClearDepthStencil: Invalid depthIndex=%u (max=2)", depthIndex);
        return;
    }

    // Get DSV handle for specified depth texture
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_depthTextureProvider->GetDSV(static_cast<int>(depthIndex));

    // Clear depth and stencil
    cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, clearDepth, clearStencil, 0, nullptr);
    LogInfo(LogRenderer, "ClearDepthStencil: depthIndex=%u, depth=%.2f, stencil=%u",
            depthIndex, clearDepth, clearStencil);
}

// [REFACTOR] ClearAllRenderTargets - Use RenderTargetConfig for per-RT clear settings
void RendererSubsystem::ClearAllRenderTargets()
{
    auto* cmdList = GetCurrentCommandList();
    if (!cmdList)
    {
        LogWarn(LogRenderer, "ClearAllRenderTargets: No active command list");
        return;
    }

    // Clear colortex based on each RT's config
    if (m_colorTextureProvider)
    {
        for (int i = 0; i < m_colorTextureProvider->GetCount(); ++i)
        {
            const RenderTargetConfig& config = m_colorTextureProvider->GetConfig(i);
            if (config.loadAction == LoadAction::Clear)
            {
                auto rtvHandle = m_colorTextureProvider->GetMainRTV(i);
                D3D12RenderSystem::ClearRenderTargetByConfig(cmdList, rtvHandle, config);
            }
        }
    }

    // Clear depthtex based on each RT's config
    if (m_depthTextureProvider)
    {
        for (int i = 0; i < m_depthTextureProvider->GetCount(); ++i)
        {
            const RenderTargetConfig& config = m_depthTextureProvider->GetConfig(i);
            if (config.loadAction == LoadAction::Clear)
            {
                auto dsvHandle = m_depthTextureProvider->GetMainRTV(i); // DSV handle
                D3D12RenderSystem::ClearDepthStencilByConfig(cmdList, dsvHandle, config);
            }
        }
    }

    // Clear shadowtex based on each RT's config
    if (m_shadowTextureProvider)
    {
        for (int i = 0; i < m_shadowTextureProvider->GetCount(); ++i)
        {
            const RenderTargetConfig& config = m_shadowTextureProvider->GetConfig(i);
            if (config.loadAction == LoadAction::Clear)
            {
                auto dsvHandle = m_shadowTextureProvider->GetMainRTV(i); // DSV handle
                D3D12RenderSystem::ClearDepthStencilByConfig(cmdList, dsvHandle, config);
            }
        }
    }

    // Clear shadowcolor based on each RT's config
    if (m_shadowColorProvider)
    {
        for (int i = 0; i < m_shadowColorProvider->GetCount(); ++i)
        {
            const RenderTargetConfig& config = m_shadowColorProvider->GetConfig(i);
            if (config.loadAction == LoadAction::Clear)
            {
                auto rtvHandle = m_shadowColorProvider->GetMainRTV(i);
                D3D12RenderSystem::ClearRenderTargetByConfig(cmdList, rtvHandle, config);
            }
        }
    }
}

#pragma endregion
