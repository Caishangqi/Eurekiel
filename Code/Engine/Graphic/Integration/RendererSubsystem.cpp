#include "RendererSubsystem.hpp"

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Core/ImGui/ImGuiSubsystem.hpp"
#include "Engine/Graphic/Resource/Buffer/D12VertexBuffer.hpp"
#include "Engine/Graphic/Resource/Buffer/D12IndexBuffer.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Target/RenderTargetHelper.hpp" // 阶段3.3: RenderTarget工具类
#include "Engine/Graphic/Target/RenderTargetBinder.hpp" // RenderTarget绑定器
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformCommon.hpp" // [ADD] For UniformException hierarchy
#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp" // [ADD] For ENGINE_BUFFER_RING_CAPACITY
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp" // MatricesUniforms结构体
#include "Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp" // PerObjectUniforms结构体
#include "Engine/Graphic/Shader/Uniform/CustomImageManager.hpp" // CustomImageManager类
#include "Engine/Graphic/Shader/PSO/PSOManager.hpp" // PSO管理器
#include "Engine/Graphic/Shader/PSO/RenderStateValidator.hpp" // 渲染状态验证器
#include "Engine/Graphic/Camera/ICamera.hpp" // [NEW] ICamera interface for new camera system
#include "Engine/Graphic/Shader/Program/ShaderProgram.hpp" // M6.2: ShaderProgram
#include "Engine/Graphic/Shader/Program/ShaderSource.hpp" // Shrimp Task 1: ShaderSource
#include "Engine/Graphic/Shader/Program/ShaderProgramBuilder.hpp" // Shrimp Task 1: ShaderProgramBuilder
#include "Engine/Graphic/Shader/Common/ShaderCompilationHelper.hpp" // Shrimp Task 2: 编译辅助工具
#include "Engine/Graphic/Shader/Common/ShaderIncludeHelper.hpp" // Shrimp Task 6: Include系统工具
#include "Engine/Graphic/Shader/Program/Include/ShaderPath.hpp" // Shrimp Task 6: ShaderPath路径抽象
#include "Engine/Graphic/Integration/RingBuffer/VertexRingBuffer.hpp" // [NEW] Option D: RingBuffer wrapper
#include "Engine/Graphic/Integration/RingBuffer/IndexRingBuffer.hpp"  // [NEW] Option D: RingBuffer wrapper
#include "Engine/Graphic/Integration/DrawBindingHelper.hpp" // [NEW] Draw binding helper
#include "Engine/Graphic/Resource/VertexLayout/VertexLayoutCommon.hpp"
#include "Engine/Graphic/Resource/VertexLayout/VertexLayoutRegistry.hpp" // [NEW] VertexLayout state management
#include "Engine/Graphic/Target/IRenderTargetProvider.hpp"
enigma::graphic::RendererSubsystem* g_theRendererSubsystem = nullptr;

#pragma region Lifecycle Management
void RendererSubsystem::RenderStatistics::Reset()
{
    drawCalls            = 0;
    trianglesRendered    = 0;
    activeShaderPrograms = 0;
}

// Milestone 3.0: 构造函数参数类型从Configuration改为RendererSubsystemConfig
RendererSubsystem::RendererSubsystem(RendererSubsystemConfig& config)
{
    m_configuration = config;
}

RendererSubsystem::~RendererSubsystem()
{
}

void RendererSubsystem::Initialize()
{
    LogInfo(LogRenderer, "Initializing D3D12 rendering system...");

    //-----------------------------------------------------------------------------------------------
    // Milestone 3.0: 配置系统重构完成
    // 配置已在构造函数中通过参数传入（m_configuration），无需在Initialize()中加载
    // 删除了原有的ParseFromYaml()调用和m_config赋值代码
    //-----------------------------------------------------------------------------------------------

    // 获取窗口句柄（通过配置参数）
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
        m_configuration.renderHeight // Render height
    );
    if (!success)
    {
        LogError(LogRenderer, "Failed to initialize D3D12RenderSystem");
        m_isInitialized = false;
        return;
    }

    // [INIT] Initialize engine default material
    D3D12RenderSystem::PrepareDefaultTextures();

    m_isInitialized = true;
    LogInfo(LogRenderer, "D3D12RenderSystem initialized successfully through RendererSubsystem");

    // Create PSOManager
    m_psoManager = std::make_unique<PSOManager>();
    LogInfo(LogRenderer, "PSOManager created successfully");

    // Display infrastructure flow confirmation information
    LogInfo(LogRenderer, "Initialization flow: RendererSubsystem → D3D12RenderSystem → SwapChain creation completed");
    g_theRendererSubsystem = this;
}

void RendererSubsystem::Startup()
{
    LogInfo(LogRenderer, "Starting up...");
    // ==================== 创建RenderTargetManager (Milestone 3.0任务4) ====================
    // 初始化GBuffer管理器 - 管理16个colortex RenderTarget（Iris兼容）
    try
    {
        LogInfo(LogRenderer, "Creating RenderTargetManager...");

        // Step 1: 准备RTConfig配置数组（16个）
        // 参考Iris的colortex配置，使用RGBA16F（高精度）和RGBA8（辅助数据）
        std::array<RTConfig, 16> rtConfigs = {};

        // colortex0-7: RGBA16F格式（高精度渲染，适合HDR/法线/位置等）
        for (int i = 0; i < 8; ++i)
        {
            rtConfigs[i] = RTConfig::ColorTargetWithScale(
                "colortex" + std::to_string(i), // name
                1.0f, // widthScale - 全分辨率
                1.0f, // heightScale
                DXGI_FORMAT_R8G8B8A8_UNORM,
                true, // enableFlipper
                LoadAction::Clear, // loadAction
                ClearValue::Color(m_configuration.defaultClearColor), // clearValue
                false, // enableMipmap - 默认关闭
                true, // allowLinearFilter
                1 // sampleCount - 无MSAA
            );
        }

        // colortex8-15: RGBA8格式（辅助数据，节省内存）
        for (int i = 8; i < 16; ++i)
        {
            rtConfigs[i] = RTConfig::ColorTargetWithScale(
                "colortex" + std::to_string(i), // name
                1.0f, // widthScale
                1.0f, // heightScale
                DXGI_FORMAT_R8G8B8A8_UNORM, // format - RGBA8
                true, // enableFlipper
                LoadAction::Clear, // loadAction
                ClearValue::Color(m_configuration.defaultClearColor), // clearValue
                false, // enableMipmap
                true, // allowLinearFilter
                1 // sampleCount
            );
        }

        // Step 2: 从配置读取渲染尺寸和colortex数量
        const int baseWidth     = m_configuration.renderWidth;
        const int baseHeight    = m_configuration.renderHeight;
        const int colorTexCount = m_configuration.gbufferColorTexCount;

        LogInfo(LogRenderer, "RenderTargetManager configuration: %dx%d, %d colortex (max 16)", baseWidth, baseHeight, colorTexCount);

        LogInfo(LogRenderer, "Creating ColorTextureProvider...");

        // Convert std::array to std::vector for use by Provider
        std::vector<RTConfig> colorConfigs(rtConfigs.begin(), rtConfigs.end());

        m_colorTextureProvider = std::make_unique<ColorTextureProvider>(baseWidth, baseHeight, colorConfigs);

        LogInfo(LogRenderer, "ColorTextureProvider created successfully (%d colortex)", colorTexCount);
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "Failed to create RenderTargetManager/ColorTextureProvider: %s", e.what());
        ERROR_AND_DIE(Stringf("RenderTargetManager/ColorTextureProvider initialization failed! Error: %s", e.what()))
    }

    // ==================== Create UniformManager ====================
    // Initialize the Uniform Buffer Manager - manage all Uniform Buffers
    try
    {
        LogInfo(LogRenderer, "Creating UniformManager...");

        // [RAII] UniformManager constructor automatically completes all initialization:
        // 1. Allocates Custom Buffer Descriptor pool (100 contiguous descriptors)
        // 2. Validates descriptor continuity (required for Descriptor Table)
        // 3. Stores first descriptor's GPU handle as Descriptor Table base address
        // [IMPORTANT] Shader must use register(bN, space1) for slot >= 15
        m_uniformManager = std::make_unique<UniformManager>();

        LogInfo(LogRenderer, "UniformManager created successfully (RAII initialization complete)");

        //Register MatricesUniforms as PerObject Buffer, allocate 10000 draws
        //Teaching points:
        // 1. The original size of MatricesUniforms is 1152 bytes, and it is 1280 bytes after alignment.
        // 2. 10000 × 1280 = 12.8 MB
        // 4. [NEW] Explicitly specify slot 7
        m_uniformManager->RegisterBuffer<MatricesUniforms>(
            7, // registerSlot: Slot 7 for Matrices
            UpdateFrequency::PerObject,
            BufferSpace::Engine, // space=0 (Engine Root CBV)
            ENGINE_BUFFER_RING_CAPACITY // [REFACTORED] Use unified constant instead of hardcoded 10000
        );

        LogInfo(LogRenderer, "Matrices Ring Buffer allocated: %u × 1280 bytes", ENGINE_BUFFER_RING_CAPACITY);

        // ==================== Register PerObjectUniforms Ring Buffer ====================
        // Register PerObjectUniforms as PerObject Buffer
        // Teaching points:
        // 1. PerObjectUniforms original size 128 bytes (2 Mat44), 256 bytes after alignment
        // 2. ENGINE_BUFFER_RING_CAPACITY × 256 = 2.5 MB (reasonable memory overhead)
        // 3. This is the second PerObject Buffer (the first is MatricesUniforms)
        // 4. [NEW] Explicitly specify slot 1
        m_uniformManager->RegisterBuffer<PerObjectUniforms>(
            1, // registerSlot: Slot 1 for PerObjectUniforms (Iris-compatible)
            UpdateFrequency::PerObject,
            BufferSpace::Engine, // space=0 (Engine Root CBV)
            ENGINE_BUFFER_RING_CAPACITY // [REFACTORED] Use unified constant
        );

        // ==================== Register CustomImageUniform Ring Buffer ====================
        // Register CustomImageUniform as PerObject Buffer
        // Teaching points:
        // 1. CustomImageUniform size 64 bytes (16 × uint32_t), 256 bytes after alignment
        // 2. ENGINE_BUFFER_RING_CAPACITY × 256 = 2.5 MB (reasonable memory overhead)
        // 3. This is the third PerObject Buffer (after MatricesUniforms and PerObjectUniforms)
        // 4. [NEW] Explicitly specify slot 2 for CustomImage
        m_uniformManager->RegisterBuffer<CustomImageUniform>(
            2, // registerSlot: Slot 2 for CustomImage (Iris-compatible)
            UpdateFrequency::PerObject,
            BufferSpace::Engine, // space=0 (Engine Root CBV)
            ENGINE_BUFFER_RING_CAPACITY // [REFACTORED] Use unified constant
        );

        LogInfo(LogRenderer, "CustomImageUniform Ring Buffer registered: slot 2, %u × 256 bytes", ENGINE_BUFFER_RING_CAPACITY);
    }
    catch (const UniformException& e)
    {
        // [ADD] Catch Uniform system specific exceptions (UniformBufferException, DescriptorHeapException)
        LogError(LogRenderer, "UniformManager initialization failed: %s", e.what());
        ERROR_AND_DIE(Stringf("UniformManager initialization failed: %s", e.what()))
    }
    catch (const std::exception& e)
    {
        // [ADD] Fallback for unexpected exceptions
        LogError(LogRenderer, "UniformManager unexpected error: %s", e.what());
        ERROR_AND_DIE(Stringf("UniformManager construction failed! Error: %s", e.what()))
    }

    // ==================== Create CustomImageManager ====================
    // Initialize CustomImage Manager - manage 16 CustomImage slots
    try
    {
        LogInfo(LogRenderer, "Creating CustomImageManager...");

        // Create CustomImageManager with UniformManager dependency injection
        m_customImageManager = std::make_unique<CustomImageManager>(m_uniformManager.get());

        LogInfo(LogRenderer, "CustomImageManager created successfully");
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "Failed to create CustomImageManager: {}", e.what());
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

    try
    {
        std::array<RTConfig, 3> depthConfigs = {};
        for (int i = 0; i < 3; ++i)
        {
            depthConfigs[i].width  = m_configuration.renderWidth;
            depthConfigs[i].height = m_configuration.renderHeight;
            depthConfigs[i].format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            depthConfigs[i].name   = "depthtex" + std::to_string(i);
        }
        LogInfo(LogRenderer, "Creating DepthTextureProvider...");

        std::vector<RTConfig> depthTextureVec(depthConfigs.begin(), depthConfigs.end());
        m_depthTextureProvider = std::make_unique<DepthTextureProvider>(m_configuration.renderWidth, m_configuration.renderHeight, depthTextureVec);

        LogInfo(LogRenderer, "DepthTextureProvider created successfully (3 depthtex)");
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "Failed to create DepthTextureManager/DepthTextureProvider: %s", e.what());
        ERROR_AND_DIE(Stringf("DepthTextureManager/DepthTextureProvider initialization failed! Error: %s", e.what()))
    }

    try
    {
        std::array<RTConfig, 8> shadowColorConfigs = {};
        for (int i = 0; i < 8; ++i)
        {
            shadowColorConfigs[i] = RTConfig::ColorTarget(
                "shadowcolor" + std::to_string(i),
                m_configuration.renderWidth,
                m_configuration.renderHeight,
                DXGI_FORMAT_R16G16B16A16_FLOAT,
                true, LoadAction::Clear,
                ClearValue::Color(Rgba8::BLACK),
                false, true, 1
            );
        }
        LogInfo(LogRenderer, "Creating ShadowColorProvider...");

        // 将std::array转换为std::vector供Provider使用
        std::vector<RTConfig> shadowColorVec(shadowColorConfigs.begin(), shadowColorConfigs.end());
        m_shadowColorProvider = std::make_unique<ShadowColorProvider>(m_configuration.renderWidth, m_configuration.renderHeight, shadowColorVec);
        LogInfo(LogRenderer, "ShadowColorProvider created successfully (8 shadowcolor)");
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "Failed to create ShadowColorManager/ShadowColorProvider: %s", e.what());
        ERROR_AND_DIE(Stringf("ShadowColorManager/ShadowColorProvider initialization failed! Error: %s", e.what()))
    }

    try
    {
        LogInfo(LogRenderer, "Creating ShadowTextureManager...");

        std::array<RTConfig, 2> shadowTexConfigs = {};
        for (int i = 0; i < 2; ++i)
        {
            shadowTexConfigs[i] = RTConfig::DepthTarget(
                "shadowtex" + std::to_string(i),
                m_configuration.renderWidth,
                m_configuration.renderHeight,
                DXGI_FORMAT_D32_FLOAT, true,
                LoadAction::Clear,
                ClearValue::Depth(1.0f, 0)
            );
        }

        LogInfo(LogRenderer, "Creating ShadowTextureProvider...");
        std::vector<RTConfig> shadowTextureConfigs(shadowTexConfigs.begin(), shadowTexConfigs.end());
        m_shadowTextureProvider = std::make_unique<ShadowTextureProvider>(m_configuration.renderWidth, m_configuration.renderHeight, shadowTextureConfigs);

        LogInfo(LogRenderer, "ShadowTextureProvider created successfully (2 shadowtex)");
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "Failed to create ShadowTextureManager/ShadowTextureProvider: %s", e.what());
        ERROR_AND_DIE(Stringf("ShadowTextureManager/ShadowTextureProvider initialization failed! Error: %s", e.what()))
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

    try
    {
        LogInfo(LogRenderer, "Creating fullscreen triangle VertexBuffer...");

        Vec2 vertices[] = {
            Vec2(-1.0f, -1.0f), // 左下
            Vec2(3.0f, -1.0f), // 右下（超出屏幕）
            Vec2(-1.0f, 3.0f) // 左上（超出屏幕）
        };

        m_fullscreenTriangleVB.reset(CreateVertexBuffer(sizeof(vertices), sizeof(Vec2)));
        if (!m_fullscreenTriangleVB)
        {
            LogError(LogRenderer, "Failed to create fullscreen triangle VertexBuffer");
        }

        UpdateBuffer(m_fullscreenTriangleVB.get(), vertices, sizeof(vertices));

        LogInfo(LogRenderer, "Fullscreen triangle VertexBuffer created successfully");
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "Failed to create fullscreen triangle VB: {}", e.what());
        ERROR_AND_DIE(Stringf("Fullscreen triangle VB initialization failed! Error: %s", e.what()))
    }

    // ==================== Create Immediate Mode RingBuffers (Option D Architecture) ====================
    // [NEW] RAII Ring Buffer wrappers encapsulate D12Buffer + offset state together
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
}

void RendererSubsystem::Shutdown()
{
    LogInfo(LogRenderer, "Shutting down...");

    // ==================== Shutdown VertexLayoutRegistry ====================
    // Cleanup static registry before D3D12RenderSystem shutdown
    if (VertexLayoutRegistry::IsInitialized())
    {
        VertexLayoutRegistry::Shutdown();
        LogInfo(LogRenderer, "VertexLayoutRegistry shutdown complete");
    }

    // Step 3: 最后关闭D3D12RenderSystem
    D3D12RenderSystem::Shutdown();
}

// TODO: M2 - 移除PreparePipeline，实现新的灵活渲染接口
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
    // ========================================================================
    // Pipeline 生命周期重构 - BeginFrame 阶段
    // ========================================================================
    // 职责对应 Minecraft renderLevel() 最开头:
    // 1. DirectX 12 帧准备 (PrepareNextFrame)
    // 2. 清屏操作 (对应 Minecraft CLEAR 注入点之前)
    // 3. 重置RT绑定状态缓存 (ClearBindings)
    // ========================================================================
    // [IMPORTANT] 架构设计说明：
    // - BeginFrame() 不绑定任何 RenderTarget
    // - RT 绑定由 UseProgram() 负责（三种模式）
    // - 保持职责单一，符合 SOLID 原则
    // - 避免冗余绑定，保留 Hash 缓存性能优化
    // ========================================================================
    // [CRITICAL] Hash 缓存机制：
    // - 目标：帧内优化（避免同一帧内的冗余 OMSetRenderTargets 调用）
    // - 命中率：95-98%（复杂场景），性能提升 16.7 倍
    // - 帧边界重置：每帧开始时调用 ClearBindings() 清除缓存
    // - 业界标准：UE4/5、Unity、DirectX 12 官方建议
    // ========================================================================
    // 教学要点: 
    // - 显式状态管理（现代图形API设计理念）
    // - "谁使用谁绑定"原则
    // - 性能优化与架构纯净性的平衡
    // - 帧内优化 vs 跨帧优化（跨帧缓存是错误的设计）
    // ========================================================================

    // [NEW] Reset Ring Buffers at frame start (Option D Architecture)
    // Teaching points:
    // - Per-Frame Append strategy: reset offset to 0, reuse buffer memory
    // - RAII wrapper encapsulates reset logic
    // - No need to recreate buffers each frame
    if (m_immediateVertexRingBuffer)
    {
        m_immediateVertexRingBuffer->Reset();
    }
    if (m_immediateIndexRingBuffer)
    {
        m_immediateIndexRingBuffer->Reset();
    }

    // [NEW] Reset VertexLayout to default at frame start
    // Teaching points:
    // - Ensures consistent state for each frame
    // - Default layout is Vertex_PCUTBN (set by VertexLayoutRegistry)
    // - RenderPass can override with SetVertexLayout() per draw call
    m_currentVertexLayout = VertexLayoutRegistry::GetDefault();

    // [CRITICAL FIX] 重置上一帧的PSO绑定状态（修复跨帧PSO缓存污染）
    // 原因：CommandList在帧边界被重置，GPU状态失效，必须清除CPU侧的缓存
    // 确保每帧第一次Draw调用时PSO被正确设置到CommandList
    m_lastBoundPSO = nullptr;

    // 重置Draw计数，配合Ring Buffer实现索引管理
    if (m_uniformManager)
    {
        m_uniformManager->ResetDrawCount();
        LogDebug(LogRenderer, "BeginFrame - Draw count reset to 0");
    }

    // 1. DirectX 12 帧准备 - 获取下一帧的后台缓冲区
    D3D12RenderSystem::PrepareNextFrame();
    LogDebug(LogRenderer, "BeginFrame - D3D12 next frame prepared");

    // ========================================================================
    // [CRITICAL FIX] 重置RT绑定状态（修复跨帧Hash缓存污染问题）
    // ========================================================================
    // 教学要点：
    // - Hash缓存的目标是帧内优化（95-98%命中率），而非跨帧优化
    // - GPU状态在帧边界被SwapChain重置，必须清除缓存
    // - 确保第一次UseProgram正确绑定RT（不被Hash缓存跳过）
    // - 帧内后续调用仍然享受Hash缓存优化
    // - 符合UE4/5、Unity、DirectX 12官方建议
    // ========================================================================
    if (m_renderTargetBinder)
    {
        m_renderTargetBinder->ClearBindings();
        LogDebug(LogRenderer, "BeginFrame: RT bindings cleared for new frame");
    }

    // TODO: M2 - 准备当前维度的渲染资源
    // 替代 PreparePipeline 调用
    LogDebug(LogRenderer, "BeginFrame - Render resources prepared for current dimension");

    // 3. 执行清屏操作 (对应 Minecraft CLEAR 注入点)
    // 教学要点: 这是 MixinLevelRenderer 中 CLEAR target 所在位置
    if (m_configuration.enableAutoClearColor)
    {
        // Clear SwapChain BackBuffer first
        bool success = D3D12RenderSystem::BeginFrame(
            m_configuration.defaultClearColor, // 配置的默认颜色
            m_configuration.defaultClearDepth, // 深度清除值
            m_configuration.defaultClearStencil // 模板清除值
        );

        if (!success)
        {
            LogWarn(LogRenderer, "BeginFrame - D3D12 frame clear failed");
        }

        // [NEW] Clear all GBuffer RTs and DepthTex (centralized clear strategy)
        // This ensures clean state for the frame
        // Clear order:
        // 1. SwapChain BackBuffer: cleared by D3D12RenderSystem::BeginFrame (above)
        // 2. GBuffer colortex (0-7): cleared here
        // 3. DepthTex (0-2): cleared here (including stencil)
        //
        // Centralized approach benefits:
        // - All RTs start with clean state
        // - Multi-pass rendering can rely on preserved values (via LoadAction::Load)
        // - No RT trailing artifacts
        ClearAllRenderTargets(m_configuration.defaultClearColor);

        LogDebug(LogRenderer, "BeginFrame - All render targets cleared (centralized strategy)");
    }

    LogInfo(LogRenderer, "BeginFrame - Frame preparation completed (ready for game update)");
}

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
void RendererSubsystem::EndFrame()
{
    // ========================================================================
    // [OK] SIMPLIFIED (2025-10-21): 简单委托给 D3D12RenderSystem
    // ========================================================================
    // 职责: 结束帧渲染，提交 GPU 命令并呈现画面
    // - 不再包含 Phase 管理逻辑（已移除）
    // - 不再包含 RenderCommandQueue 管理（TODO: M2）
    // - 只负责简单委托给底层 API 封装层
    // ========================================================================
    // 教学要点: 高层Subsystem应该是轻量级的委托层
    // - BeginFrame() → D3D12RenderSystem::BeginFrame()
    // - EndFrame() → D3D12RenderSystem::EndFrame()
    // - 避免重复的业务逻辑（KISS原则）
    // ========================================================================

    LogInfo(LogRenderer, "RendererSubsystem::EndFrame() called");

    D3D12RenderSystem::EndFrame();

    LogInfo(LogRenderer, "RendererSubsystem::EndFrame() completed");

    // TODO (M2): 恢复 RenderCommandQueue 管理（在新的灵活渲染架构中）
    // - 双缓冲队列交换
    // - Setup/Begin/End 阶段
    // - WorldRenderingPhase 执行
    // - 当前简化版本只负责委托，业务逻辑在 D3D12RenderSystem
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
        ShaderType::GBuffers_Terrain,
        buildResult.directives // 使用解析后的 Directives
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

//-----------------------------------------------------------------------------------------------
// ImGui Integration Support (7 getter methods for IImGuiRenderContext)
//-----------------------------------------------------------------------------------------------

ID3D12GraphicsCommandList* RendererSubsystem::GetCurrentCommandList() const noexcept
{
    return D3D12RenderSystem::GetCurrentCommandList();
}

ID3D12DescriptorHeap* RendererSubsystem::GetSRVHeap() const noexcept
{
    auto* heapManager = D3D12RenderSystem::GetGlobalDescriptorHeapManager();
    return heapManager ? heapManager->GetCbvSrvUavHeap() : nullptr;
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
    // [DELEGATION] 委托给CustomImageManager处理
    // 教学要点：
    // - 简单的委托模式（Delegation Pattern）
    // - 封装实现细节，提供用户友好的接口
    // - 完整的空指针检查

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
    // [DELEGATION] 委托给CustomImageManager处理
    // 教学要点：
    // - const方法，不修改对象状态
    // - 返回原始指针（非所有权）
    // - 空指针检查确保安全

    if (m_customImageManager)
    {
        return m_customImageManager->GetCustomImage(slotIndex);
    }

    LogWarn(LogRenderer, "GetCustomImage: CustomImageManager is not initialized");
    return nullptr;
}

void RendererSubsystem::ClearCustomImage(int slotIndex)
{
    // [DELEGATION] 委托给CustomImageManager处理
    // 教学要点：
    // - 等价于SetCustomImage(slotIndex, nullptr)
    // - 提供更清晰的语义
    // - 完整的空指针检查

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
    std::shared_ptr<ShaderProgram>             shaderProgram,
    const std::vector<std::pair<RTType, int>>& targets
)
{
    if (!shaderProgram)
    {
        LogError(LogRenderer, "UseProgram: shaderProgram is nullptr");
        return;
    }

    // Cache current ShaderProgram for subsequent Draw() calls
    m_currentShaderProgram = shaderProgram.get();

    auto cmdList = D3D12RenderSystem::GetCurrentCommandList();
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
// [NEW] GetProvider - Access RT provider for dynamic configuration
//-----------------------------------------------------------------------------------------------

IRenderTargetProvider* RendererSubsystem::GetProvider(RTType rtType)
{
    if (!m_renderTargetBinder)
    {
        LogError(LogRenderer, "GetProvider: RenderTargetBinder is nullptr");
        return nullptr;
    }

    return m_renderTargetBinder->GetProvider(rtType);
}

// [NEW] BeginCamera - ICamera interface version
void RendererSubsystem::BeginCamera(const ICamera& camera)
{
    LogInfo(LogRenderer, "BeginCamera(ICamera):: Camera type: %d", static_cast<int>(camera.GetCameraType()));

    // Validate UniformManager
    if (!m_uniformManager)
    {
        LogError(LogRenderer, "BeginCamera(ICamera):: UniformManager is not initialized");
        ERROR_AND_DIE("UniformManager is not initialized")
    }

    // Validate rendering system ready
    if (!IsReadyForRendering())
    {
        LogError(LogRenderer, "BeginCamera(ICamera):: The rendering system is not ready");
        ERROR_AND_DIE("The rendering system is not ready")
    }

    try
    {
        // [NEW] Create MatricesUniforms and let camera fill it
        MatricesUniforms uniforms;
        camera.UpdateMatrixUniforms(uniforms);

        // Upload to GPU
        m_uniformManager->UploadBuffer<MatricesUniforms>(uniforms);

        LogInfo(LogRenderer, "BeginCamera(ICamera):: Camera matrices uploaded successfully");
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "BeginCamera(ICamera):: Exception - %s", e.what());
        ERROR_AND_DIE(e.what())
    }
}

void RendererSubsystem::EndCamera(const ICamera& camera)
{
    UNUSED(camera)
    LogWarn(LogRenderer, "EndCamera:: Not implemented yet");
}

D12VertexBuffer* RendererSubsystem::CreateVertexBuffer(size_t size, unsigned stride)
{
    // 教学要点：
    // 1. 调用D3D12RenderSystem::CreateVertexBufferTyped()创建新架构的VertexBuffer
    // 2. 返回裸指针，调用者负责管理生命周期
    // 3. 使用nullptr作为initialData（后续通过UpdateBuffer上传数据）

    if (size == 0 || stride == 0)
    {
        LogError(LogRenderer,
                 "CreateVertexBuffer: Invalid parameters (size: {}, stride: {})",
                 size, stride);
        return nullptr;
    }

    // 调用D3D12RenderSystem创建D12VertexBuffer
    auto vertexBuffer = D3D12RenderSystem::CreateVertexBuffer(size, stride, nullptr, "AppVertexBuffer");

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

    // 教学要点：
    // - D12VertexBuffer使用Map/Unmap模式进行CPU更新
    // - Map()将GPU内存映射到CPU可访问地址空间
    // - Unmap()取消映射并确保GPU可见性
    // - 对应DirectX 11的UpdateSubresource，但DirectX 12更显式

    // 检查越界
    if (offset + size > buffer->GetSize())
    {
        LogError(LogRenderer,
                 "UpdateBuffer: Data exceeds buffer size (offset: {}, size: {}, buffer size: {})",
                 offset, size, buffer->GetSize());
        return;
    }

    // Map缓冲区（获取CPU可访问指针）
    void* mappedPtr = buffer->Map(nullptr);
    if (!mappedPtr)
    {
        LogError(LogRenderer, "UpdateBuffer: Failed to map D12VertexBuffer");
        return;
    }

    // 将数据拷贝到映射的内存（支持offset）
    memcpy(static_cast<uint8_t*>(mappedPtr) + offset, data, size);

    // Unmap缓冲区（使数据对GPU可见）
    D3D12_RANGE writtenRange = {offset, offset + size};
    buffer->Unmap(&writtenRange);

    LogDebug(LogRenderer,
             "UpdateBuffer: Updated {} bytes at offset {} (total size: {})",
             size, offset, buffer->GetSize());
}

bool RendererSubsystem::PreparePSOAndBindings(ID3D12GraphicsCommandList* cmdList)
{
    // Step 1: Prepare custom images (before draw)
    if (m_customImageManager)
    {
        DrawBindingHelper::PrepareCustomImages(m_customImageManager.get());
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
    state.blendMode           = m_currentBlendMode;
    state.depthMode           = m_currentDepthMode;
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

    // Step 6: Get or create PSO (with layout parameter)
    ID3D12PipelineState* pso = m_psoManager->GetOrCreatePSO(
        state.program,
        layout,
        state.rtFormats.data(),
        state.depthFormat,
        state.blendMode,
        state.depthMode,
        state.stencilDetail,
        state.rasterizationConfig
    );

    // Step 7: Bind PSO (avoid redundant binding)
    if (pso != m_lastBoundPSO)
    {
        cmdList->SetPipelineState(pso);
        m_lastBoundPSO = pso;
    }

    // Step 8: Bind Root Signature
    if (m_currentShaderProgram)
    {
        m_currentShaderProgram->Use(cmdList);
    }

    // Step 9: Set Primitive Topology
    cmdList->IASetPrimitiveTopology(state.topology);

    // Step 10: Bind Engine Buffers (slots 0-14)
    DrawBindingHelper::BindEngineBuffers(cmdList, m_uniformManager.get());

    // Step 11: Bind Custom Buffer Table (slot 15)
    DrawBindingHelper::BindCustomBufferTable(cmdList, m_uniformManager.get());

    return true;
}

void RendererSubsystem::Draw(uint32_t vertexCount, uint32_t startVertex)
{
    if (vertexCount == 0)
    {
        LogWarn(LogRenderer, "Draw: vertexCount is 0, nothing to draw");
        return;
    }

    auto cmdList = D3D12RenderSystem::GetCurrentCommandList();
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

    auto cmdList = D3D12RenderSystem::GetCurrentCommandList();
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

    auto cmdList = D3D12RenderSystem::GetCurrentCommandList();
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

void RendererSubsystem::DrawFullscreenQuad()
{
    // M6.3: 全屏三角形技术（3个顶点覆盖整个屏幕）
    // 教学要点：比四边形更高效（3个顶点 vs 6个顶点），业界标准做法
    // VB已在Startup()中预创建，避免首帧卡顿

    if (!m_fullscreenTriangleVB)
    {
        LogError(LogRenderer, "DrawFullscreenQuad: VB not initialized (call Startup first)");
        return;
    }

    SetVertexBuffer(m_fullscreenTriangleVB.get());
    Draw(3);
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
    auto cmdList       = D3D12RenderSystem::GetCurrentCommandList();

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
    DrawFullscreenQuad();

    LogDebug(LogRenderer, "PresentWithShader: Rendered to BackBuffer");
}

void RendererSubsystem::PresentRenderTarget(int rtIndex, RTType rtType)
{
    // ========================================================================
    // [REFACTORED] Provider-based Architecture
    // ========================================================================
    // 重构说明：
    // - 使用 IRenderTargetProvider 统一接口替代废弃的 m_renderTargetManager
    // - 支持所有四种 RTType（ColorTex, DepthTex, ShadowColor, ShadowTex）
    // - 使用 GetMainResource()/GetAltResource() 获取 ID3D12Resource*
    // - 符合 SOLID 原则（依赖倒置）
    // ========================================================================

    // ========================================================================
    // Step 1: 获取对应的 Provider
    // ========================================================================

    IRenderTargetProvider* provider = GetProvider(rtType);
    if (!provider)
    {
        LogError(LogRenderer, "PresentRenderTarget: Provider is null for RTType %d", static_cast<int>(rtType));
        return;
    }

    // ========================================================================
    // Step 2: 验证 rtIndex 范围
    // ========================================================================

    const int rtCount = provider->GetCount();
    if (rtIndex < 0 || rtIndex >= rtCount)
    {
        LogError(LogRenderer, "PresentRenderTarget: rtIndex %d out of range [0, %d) for RTType %d",
                 rtIndex, rtCount, static_cast<int>(rtType));
        return;
    }

    // ========================================================================
    // Step 3: 根据 FlipState 选择 Main 或 Alt 资源
    // ========================================================================

    ID3D12Resource* sourceRT       = nullptr;
    bool            useAltResource = false;

    // 检查 Provider 是否支持 FlipState
    if (provider->SupportsFlipState())
    {
        // 对于支持 FlipState 的 Provider（ColorTex, ShadowColor）：
        // - 默认状态（未翻转）：使用 Main 资源
        // - 翻转后状态：使用 Alt 资源
        // 注意：这里我们总是使用 Main 资源作为呈现源
        // 因为 Main 是当前帧渲染的目标
        sourceRT = provider->GetMainResource(rtIndex);
    }
    else
    {
        // 对于不支持 FlipState 的 Provider（DepthTex, ShadowTex）：
        // - 只有 Main 资源
        sourceRT = provider->GetMainResource(rtIndex);
    }

    // 验证源资源
    if (!sourceRT)
    {
        LogError(LogRenderer, "PresentRenderTarget: Source resource is null (rtIndex=%d, rtType=%d, useAlt=%d)",
                 rtIndex, static_cast<int>(rtType), useAltResource ? 1 : 0);
        return;
    }

    // ========================================================================
    // Step 4: 获取 BackBuffer 资源
    // ========================================================================

    ID3D12Resource* backBuffer = D3D12RenderSystem::GetBackBufferResource();
    if (!backBuffer)
    {
        LogError(LogRenderer, "PresentRenderTarget: BackBuffer resource is null");
        return;
    }

    // [NOTE] ImGui 渲染在 Present 之前执行
    g_theImGui->Render();

    // ========================================================================
    // Step 5: 获取 CommandList
    // ========================================================================

    ID3D12GraphicsCommandList* cmdList = D3D12RenderSystem::GetCurrentCommandList();
    if (!cmdList)
    {
        LogError(LogRenderer, "PresentRenderTarget: CommandList is null");
        return;
    }

    // ========================================================================
    // Step 6: 确定源资源的初始状态
    // ========================================================================
    // 不同 RTType 的资源可能处于不同的初始状态：
    // - ColorTex/ShadowColor: D3D12_RESOURCE_STATE_RENDER_TARGET
    // - DepthTex/ShadowTex: D3D12_RESOURCE_STATE_DEPTH_WRITE

    D3D12_RESOURCE_STATES sourceInitialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    if (rtType == RTType::DepthTex || rtType == RTType::ShadowTex)
    {
        sourceInitialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }

    // ========================================================================
    // Step 7: 创建资源屏障数组
    // ========================================================================

    D3D12_RESOURCE_BARRIER barriers[2];

    // barriers[0]: sourceRT (初始状态 → COPY_SOURCE)
    barriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barriers[0].Transition.pResource   = sourceRT;
    barriers[0].Transition.StateBefore = sourceInitialState;
    barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    // barriers[1]: backBuffer (RENDER_TARGET → COPY_DEST)
    barriers[1].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barriers[1].Transition.pResource   = backBuffer;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    // ========================================================================
    // Step 8: 执行 GPU 拷贝
    // ========================================================================

    // 8.1 转换资源状态
    D3D12RenderSystem::TransitionResources(cmdList, barriers, 2, "PresentRenderTarget::PreCopy");

    // 8.2 执行拷贝
    cmdList->CopyResource(backBuffer, sourceRT);

    // ========================================================================
    // Step 9: 恢复资源状态
    // ========================================================================

    // barriers[0]: COPY_SOURCE → 初始状态
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.StateAfter  = sourceInitialState;

    // barriers[1]: COPY_DEST → RENDER_TARGET
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;

    D3D12RenderSystem::TransitionResources(cmdList, barriers, 2, "PresentRenderTarget::PostCopy");

    // ========================================================================
    // Step 10: 输出日志
    // ========================================================================

    const char* rtTypeName = "Unknown";
    switch (rtType)
    {
    case RTType::ColorTex: rtTypeName = "colortex";
        break;
    case RTType::DepthTex: rtTypeName = "depthtex";
        break;
    case RTType::ShadowColor: rtTypeName = "shadowcolor";
        break;
    case RTType::ShadowTex: rtTypeName = "shadowtex";
        break;
    }

    LogInfo(LogRenderer, "PresentRenderTarget: Successfully copied %s%d to BackBuffer",
            rtTypeName, rtIndex);
}

void RendererSubsystem::PresentCustomTexture(std::shared_ptr<D12Texture> texture)
{
    // TODO: 实现需要完善资源访问机制
    UNUSED(texture);
    LogWarn(LogRenderer, "PresentCustomTexture: Not implemented yet (需要完善资源访问机制)");
}

void RendererSubsystem::SetBlendMode(BlendMode mode)
{
    // 避免重复设置
    if (m_currentBlendMode == mode)
    {
        return;
    }

    m_currentBlendMode = mode;
    LogDebug(LogRenderer, "SetBlendMode: Blend mode updated to {}", static_cast<int>(mode));
}

void RendererSubsystem::SetDepthMode(DepthMode mode)
{
    // 避免重复设置
    if (m_currentDepthMode == mode)
    {
        return;
    }

    m_currentDepthMode = mode;
    LogDebug(LogRenderer, "SetDepthMode: Depth mode updated to {}", static_cast<int>(mode));
}

void RendererSubsystem::SetStencilTest(const StencilTestDetail& detail)
{
    m_currentStencilTest = detail;

    // [IMPORTANT] Stencil configuration is part of PSO (immutable state)
    // Changing it requires PSO rebuild. Next UseProgram() will create new PSO
    // with updated stencil settings via PSOManager.

    LogDebug(LogRenderer, "SetStencilTest: Stencil state updated (enable={})", detail.enable);
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
    auto* cmdList = D3D12RenderSystem::GetCurrentCommandList();
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

void RendererSubsystem::BindRenderTargets(const std::vector<std::pair<RTType, int>>& targets)
{
    m_renderTargetBinder->BindRenderTargets(targets);
}

size_t RendererSubsystem::GetCurrentVertexOffset() const noexcept
{
    // [NEW] Delegate to VertexRingBuffer wrapper
    // Returns 0 if RingBuffer not yet initialized
    if (m_immediateVertexRingBuffer)
    {
        return m_immediateVertexRingBuffer->GetCurrentOffset();
    }
    return 0;
}

size_t RendererSubsystem::GetCurrentIndexOffset() const noexcept
{
    // [NEW] Delegate to IndexRingBuffer wrapper
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

    LogInfo(LogRenderer, "DrawVertexArray:: called, count=%zu, offset=%zu",
            count, m_immediateVertexRingBuffer->GetCurrentOffset());

    // [NEW] Use RingBuffer wrapper API (Option D Architecture)
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

    LogInfo(LogRenderer, "[2-PARAM] DrawVertexArray called, vertexCount=%zu, indexCount=%zu", vertexCount, indexCount);

    if (!vertices || vertexCount == 0 || !indices || indexCount == 0)
    {
        ERROR_RECOVERABLE("DrawVertexArray:: Invalid vertex or index data")
        return;
    }

    // [NEW] Use RingBuffer wrapper API (Option D Architecture)
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

void RendererSubsystem::ClearRenderTarget(RTType rtType, int rtIndex, const Rgba8& clearColor)
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

    // Step 1: 验证 RTType 是否支持 RTV Clear
    if (rtType == RTType::DepthTex || rtType == RTType::ShadowTex)
    {
        LogError(LogRenderer, "ClearRenderTarget: RTType %d is depth-based, use ClearDepthStencil instead",
                 static_cast<int>(rtType));
        return;
    }

    // Step 2: 获取 CommandList
    auto* cmdList = D3D12RenderSystem::GetCurrentCommandList();
    if (!cmdList)
    {
        LogError(LogRenderer, "ClearRenderTarget: No active CommandList");
        return;
    }

    // Step 3: 获取对应的 Provider
    IRenderTargetProvider* provider = GetProvider(rtType);
    if (!provider)
    {
        LogError(LogRenderer, "ClearRenderTarget: Provider is null for RTType %d", static_cast<int>(rtType));
        return;
    }

    // Step 4: 验证 rtIndex 范围
    const int rtCount = provider->GetCount();
    if (rtIndex < 0 || rtIndex >= rtCount)
    {
        LogError(LogRenderer, "ClearRenderTarget: rtIndex %d out of range [0, %d) for RTType %d",
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
    const char* rtTypeName = (rtType == RTType::ColorTex) ? "colortex" : "shadowcolor";
    LogDebug(LogRenderer, "ClearRenderTarget: Cleared %s%d to RGBA(%u,%u,%u,%u)",
             rtTypeName, rtIndex, clearColor.r, clearColor.g, clearColor.b, clearColor.a);
}

void RendererSubsystem::ClearDepthStencil(uint32_t depthIndex, float clearDepth, uint8_t clearStencil)
{
    // Get active CommandList
    auto* cmdList = D3D12RenderSystem::GetCurrentCommandList();
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

void RendererSubsystem::ClearAllRenderTargets(const Rgba8& clearColor)
{
    // Clear all active colortex (0 to gbufferColorTexCount-1)
    for (int i = 0; i < m_configuration.gbufferColorTexCount; ++i)
    {
        ClearRenderTarget(RTType::ColorTex, i, clearColor);
    }

    // Clear all 3 depthtex (0 to 2)
    for (uint32_t i = 0; i < 3; ++i)
    {
        ClearDepthStencil(i, 1.0f, 0);
    }

    // [FIX] Clear shadowtex (0 to 1) - shadow depth buffers
    // Without this, shadow depth test always fails (depth buffer contains garbage/0.0)
    if (m_shadowTextureProvider)
    {
        float clearDepth = 1.0f;
        for (int i = 0; i < m_shadowTextureProvider->GetCount(); ++i)
        {
            m_shadowTextureProvider->Clear(i, &clearDepth);
        }
    }

    // [FIX] Clear shadowcolor (0 to 1) - shadow color buffers
    if (m_shadowColorProvider)
    {
        for (int i = 0; i < m_shadowColorProvider->GetCount(); ++i)
        {
            ClearRenderTarget(RTType::ShadowColor, i, clearColor);
        }
    }
}

#pragma endregion
