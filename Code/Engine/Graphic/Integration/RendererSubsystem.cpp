#include "RendererSubsystem.hpp"

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Core/Yaml.hpp" // Milestone 3.0 任务3: YAML配置读取
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Core/ImGui/ImGuiSubsystem.hpp"
#include "Engine/Graphic/Resource/Buffer/D12VertexBuffer.hpp"
#include "Engine/Graphic/Resource/Buffer/D12IndexBuffer.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Target/RenderTargetManager.hpp" // Milestone 3.0: GBuffer配置API
#include "Engine/Graphic/Target/RenderTargetHelper.hpp" // 阶段3.3: RenderTarget工具类
#include "Engine/Graphic/Target/D12RenderTarget.hpp" // D12RenderTarget类定义
#include "Engine/Graphic/Target/DepthTextureManager.hpp" // 深度纹理管理器
#include "Engine/Graphic/Target/ShadowColorManager.hpp" // Shadow Color管理器
#include "Engine/Graphic/Target/ShadowTargetManager.hpp" // Shadow Target管理器
#include "Engine/Graphic/Target/RenderTargetBinder.hpp" // RenderTarget绑定器
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp" // MatricesUniforms结构体
#include "Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp" // PerObjectUniforms结构体
#include "Engine/Graphic/Shader/Uniform/CustomImageManager.hpp" // CustomImageManager类
#include "Engine/Graphic/Shader/PSO/PSOManager.hpp" // PSO管理器
// [DELETE] Removed PSOStateCollector.hpp - class deleted after refactor
#include "Engine/Graphic/Shader/PSO/RenderStateValidator.hpp" // 渲染状态验证器
#include "Engine/Graphic/Camera/EnigmaCamera.hpp" // EnigmaCamera支持
#include "Engine/Graphic/Shader/ShaderPack/ShaderProgram.hpp" // M6.2: ShaderProgram
#include "Engine/Graphic/Shader/ShaderPack/ShaderSource.hpp" // Shrimp Task 1: ShaderSource
#include "Engine/Graphic/Shader/ShaderPack/ShaderProgramBuilder.hpp" // Shrimp Task 1: ShaderProgramBuilder
#include "Engine/Graphic/Shader/Common/ShaderCompilationHelper.hpp" // Shrimp Task 2: 编译辅助工具
#include "Engine/Graphic/Shader/Common/ShaderIncludeHelper.hpp" // Shrimp Task 6: Include系统工具
#include "Engine/Graphic/Shader/ShaderPack/Include/ShaderPath.hpp" // Shrimp Task 6: ShaderPath路径抽象
#include "Engine/Graphic/Shader/ShaderPack/ShaderPackHelper.hpp" // 阶段2.3: ShaderPackHelper工具类
#include "Engine/Graphic/Resource/Buffer/BufferHelper.hpp" // 阶段2.4: BufferHelper工具类 [REFACTOR 2025-01-06]
#include "Engine/Graphic/Integration/ImmediateDrawHelper.hpp" // [NEW] Immediate draw helper
#include "Engine/Graphic/Integration/DrawBindingHelper.hpp" // [NEW] Draw binding helper
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

    // ==================== Initialize ShaderCache ====================
    // Create ShaderCache and register ProgramId mapping
    m_shaderCache = std::make_unique<ShaderCache>();

    //Register 91 ProgramId mappings (using ProgramIdToSourceName function)
    std::unordered_map<ProgramId, std::string> programIdMappings;
    for (int i = 0; i < static_cast<int>(ProgramId::COUNT); ++i)
    {
        ProgramId   id   = static_cast<ProgramId>(i);
        std::string name = ProgramIdToSourceName(id);
        if (name != "unknown")
        {
            programIdMappings[id] = name;
        }
    }

    m_shaderCache->RegisterProgramIds(programIdMappings);
    LogInfo(LogRenderer, "ShaderCache initialized with %zu ProgramId mappings",
            m_shaderCache->GetProgramIdCount());

    // Display infrastructure flow confirmation information
    LogInfo(LogRenderer, "Initialization flow: RendererSubsystem → D3D12RenderSystem → SwapChain creation completed");
    g_theRendererSubsystem = this;
}

void RendererSubsystem::Startup()
{
    LogInfo(LogRenderer, "Starting up...");

    // ==================== ShaderPack Initialization (Dual ShaderPack Architecture) ====================
    // Architecture (2025-10-19): Dual ShaderPack system with in-memory fallback
    // - m_userShaderPack: User-downloaded shader packs (optional, may be null)
    // - m_defaultShaderPack: Engine built-in shaders (required, always valid)
    // - Fallback: User → Engine Default → ERROR_AND_DIE()

    // Step 1: Initialize current pack name from Configuration (unified config interface)
    m_currentPackName = m_configuration.currentShaderPackName;

    // Step 2: Attempt to load User ShaderPack (if specified in configuration)
    if (!m_currentPackName.empty())
    {
        std::filesystem::path userPackPath = std::filesystem::path(USER_SHADERPACK_SEARCH_PATH) / m_currentPackName;

        if (std::filesystem::exists(userPackPath))
        {
            LogInfo(LogRenderer,
                    "Loading user ShaderPack '{}' from: {}",
                    m_currentPackName.c_str(),
                    userPackPath.string().c_str());

            // Load user ShaderPack (may fail without crashing)
            try
            {
                m_userShaderPack = std::make_unique<ShaderPack>(userPackPath);

                if (!m_userShaderPack || !m_userShaderPack->IsValid())
                {
                    LogError(LogRenderer,
                             "User ShaderPack loaded but validation failed: {}",
                             m_currentPackName.c_str());
                    m_userShaderPack.reset();
                }
                else
                {
                    LogInfo(LogRenderer,
                            "User ShaderPack loaded successfully: {}",
                            m_currentPackName.c_str());
                }
            }
            catch (const std::exception& e)
            {
                LogError(LogRenderer,
                         "Exception loading user ShaderPack '{}': {}",
                         m_currentPackName.c_str(), e.what());
                m_userShaderPack.reset();
            }
        }
        else
        {
            LogWarn(LogRenderer,
                    "User ShaderPack '{}' not found at '{}', using engine default only",
                    m_currentPackName.c_str(),
                    userPackPath.string().c_str());
        }
    }

    // Step 3: ALWAYS load Engine Default ShaderPack (required fallback)
    std::filesystem::path engineDefaultPath(ENGINE_DEFAULT_SHADERPACK_PATH);
    LogInfo(LogRenderer, "Loading engine default ShaderPack from: {}", engineDefaultPath.string().c_str());

    try
    {
        m_defaultShaderPack = std::make_unique<ShaderPack>(engineDefaultPath);

        if (!m_defaultShaderPack || !m_defaultShaderPack->IsValid())
        {
            LogError("Engine default ShaderPack loaded but validation failed! Path: %s",
                     engineDefaultPath.string().c_str());
            ERROR_AND_DIE(Stringf("Engine default ShaderPack loaded but validation failed! Path: %s",
                engineDefaultPath.string().c_str()))
        }

        LogInfo(LogRenderer,
                "Engine default ShaderPack loaded successfully");
    }
    catch (const std::exception& e)
    {
        LogError("Failed to load engine default ShaderPack! Path: %s, Error: %s",
                 engineDefaultPath.string().c_str(), e.what());
        ERROR_AND_DIE(Stringf("Failed to load engine default ShaderPack! Path: %s, Error: %s",
            engineDefaultPath.string().c_str(), e.what()))
    }

    // Step 4: Verify at least one ShaderPack is available
    // Teaching Note: m_defaultShaderPack is always required, m_userShaderPack is optional
    if (!m_defaultShaderPack)
    {
        LogError(LogRenderer, "Both user and engine default ShaderPacks failed to load!");
        ERROR_AND_DIE("Both user and engine default ShaderPacks failed to load!")
    }

    // Step 5: Log final ShaderPack system status
    LogInfo(LogRenderer,
            "ShaderPack system initialized (User: {}, Default: {})",
            m_userShaderPack ? "+" : "-",
            m_defaultShaderPack ? "+" : "-");

    // Step 6: 从引擎默认ShaderPack加载ShaderSource到ShaderCache (Shrimp Task)
    // 缓存默认ProgramSet的ShaderSource
    if (m_shaderCache && m_defaultShaderPack)
    {
        size_t totalSourcesCached = 0;

        // 获取默认ProgramSet（world0）
        const ProgramSet* programSet = m_defaultShaderPack->GetProgramSet();
        if (programSet)
        {
            // 获取所有单一程序（ProgramId对应的ShaderSource）
            const auto& singlePrograms = programSet->GetPrograms();
            for (const auto& [id, source] : singlePrograms)
            {
                if (source && source->IsValid())
                {
                    std::string name = m_shaderCache->GetProgramName(id);
                    if (!name.empty())
                    {
                        // 使用空deleter从unique_ptr创建shared_ptr（不转移所有权）
                        m_shaderCache->CacheSource(name, std::shared_ptr<ShaderSource>(source.get(), [](ShaderSource*)
                        {
                        }));
                        ++totalSourcesCached;
                    }
                }
            }
        }

        LogInfo(LogRenderer, "ShaderCache: Loaded %zu ShaderSources from engine default ShaderPack",
                totalSourcesCached);
    }
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

        LogInfo(LogRenderer,
                "RenderTargetManager configuration: %dx%d, %d colortex (max 16)",
                baseWidth, baseHeight, colorTexCount);

        // Step 3: 创建RenderTargetManager实例（RAII原则，无需单独Initialize）
        m_renderTargetManager = std::make_unique<RenderTargetManager>(
            baseWidth,
            baseHeight,
            rtConfigs,
            colorTexCount
        );

        // Step 4: 日志输出成功信息
        LogInfo(LogRenderer,
                "RenderTargetManager created successfully (%dx%d, %d colortex)",
                baseWidth, baseHeight, colorTexCount);

        // Step 5: 输出内存优化信息（仅当colorTexCount < 16时）
        if (colorTexCount < RenderTargetManager::MAX_COLOR_TEXTURES)
        {
            const float optimization = (1.0f - static_cast<float>(colorTexCount) /
                static_cast<float>(RenderTargetManager::MAX_COLOR_TEXTURES)) * 100.0f;
            LogInfo(LogRenderer,
                    "Memory optimization enabled: ~%.1f%% saved (%d colortex instead of 16)",
                    optimization, colorTexCount);
        }
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer,
                 "Failed to create RenderTargetManager: %s",
                 e.what());
        ERROR_AND_DIE(Stringf("RenderTargetManager initialization failed! Error: %s", e.what()))
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
            10000 // maxDrawsPerFrame
        );

        LogInfo(LogRenderer, "Matrices Ring Buffer allocated: 10000 × 1280 bytes = 12.8 MB");

        // ==================== Register PerObjectUniforms Ring Buffer ====================
        // Register PerObjectUniforms as PerObject Buffer, allocate 10000 draws
        // Teaching points:
        // 1. PerObjectUniforms original size 128 bytes (2 Mat44), 256 bytes after alignment
        // 2. 10000 × 256 = 2.5 MB (reasonable memory overhead)
        // 3. This is the second PerObject Buffer (the first is MatricesUniforms)
        // 4. [NEW] Explicitly specify slot 1
        m_uniformManager->RegisterBuffer<PerObjectUniforms>(
            1, // registerSlot: Slot 1 for PerObjectUniforms (Iris-compatible)
            UpdateFrequency::PerObject,
            10000 // maxDrawsPerFrame, consistent with Matrices
        );

        // ==================== Register CustomImageUniform Ring Buffer ====================
        // Register CustomImageUniform as PerObject Buffer, allocate 10000 draws
        // Teaching points:
        // 1. CustomImageUniform size 64 bytes (16 × uint32_t), 256 bytes after alignment
        // 2. 10000 × 256 = 2.5 MB (reasonable memory overhead)
        // 3. This is the third PerObject Buffer (after MatricesUniforms and PerObjectUniforms)
        // 4. [NEW] Explicitly specify slot 2 for CustomImage
        m_uniformManager->RegisterBuffer<CustomImageUniform>(
            2, // registerSlot: Slot 2 for CustomImage (Iris-compatible)
            UpdateFrequency::PerObject,
            10000 // maxDrawsPerFrame, consistent with other PerObject buffers
        );

        LogInfo(LogRenderer, "CustomImageUniform Ring Buffer registered: slot 2, 10000 × 256 bytes = 2.5 MB");
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "Failed to create UniformManager: {}", e.what());
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

    // ==================== 创建DepthTextureManager ====================
    try
    {
        LogInfo(LogRenderer, "Creating DepthTextureManager...");

        std::array<DepthTextureConfig, 3> depthConfigs = {};
        for (int i = 0; i < 3; ++i)
        {
            depthConfigs[i].width        = m_configuration.renderWidth;
            depthConfigs[i].height       = m_configuration.renderHeight;
            depthConfigs[i].format       = DepthFormat::D24_UNORM_S8_UINT;
            depthConfigs[i].semanticName = "depthtex" + std::to_string(i);
        }

        m_depthTextureManager = std::make_unique<DepthTextureManager>(
            m_configuration.renderWidth,
            m_configuration.renderHeight,
            depthConfigs,
            3
        );
        LogInfo(LogRenderer, "DepthTextureManager created successfully");
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "Failed to create DepthTextureManager: {}", e.what());
        ERROR_AND_DIE(Stringf("DepthTextureManager initialization failed! Error: %s", e.what()))
    }

    // ==================== 创建ShadowColorManager (M6.1.5) ====================
    try
    {
        LogInfo(LogRenderer, "Creating ShadowColorManager...");

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

        m_shadowColorManager = std::make_unique<ShadowColorManager>(shadowColorConfigs);
        LogInfo(LogRenderer, "ShadowColorManager created successfully");
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "Failed to create ShadowColorManager: {}", e.what());
        ERROR_AND_DIE(Stringf("ShadowColorManager initialization failed! Error: %s", e.what()))
    }

    // ==================== 创建ShadowTargetManager (M6.1.5) ====================
    try
    {
        LogInfo(LogRenderer, "Creating ShadowTargetManager...");

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

        m_shadowTargetManager = std::make_unique<ShadowTargetManager>(shadowTexConfigs);
        LogInfo(LogRenderer, "ShadowTargetManager created successfully");
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "Failed to create ShadowTargetManager: {}", e.what());
        ERROR_AND_DIE(Stringf("ShadowTargetManager initialization failed! Error: %s", e.what()))
    }

    // ==================== 创建RenderTargetBinder (M6.1.5) ====================
    try
    {
        LogInfo(LogRenderer, "Creating RenderTargetBinder...");

        m_renderTargetBinder = std::make_unique<RenderTargetBinder>(
            m_renderTargetManager.get(),
            m_depthTextureManager.get(),
            m_shadowColorManager.get(),
            m_shadowTargetManager.get()
        );

        LogInfo(LogRenderer, "RenderTargetBinder created successfully");
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "Failed to create RenderTargetBinder: %s", e.what());
    }

    // ==================== 创建全屏三角形VB (M6.3) ====================
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
}

void RendererSubsystem::Shutdown()
{
    LogInfo(LogRenderer, "Shutting down...");

    // Step 1: Unload ShaderPack before other resources (Phase 5.3)
    // Teaching Note: Iris destroys ShaderPack in destroyEverything()
    // Reference: Iris.java Line 576-593
    UnloadShaderPack();

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

    // [NEW] 重置immediate buffer offset（Per-Frame Append策略）
    m_currentVertexOffset = 0;
    m_currentIndexOffset  = 0;

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

void RendererSubsystem::RenderFrame()
{
    // ========================================================================
    // Pipeline 生命周期重构 - RenderFrame 阶段 (Game Update)
    // ========================================================================
    // 职责: 游戏逻辑的 Update 阶段 (对应 Minecraft 的 tick/update)
    // - 上传 Buffer 修改 (Uniform 更新)
    // - 材质修改 (纹理加载/卸载)
    // - 几何体数据更新
    // - 提交 Immediate 指令到 RenderCommandQueue
    // ========================================================================
    // 教学要点: 这个阶段是 Game 线程提交渲染指令的时机
    // - Game 线程写入 submitQueue (双缓冲的另一半)
    // - Render 线程读取 executeQueue (上一帧的指令)
    // - SwapBuffers() 在下一帧 EndFrame 开始时调用
    // ========================================================================

    // TODO (Milestone 3.2): 游戏逻辑更新
    // - 更新实体位置
    // - 更新方块状态
    // - 提交几何体绘制指令

    // TODO (Milestone 3.2): Uniform 变量更新
    // - 更新摄像机矩阵
    // - 更新时间变量
    // - 更新光照参数

    // TODO (Milestone 3.2): 纹理资源更新
    // - 加载新的纹理
    // - 卸载未使用的纹理
    // - 更新纹理动画

    LogDebug(LogRenderer, "RenderFrame - Game update completed (commands submitted to queue)");
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

#pragma region ShaderPack Management

//-----------------------------------------------------------------------------------------------
// Shrimp Task 2: CreateShaderProgramFromFiles 核心 API
// Teaching Note: 独立着色器编译接口，解决 Game.cpp:28 的 TODO 问题
//-----------------------------------------------------------------------------------------------

std::shared_ptr<ShaderProgram> RendererSubsystem::CreateShaderProgramFromFiles(
    const std::filesystem::path& vsPath,
    const std::filesystem::path& psPath,
    const std::string&           programName
)
{
    // ✅ 修复：创建配置并传递入口点
    ShaderCompileOptions options = ShaderCompileOptions::WithCommonInclude();
    options.entryPoint           = m_configuration.shaderEntryPoint; // 传递配置的入口点

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
    // Step 4: 检查缓存 (Shrimp Task)
    // ========================================================================
    if (m_shaderCache && m_shaderCache->HasProgram(programName))
    {
        LogWarn(LogRenderer, "ShaderProgram '%s' already exists in cache, returning cached version",
                programName.c_str());
        return m_shaderCache->GetProgram(programName);
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

    // ========================================================================
    // Step 6: 缓存 ShaderProgram (Shrimp Task)
    // ========================================================================
    if (m_shaderCache)
    {
        m_shaderCache->CacheProgram(programName, program);
        LogInfo(LogRenderer, "ShaderProgram '%s' cached successfully", programName.c_str());
    }

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
                LogWarn(LogRenderer, "IncludeGraph has {} failures:", graph->GetFailures().size());
                for (const auto& [path, error] : graph->GetFailures())
                {
                    LogWarn(LogRenderer, "  - {}: {}", path.GetPathString().c_str(), error.c_str());
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

    // ========================================================================
    // Step 4: 提取程序名称（如果未提供）
    // ========================================================================
    std::string finalProgramName = programName.empty()
                                       ? ShaderCompilationHelper::ExtractProgramNameFromPath(vsPath)
                                       : programName;

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

//-----------------------------------------------------------------------------------------------
// ShaderPack Query and Fallback (Phase 5.2)
//-----------------------------------------------------------------------------------------------

const ShaderSource* RendererSubsystem::GetShaderSource(ProgramId id) const noexcept
{
    // Delegate to GetShaderProgram() for dual ShaderPack fallback (Phase 5.2)
    // Teaching Note: GetShaderProgram() handles user -> default fallback automatically
    return GetShaderProgram(id, "world0");
}

//----------------------------------------------------------------------------------------------------------------------
// Get shader program with dual ShaderPack fallback mechanism (Phase 5.2 - 2025-10-19)
// Priority: User ShaderPack → Engine Default ShaderPack → nullptr (3-step fallback)
//
// Teaching Note:
// - KISS principle: Simple 3-step lookup, no complex file copying
// - In-memory fallback: Check user pack first, then engine default
// - Iris-compatible: Matches NewWorldRenderingPipeline::getShaderProgram() behavior
// - User ShaderPack can partially override engine defaults
//----------------------------------------------------------------------------------------------------------------------
const ShaderSource* RendererSubsystem::GetShaderProgram(ProgramId id, const std::string& dimension) const noexcept
{
    // Priority 1: Try user ShaderPack first (if loaded)
    if (m_userShaderPack)
    {
        const ProgramSet* userProgramSet = m_userShaderPack->GetProgramSet(dimension);
        if (userProgramSet)
        {
            std::optional<const ShaderSource*> userSource = userProgramSet->Get(id);
            if (userSource && *userSource && (*userSource)->IsValid())
            {
                // User shader found and valid - use it
                return *userSource;
            }
        }
    }

    // Priority 2: Fallback to engine default ShaderPack (always loaded)
    if (m_defaultShaderPack)
    {
        const ProgramSet* defaultProgramSet = m_defaultShaderPack->GetProgramSet(dimension);
        if (defaultProgramSet)
        {
            std::optional<const ShaderSource*> defaultSource = defaultProgramSet->Get(id);
            if (defaultSource && *defaultSource && (*defaultSource)->IsValid())
            {
                // Engine default shader found - use as fallback
                return *defaultSource;
            }
        }
    }

    // Priority 3: Complete failure - program not found in both packs
    // Teaching Note: Return nullptr (caller should handle gracefully)
    // Iris behavior: Returns null and logs error in NewWorldRenderingPipeline::getShaderProgram()
    return nullptr;
}

//-----------------------------------------------------------------------------------------------
// GetShaderProgram(ProgramId) - 从ShaderCache获取编译后的ShaderProgram (Shrimp Task)
//-----------------------------------------------------------------------------------------------
std::shared_ptr<ShaderProgram> RendererSubsystem::GetShaderProgram(ProgramId id)
{
    if (!m_shaderCache)
    {
        LogError(LogRenderer, "ShaderCache not initialized");
        return nullptr;
    }

    // 从ShaderCache获取ShaderProgram（支持ProgramId查找）
    return m_shaderCache->GetProgram(id);
}

//-----------------------------------------------------------------------------------------------
// ShaderPack Lifecycle Management (Phase 5.2)
// Teaching Note: Based on Iris.java lifecycle methods
// Reference: F:\github\Iris\common\src\main\java\net\irisshaders\iris\Iris.java
//-----------------------------------------------------------------------------------------------

bool RendererSubsystem::LoadShaderPack(const std::string& packPath)
{
    // Teaching Note: Public interface - converts string to filesystem::path
    // Delegates to ShaderPackHelper for actual loading
    auto result = ShaderPackHelper::LoadShaderPackFromPath(std::filesystem::path(packPath), m_shaderCache.get());
    return result != nullptr;
}

void RendererSubsystem::UnloadShaderPack()
{
}

// 阶段2.3: LoadShaderPackInternal, UnloadShaderPack, SelectShaderPackPath已移至ShaderPackHelper
#pragma endregion

#pragma region Rendering API
// TODO: M2 - 实现60+ API灵活渲染接口
// BeginCamera/EndCamera, UseProgram, DrawVertexBuffer等
// 替代旧的Phase系统（已删除IWorldRenderingPipeline/PipelineManager）
#pragma endregion

#pragma region RenderTarget Management
// TODO: M2 - 实现RenderTarget管理接口
// ConfigureGBuffer, FlipRenderTarget, SwitchDepthBuffer等
#pragma endregion

#pragma region State Management
//-----------------------------------------------------------------------------------------------
bool RendererSubsystem::ReloadShaderPack()
{
    // Teaching Note: Based on Iris.java::reload() implementation
    // Reference: Iris.java Line 556-571

    LogInfo(LogRenderer, "Reloading ShaderPack (hot-reload)");

    // Step 1: Get current ShaderPack path (before unloading)
    // Teaching Note: Iris uses SelectShaderPackPath() to resolve path priority
    std::string selectedPath = ShaderPackHelper::SelectShaderPackPath(
        m_currentPackName,
        m_configuration.shaderPackSearchPath,
        m_configuration.engineDefaultShaderPackPath
    );
    std::filesystem::path currentPath(selectedPath);

    // Step 2: Unload current ShaderPack
    // Teaching Note: Iris calls destroyEverything() first
    // Reference: Iris.java Line 561
    UnloadShaderPack();

    // Step 3: Reload from same path
    // Teaching Note: Iris calls loadShaderpack() after destruction
    // Reference: Iris.java Line 564
    auto result  = ShaderPackHelper::LoadShaderPackFromPath(currentPath, m_shaderCache.get());
    bool success = result != nullptr;

    if (success)
    {
        LogInfo(LogRenderer, "ShaderPack reloaded successfully");

        // TODO (Phase 5.4): Immediately rebuild pipeline for current dimension
        // Teaching Note: Iris re-creates pipeline IMMEDIATELY after reload
        // Reference: Iris.java Line 568-570
        // - Check if in game world (Minecraft.getInstance().level != null)
        // - Call preparePipeline(getCurrentDimension())
        // - CRITICAL: Prevents "extremely dangerous" inconsistent state (Iris warning)
    }
    else
    {
        LogError(LogRenderer, "Failed to reload ShaderPack");

        // TODO (Phase 6+): Implement fallback to engine default
        // Teaching Note: Iris calls setShadersDisabled() on failure
        // Reference: Iris.java Line 224
        // - Attempt to load ENGINE_DEFAULT_SHADERPACK_PATH
        // - If that fails too, disable shaders completely
    }

    return success;
}

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

//-----------------------------------------------------------------------------------------------
// GBuffer配置API (Milestone 3.0 任务2)
// Teaching Note: 提供运行时配置GBuffer colortex数量的能力，优化内存使用
//-----------------------------------------------------------------------------------------------

void RendererSubsystem::ConfigureGBuffer(int colorTexCount)
{
    // 阶段3.3: 使用RenderTargetHelper进行配置验证
    constexpr int MAX = RenderTargetManager::MAX_COLOR_TEXTURES;

    // 1. 验证RT配置
    auto validationResult = RenderTargetHelper::ValidateRTConfiguration(colorTexCount, MAX);
    if (!validationResult.isValid)
    {
        LogError(LogRenderer, "Invalid RT config: {}", validationResult.errorMessage.c_str());
        colorTexCount = 8; // 使用默认值8
        LogWarn(LogRenderer, "Using default colorTexCount: 8");
    }

    // 2. 计算内存使用量
    size_t memoryUsage = RenderTargetHelper::CalculateRTMemoryUsage(
        m_configuration.renderWidth,
        m_configuration.renderHeight,
        colorTexCount,
        DXGI_FORMAT_R8G8B8A8_UNORM
    );

    // 3. 更新配置对象
    m_configuration.gbufferColorTexCount = colorTexCount;

    // 4. 计算内存优化百分比
    float optimization = (1.0f - static_cast<float>(colorTexCount) / static_cast<float>(MAX)) * 100.0f;

    // 5. 输出配置信息
    LogInfo(LogRenderer,
            "GBuffer configured: {} colortex (max: {}). Memory: {:.2f} MB. Optimization: ~{:.1f}%",
            colorTexCount, MAX, memoryUsage / (1024.0f * 1024.0f), optimization);
}

//-----------------------------------------------------------------------------------------------
// CustomImage管理API实现
// Teaching Note: 提供高层API，封装CustomImageManager的实现细节
//-----------------------------------------------------------------------------------------------

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
// M6.2.1: UseProgram RT绑定（两种方式）
//-----------------------------------------------------------------------------------------------

void RendererSubsystem::UseProgram(std::shared_ptr<ShaderProgram> shaderProgram, const std::vector<uint32_t>& rtOutputs, int depthIndex)
{
    if (!shaderProgram)
    {
        LogError(LogRenderer, "UseProgram: shaderProgram is nullptr");
        return;
    }

    // ========================================================================
    // [REFACTORED] UseProgram - PSO延迟绑定架构 (Phase 2)
    // ========================================================================
    // 职责：
    // 1. 缓存当前ShaderProgram状态
    // 2. 绑定RenderTarget（三种模式）
    // 
    // PSO创建和绑定已移至Draw()方法（延迟到实际绘制时执行）
    // ========================================================================

    // 1. 缓存当前ShaderProgram（供后续Draw()使用）
    m_currentShaderProgram = shaderProgram.get();

    // 2. 确定RT输出：优先使用rtOutputs参数，否则读取DRAWBUFFERS指令
    const auto& drawBuffers = rtOutputs.empty() ? shaderProgram->GetDirectives().GetDrawBuffers() : rtOutputs;

    // 3. RT绑定逻辑（三种模式）
    auto cmdList = D3D12RenderSystem::GetCurrentCommandList();
    if (!cmdList)
    {
        LogError(LogRenderer, "UseProgram: CommandList is nullptr");
        return;
    }

    if (drawBuffers.empty())
    {
        // Mode B: 空数组绑定SwapChain后台缓冲区
        auto rtvHandle = D3D12RenderSystem::GetBackBufferRTV();
        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
        LogDebug(LogRenderer, "UseProgram: Bound SwapChain as default RT (Mode B)");

        // 清除RenderTargetBinder状态缓存
        if (m_renderTargetBinder)
        {
            m_renderTargetBinder->ClearBindings();
        }
    }
    else if (m_renderTargetBinder)
    {
        // Mode A/C: 绑定指定的RenderTarget
        std::vector<RTType> rtTypes(drawBuffers.size(), RTType::ColorTex);
        std::vector<int>    indices(drawBuffers.begin(), drawBuffers.end());

        std::vector<ClearValue> clearValues(drawBuffers.size(), ClearValue::Color(Rgba8::BLACK));
        m_renderTargetBinder->BindRenderTargets(
            rtTypes, indices,
            RTType::DepthTex, depthIndex,
            false,
            LoadAction::Load,
            clearValues,
            ClearValue::Depth(1.0f, 0)
        );

        // 刷新RT绑定（延迟提交）
        m_renderTargetBinder->FlushBindings(cmdList);
    }

    LogDebug(LogRenderer, "UseProgram: ShaderProgram cached, RenderTargets bound (PSO deferred to Draw)");
}

void RendererSubsystem::UseProgram(ProgramId programId, const std::vector<uint32_t>& rtOutputs)
{
    UNUSED(rtOutputs)
    // 当前简化实现：直接从ShaderPack获取
    const ShaderSource* shaderSource = GetShaderProgram(programId, "world0");
    if (!shaderSource)
    {
        LogError(LogRenderer, "UseProgram(ProgramId): Failed to get ShaderSource for ProgramId");
        return;
    }

    // TODO: 编译ShaderSource为ShaderProgram（M6.2后续任务）
    // 当前暂时无法调用，等待ShaderProgram编译系统实现
    LogWarn(LogRenderer, "UseProgram(ProgramId): ShaderProgram compilation not implemented yet");
}

//-----------------------------------------------------------------------------------------------
// M6.2.3: FlipRenderTarget API
//-----------------------------------------------------------------------------------------------

void RendererSubsystem::FlipRenderTarget(int rtIndex)
{
    if (!m_renderTargetManager)
    {
        LogError(LogRenderer, "FlipRenderTarget: RenderTargetManager is nullptr");
        return;
    }

    m_renderTargetManager->FlipRenderTarget(rtIndex);
}

void RendererSubsystem::BeginCamera(const EnigmaCamera& camera)
{
    LogInfo(LogRenderer, "BeginCamera(EnigmaCamera) - Start setting camera parameters");

    // 验证UniformManager是否已初始化
    if (!m_uniformManager)
    {
        LogError(LogRenderer, "BeginCamera(EnigmaCamera): UniformManager is not initialized");
        ERROR_AND_DIE("UniformManager is not initialized")
    }

    // 验证渲染系统是否准备就绪
    if (!IsReadyForRendering())
    {
        LogError(LogRenderer, "BeginCamera(EnigmaCamera): The rendering system is not ready");
        ERROR_AND_DIE("The rendering system is not ready")
    }

    try
    {
        // ========================================================================
        // 1. 从EnigmaCamera读取数据并设置到UniformManager
        // ========================================================================

        // [TODO] 设置相机位置 (cameraPosition - CameraAndPlayerUniforms)
        // CameraAndPlayerUniforms 尚未注册到UniformManager，暂时保留旧API
        // m_uniformManager->Uniform3f("cameraPosition", camera.GetPosition());

        // ========================================================================
        // 2. [REFACTORED] 使用新的UploadBuffer API上传Camera矩阵
        // ========================================================================
        // 重构说明：
        // - 旧API: 使用 UniformMat4() 逐个上传矩阵，然后调用 SyncToGPU()
        // - 新API: 使用 UploadBuffer<MatricesUniforms>() 一次性上传所有矩阵数据
        // - 优点: 类型安全、减少API调用、自动同步
        // - 配合: Geometry::Render 使用新API上传Model矩阵，两者共同填充MatricesUniforms

        // 读取Camera矩阵
        Mat44 cameraToWorld    = camera.GetCameraToWorldTransform(); // Camera → World (逆矩阵)
        Mat44 worldToCamera    = camera.GetWorldToCameraTransform(); // World → Camera
        Mat44 cameraToRender   = camera.GetCameraToRenderTransform(); // Camera → Render (坐标系转换)
        Mat44 projectionMatrix = camera.GetProjectionMatrix(); // Render → Clip

        // 计算逆矩阵
        Mat44 worldToCameraInverse    = worldToCamera.GetOrthonormalInverse();
        Mat44 projectionMatrixInverse = projectionMatrix.GetOrthonormalInverse();

        // 填充MatricesUniforms的Camera相关字段
        MatricesUniforms matData;

        // GBuffer相关矩阵（延迟渲染主Pass）- 完整4矩阵变换链
        matData.gbufferModelView         = worldToCamera; // World → Camera
        matData.gbufferModelViewInverse  = cameraToWorld; // Camera → World
        matData.cameraToRenderTransform  = cameraToRender; // Camera → Render
        matData.gbufferProjection        = projectionMatrix; // Render → Clip
        matData.gbufferProjectionInverse = projectionMatrixInverse; // Clip → Render

        // 通用矩阵（当前几何体 - 默认使用相同的Camera矩阵）
        matData.modelViewMatrix         = worldToCamera;
        matData.modelViewMatrixInverse  = worldToCameraInverse;
        matData.projectionMatrix        = projectionMatrix;
        matData.projectionMatrixInverse = projectionMatrixInverse;

        // 上传到GPU（新API - 类型安全，自动同步）
        m_uniformManager->UploadBuffer<MatricesUniforms>(matData);

        // Shadow管理由Game侧负责，引擎侧只提供接口
        // 理由：不是每个场景都需要shadow，且光照类型多样（sun light, point light, spot light等）
        //
        // 游戏侧使用方式：
        // if (sceneHasSunLight) {
        //     auto shadowMatrices = CalculateSunLightShadows(camera);
        //     renderer->SetShadowMatrices(shadowMatrices.modelView, shadowMatrices.projection);
        // } else {
        //     renderer->ClearShadowMatrices();
        // }


        // ========================================================================
        // 3. 设置视口（如果需要）
        // ========================================================================

        // TODO: 设置视口 - 需要根据EnigmaCamera的viewport设置
        // 可能需要调用 D3D12RenderSystem::SetViewport() 或类似API

        LogInfo(LogRenderer, "BeginCamera(EnigmaCamera) - Camera parameter settings are completed");
        LogInfo(LogRenderer, " - Camera position: (%f, %f, %f)",
                camera.GetPosition().x, camera.GetPosition().y, camera.GetPosition().z);
        LogInfo(LogRenderer, " - Camera mode: %s",
                camera.IsPerspective() ? "Perspective projection" : "Orthogonal projection");

        // TODO: 添加更多调试信息（如FOV、近平面、远平面等）
        if (camera.IsPerspective())
        {
            LogInfo(LogRenderer, " - FOV: %f degrees, aspect ratio: %f",
                    camera.GetPerspectiveFOV(), camera.GetPerspectiveAspect());
        }
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "BeginCamera(EnigmaCamera): Exception - %s", e.what());
    }
}

void RendererSubsystem::EndCamera(const EnigmaCamera& camera)
{
    UNUSED(camera);
    // TODO (M2.1): 实现Camera清理
    LogWarn(LogRenderer, "EndCamera: Not implemented yet (M2.1)");
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
             buffer->GetIndexCount(),
             buffer->GetFormat() == D12IndexBuffer::IndexFormat::Uint16 ? "Uint16" : "Uint32");
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

    // ========================================================================
    // [REFACTORED] Draw Execution - Inline PSO State + DrawBindingHelper
    // ========================================================================
    // Changes:
    // 1. [DELETE] Removed PSOStateCollector::CollectCurrentState call
    // 2. [NEW] Inline struct construction for DrawState
    // 3. [NEW] Use DrawBindingHelper for resource binding
    // ========================================================================

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

    // Step 3: Inline PSO state construction (replaces PSOStateCollector)
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

    // Step 4: Validate Draw state
    const char* errorMsg = nullptr;
    if (!RenderStateValidator::ValidateDrawState(state, &errorMsg))
    {
        LogError(LogRenderer, "Draw validation failed: %s", errorMsg ? errorMsg : "Unknown error");
        return;
    }

    // Step 5: Get or create PSO
    ID3D12PipelineState* pso = m_psoManager->GetOrCreatePSO(
        state.program,
        state.rtFormats.data(),
        state.depthFormat,
        state.blendMode,
        state.depthMode,
        state.stencilDetail,
        state.rasterizationConfig
    );

    // Step 6: Bind PSO (avoid redundant binding)
    if (pso != m_lastBoundPSO)
    {
        cmdList->SetPipelineState(pso);
        m_lastBoundPSO = pso;
    }

    // Step 7: Bind Root Signature
    if (m_currentShaderProgram)
    {
        m_currentShaderProgram->Use(cmdList);
    }

    // Step 8: Set Primitive Topology
    cmdList->IASetPrimitiveTopology(state.topology);

    // Step 9: Bind Engine Buffers (slots 0-14) - Use DrawBindingHelper
    DrawBindingHelper::BindEngineBuffers(cmdList, m_uniformManager.get());

    // Step 10: Bind Custom Buffer Table (slot 15) - Use DrawBindingHelper
    DrawBindingHelper::BindCustomBufferTable(cmdList, m_uniformManager.get());

    // Step 11: Issue Draw call
    cmdList->DrawInstanced(vertexCount, 1, startVertex, 0);

    // Step 12: Increment draw count
    if (m_uniformManager)
    {
        m_uniformManager->IncrementDrawCount();
    }

    LogDebug(LogRenderer,
             "Draw: Drew %u vertices starting from vertex %u",
             vertexCount, startVertex);
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

    // ========================================================================
    // [REFACTORED] DrawIndexed Execution - Inline PSO State + DrawBindingHelper
    // ========================================================================
    // Changes:
    // 1. [DELETE] Removed PSOStateCollector::CollectCurrentState call
    // 2. [NEW] Inline struct construction for DrawState
    // 3. [NEW] Use DrawBindingHelper for resource binding
    // ========================================================================

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

    // Step 3: Inline PSO state construction (replaces PSOStateCollector)
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

    // Step 4: Validate Draw state
    const char* errorMsg = nullptr;
    if (!RenderStateValidator::ValidateDrawState(state, &errorMsg))
    {
        LogError(LogRenderer, "DrawIndexed validation failed: %s", errorMsg ? errorMsg : "Unknown error");
        return;
    }

    // Step 5: Get or create PSO
    ID3D12PipelineState* pso = m_psoManager->GetOrCreatePSO(
        state.program,
        state.rtFormats.data(),
        state.depthFormat,
        state.blendMode,
        state.depthMode,
        state.stencilDetail,
        state.rasterizationConfig
    );

    // Step 6: Bind PSO (avoid redundant binding)
    if (pso != m_lastBoundPSO)
    {
        cmdList->SetPipelineState(pso);
        m_lastBoundPSO = pso;
    }

    // Step 7: Bind Root Signature
    if (m_currentShaderProgram)
    {
        m_currentShaderProgram->Use(cmdList);
    }

    // Step 8: Set Primitive Topology
    cmdList->IASetPrimitiveTopology(state.topology);

    // Step 9: Bind Engine Buffers (slots 0-14) - Use DrawBindingHelper
    DrawBindingHelper::BindEngineBuffers(cmdList, m_uniformManager.get());

    // Step 10: Bind Custom Buffer Table (slot 15) - Use DrawBindingHelper
    DrawBindingHelper::BindCustomBufferTable(cmdList, m_uniformManager.get());

    // Step 11: Issue DrawIndexed call
    D3D12RenderSystem::DrawIndexed(indexCount, startIndex, baseVertex);

    // Step 12: Increment draw count
    if (m_uniformManager)
    {
        m_uniformManager->IncrementDrawCount();
    }

    LogDebug(LogRenderer,
             "DrawIndexed: Drew %u indices starting from index %u with base vertex %d",
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

    // ========================================================================
    // [REFACTORED] DrawInstanced Execution - Inline PSO State + DrawBindingHelper
    // ========================================================================
    // Changes:
    // 1. [DELETE] Removed PSOStateCollector::CollectCurrentState call
    // 2. [NEW] Inline struct construction for DrawState
    // 3. [NEW] Use DrawBindingHelper for resource binding
    // ========================================================================

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

    // Step 3: Inline PSO state construction (replaces PSOStateCollector)
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

    // Step 4: Validate Draw state
    const char* errorMsg = nullptr;
    if (!RenderStateValidator::ValidateDrawState(state, &errorMsg))
    {
        LogError(LogRenderer, "DrawInstanced validation failed: %s", errorMsg ? errorMsg : "Unknown error");
        return;
    }

    // Step 5: Get or create PSO
    ID3D12PipelineState* pso = m_psoManager->GetOrCreatePSO(
        state.program,
        state.rtFormats.data(),
        state.depthFormat,
        state.blendMode,
        state.depthMode,
        state.stencilDetail,
        state.rasterizationConfig
    );

    // Step 6: Bind PSO (avoid redundant binding)
    if (pso != m_lastBoundPSO)
    {
        cmdList->SetPipelineState(pso);
        m_lastBoundPSO = pso;
    }

    // Step 7: Bind Root Signature
    if (m_currentShaderProgram)
    {
        m_currentShaderProgram->Use(cmdList);
    }

    // Step 8: Set Primitive Topology
    cmdList->IASetPrimitiveTopology(state.topology);

    // Step 9: Bind Engine Buffers (slots 0-14) - Use DrawBindingHelper
    DrawBindingHelper::BindEngineBuffers(cmdList, m_uniformManager.get());

    // Step 10: Bind Custom Buffer Table (slot 15) - Use DrawBindingHelper
    DrawBindingHelper::BindCustomBufferTable(cmdList, m_uniformManager.get());

    // Step 11: Issue DrawInstanced call
    cmdList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);

    // Step 12: Increment draw count
    if (m_uniformManager)
    {
        m_uniformManager->IncrementDrawCount();
    }

    LogDebug(LogRenderer,
             "DrawInstanced: Drew %u vertices × %u instances starting from vertex %u, instance %u",
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
    // Step 1: 参数验证
    // ========================================================================

    // 1.1 验证rtType（仅支持RTType::ColorTex）
    if (rtType != RTType::ColorTex)
    {
        char errorMsg[256];
        sprintf_s(errorMsg, sizeof(errorMsg),
                  "PresentRenderTarget: Unsupported RTType (%d), only ColorTex is supported",
                  static_cast<int>(rtType));
        LogWarn(LogRenderer, errorMsg);
        return;
    }

    // 1.2 验证m_renderTargetManager指针非空
    if (!m_renderTargetManager)
    {
        LogError(LogRenderer, "PresentRenderTarget: RenderTargetManager is null");
        return;
    }

    // 1.3 验证rtIndex范围 [0, gbufferColorTexCount)
    const int colorTexCount = m_configuration.gbufferColorTexCount;
    if (rtIndex < 0 || rtIndex >= colorTexCount)
    {
        char errorMsg[256];
        sprintf_s(errorMsg, sizeof(errorMsg),
                  "PresentRenderTarget: rtIndex %d out of range [0, %d)",
                  rtIndex, colorTexCount);
        LogError(LogRenderer, errorMsg);
        return;
    }

    // ========================================================================
    // Step 2: 获取RenderTarget实例
    // ========================================================================

    std::shared_ptr<D12RenderTarget> renderTarget = nullptr;
    try
    {
        renderTarget = m_renderTargetManager->GetRenderTarget(rtIndex);
    }
    catch (const std::exception& e)
    {
        char errorMsg[256];
        sprintf_s(errorMsg, sizeof(errorMsg),
                  "PresentRenderTarget: Failed to get RenderTarget %d: %s",
                  rtIndex, e.what());
        LogError(LogRenderer, errorMsg);
        return;
    }

    // 2.1 检查返回值是否有效
    if (!renderTarget)
    {
        char errorMsg[256];
        sprintf_s(errorMsg, sizeof(errorMsg),
                  "PresentRenderTarget: RenderTarget %d is null",
                  rtIndex);
        LogError(LogRenderer, errorMsg);
        return;
    }

    // ========================================================================
    // Step 3: 根据FlipState选择Main或Alt纹理
    // ========================================================================

    ID3D12Resource* sourceRT  = nullptr;
    bool            isFlipped = m_renderTargetManager->IsFlipped(rtIndex);

    // BufferFlipState逻辑：
    // - IsFlipped() == false: 使用Main纹理（默认状态）
    // - IsFlipped() == true:  使用Alt纹理（翻转后状态）
    if (!isFlipped)
    {
        sourceRT = renderTarget->GetMainTextureResource();
    }
    else
    {
        sourceRT = renderTarget->GetAltTextureResource();
    }

    // 3.1 验证源纹理资源
    if (!sourceRT)
    {
        char errorMsg[256];
        sprintf_s(errorMsg, sizeof(errorMsg),
                  "PresentRenderTarget: Source texture resource is null (rtIndex=%d, isFlipped=%d)",
                  rtIndex, isFlipped ? 1 : 0);
        LogError(LogRenderer, errorMsg);
        return;
    }

    // ========================================================================
    // Step 4: 获取BackBuffer资源
    // ========================================================================

    ID3D12Resource* backBuffer = D3D12RenderSystem::GetBackBufferResource();
    if (!backBuffer)
    {
        LogError(LogRenderer, "PresentRenderTarget: BackBuffer resource is null");
        return;
    }
    g_theImGui->Render();
    // ========================================================================
    // Step 5: 获取CommandList
    // ========================================================================

    ID3D12GraphicsCommandList* cmdList = D3D12RenderSystem::GetCurrentCommandList();
    if (!cmdList)
    {
        LogError(LogRenderer, "PresentRenderTarget: CommandList is null");
        return;
    }

    // ========================================================================
    // Step 6: 创建资源屏障数组
    // ========================================================================
    // 参考 DepthTextureManager.cpp:338-365 的实现模式

    D3D12_RESOURCE_BARRIER barriers[2];

    // barriers[0]: sourceRT (RENDER_TARGET → COPY_SOURCE)
    barriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barriers[0].Transition.pResource   = sourceRT;
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
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
    // Step 7: 执行GPU拷贝
    // ========================================================================

    // 7.1 转换资源状态
    D3D12RenderSystem::TransitionResources(cmdList, barriers, 2, "PresentRenderTarget::PreCopy");

    // 7.2 执行拷贝
    cmdList->CopyResource(backBuffer, sourceRT);

    // ========================================================================
    // Step 8: 恢复资源状态
    // ========================================================================

    // 交换 StateBefore 和 StateAfter
    // barriers[0]: COPY_SOURCE → RENDER_TARGET
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;

    // barriers[1]: COPY_DEST → RENDER_TARGET
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;

    D3D12RenderSystem::TransitionResources(cmdList, barriers, 2, "PresentRenderTarget::PostCopy");

    // ========================================================================
    // Step 9: 输出日志
    // ========================================================================

    char successMsg[256];
    sprintf_s(successMsg, sizeof(successMsg),
              "PresentRenderTarget: Successfully copied colortex%d (%s texture) to BackBuffer",
              rtIndex, isFlipped ? "Alt" : "Main");
    LogInfo(LogRenderer, successMsg);
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

// ============================================================================
// Milestone 4: 深度缓冲管理实现
// ============================================================================

void RendererSubsystem::SwitchDepthBuffer(int newActiveIndex)
{
    // 验证DepthTextureManager已初始化
    if (!m_depthTextureManager)
    {
        LogError(LogRenderer, "SwitchDepthBuffer: DepthTextureManager not initialized");
        return;
    }

    // 委托DepthTextureManager执行切换
    try
    {
        m_depthTextureManager->SwitchDepthBuffer(newActiveIndex);

        const char* depthNames[3] = {"depthtex0", "depthtex1", "depthtex2"};
        LogInfo(LogRenderer, "Switched active depth buffer to {} (index {})",
                depthNames[newActiveIndex], newActiveIndex);
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "SwitchDepthBuffer failed: {}", e.what());
    }
}

void RendererSubsystem::BindRenderTargets(
    const std::vector<RTType>& rtTypes,
    const std::vector<int>&    indices,
    RTType                     depthType,
    int                        depthIndex
)
{
    if (!m_renderTargetBinder)
    {
        LogError(LogRenderer, "BindRenderTargets: RenderTargetBinder not initialized");
        return;
    }

    m_renderTargetBinder->BindRenderTargets(rtTypes, indices, depthType, depthIndex);
}

void RendererSubsystem::CopyDepth(int srcIndex, int dstIndex)
{
    // 验证DepthTextureManager已初始化
    if (!m_depthTextureManager)
    {
        LogError(LogRenderer, "CopyDepthBuffer: DepthTextureManager not initialized");
        return;
    }

    // 委托DepthTextureManager执行复制（内部会获取CommandList）
    try
    {
        m_depthTextureManager->CopyDepth(srcIndex, dstIndex);

        const char* depthNames[3] = {"depthtex0", "depthtex1", "depthtex2"};
        LogInfo(LogRenderer, "Copied depth buffer {} -> {}",
                depthNames[srcIndex], depthNames[dstIndex]);
    }
    catch (const std::exception& e)
    {
        LogError(LogRenderer, "CopyDepthBuffer failed: {}", e.what());
    }
}

int RendererSubsystem::GetActiveDepthBufferIndex() const noexcept
{
    // 验证DepthTextureManager已初始化
    if (!m_depthTextureManager)
    {
        LogWarn(LogRenderer, "GetActiveDepthBufferIndex: DepthTextureManager not initialized, returning 0");
        return 0; // 返回默认值
    }

    // 委托DepthTextureManager查询
    return m_depthTextureManager->GetActiveDepthBufferIndex();
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
    LogInfo(LogRenderer, "[1-PARAM] DrawVertexArray called, count=%zu, VBO addr=%p, offset=%zu",
            count, m_immediateVBO.get(), m_currentVertexOffset);
    // [STEP 3] Append vertex data using ImmediateDrawHelper
    uint32_t startVertex = ImmediateDrawHelper::AppendVertexData(
        m_immediateVBO,
        vertices,
        count,
        m_currentVertexOffset,
        sizeof(Vertex)
    );

    // [STEP 4] Set vertex buffer
    SetVertexBuffer(m_immediateVBO.get());

    // [STEP 9] Issue draw call
    Draw(static_cast<uint32_t>(count), startVertex);
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
    LogInfo(LogRenderer, "[2-PARAM] DrawVertexArray called, vertexCount=%zu, VBO addr=%p, offset=%zu", vertexCount, m_immediateVBO.get(), m_currentVertexOffset);

    // [STEP 1] Prepare CustomImage data before Delayed Fill
    if (m_customImageManager)
    {
        m_customImageManager->PrepareCustomImagesForDraw();
    }

    if (!vertices || vertexCount == 0 || !indices || indexCount == 0)
    {
        ERROR_RECOVERABLE("DrawVertexArray: Invalid vertex or index data");
        return;
    }

    // [STEP 2] Update Ring Buffer offsets (Delayed Fill)
    m_uniformManager->UpdateRingBufferOffsets(UpdateFrequency::PerObject);

    // [STEP 3] Append vertex and index data using ImmediateDrawHelper
    uint32_t baseVertex = ImmediateDrawHelper::AppendVertexData(
        m_immediateVBO,
        vertices,
        vertexCount,
        m_currentVertexOffset,
        sizeof(Vertex)
    );

    uint32_t startIndex = ImmediateDrawHelper::AppendIndexData(
        m_immediateIBO,
        indices,
        indexCount,
        m_currentIndexOffset
    );

    // [STEP 4] Set vertex and index buffers
    SetVertexBuffer(m_immediateVBO.get());
    SetIndexBuffer(m_immediateIBO.get());

    // [STEP 5] Get CommandList and set Root Signature
    auto* cmdList = D3D12RenderSystem::GetCurrentCommandList();
    if (!cmdList)
    {
        ERROR_AND_DIE("DrawVertexArray: CommandList is nullptr")
        return;
    }

    if (m_currentShaderProgram)
    {
        m_currentShaderProgram->Use(cmdList);
    }
    else
    {
        ERROR_RECOVERABLE("DrawVertexArray: No shader program is set");
        return;
    }

    // [STEP 6] Bind Engine Buffer Root CBVs (Slot 0-14) using DrawBindingHelper
    DrawBindingHelper::BindEngineBuffers(cmdList, m_uniformManager.get());

    // [STEP 7] Bind Custom Buffer Descriptor Table (Slot 15) using DrawBindingHelper
    DrawBindingHelper::BindCustomBufferTable(cmdList, m_uniformManager.get());

    // [STEP 8] Set primitive topology (implicit in DrawIndexed)

    // [STEP 9] Issue indexed draw call
    DrawIndexed(static_cast<uint32_t>(indexCount), startIndex, baseVertex);

    // [STEP 10] Increment Draw count for Ring Buffer
    if (m_uniformManager)
    {
        m_uniformManager->IncrementDrawCount();
    }
}

void RendererSubsystem::DrawVertexBuffer(const std::shared_ptr<D12VertexBuffer>& vbo, size_t vertexCount)
{
    if (!vbo || vertexCount == 0)
    {
        ERROR_RECOVERABLE("DrawVertexBuffer: Invalid vertex buffer or count");
        return;
    }

    // [FIX] External VBO data is copied to Ring Buffer to maintain
    // Per-Frame Append strategy consistency. This adds one memcpy but
    // avoids Pipeline Stall from mixed buffer usage.

    // Get vertex data from external VBO
    void* externalData = vbo->GetPersistentMappedData();
    if (!externalData)
    {
        ERROR_RECOVERABLE("DrawVertexBuffer: External VBO has no mapped data");
        return;
    }

    // Append vertex data to Ring Buffer using ImmediateDrawHelper
    uint32_t startVertex = ImmediateDrawHelper::AppendVertexData(
        m_immediateVBO,
        externalData,
        vertexCount,
        m_currentVertexOffset,
        vbo->GetStride()
    );

    // Use m_immediateVBO for IASetVertexBuffers (not external vbo)
    SetVertexBuffer(m_immediateVBO.get());
    Draw(static_cast<uint32_t>(vertexCount), startVertex);
}

void RendererSubsystem::DrawVertexBuffer(const std::shared_ptr<D12VertexBuffer>& vbo, const std::shared_ptr<D12IndexBuffer>& ibo, size_t indexCount)
{
    if (!vbo || !ibo || indexCount == 0)
    {
        ERROR_RECOVERABLE("DrawVertexBuffer: Invalid vertex buffer, index buffer or count");
        return;
    }

    // [FIX] External VBO/IBO data is copied to Ring Buffer to maintain
    // Per-Frame Append strategy consistency. This adds one memcpy but
    // avoids Pipeline Stall from mixed buffer usage.

    // Get vertex data from external VBO
    void* externalVertexData = vbo->GetPersistentMappedData();
    if (!externalVertexData)
    {
        ERROR_RECOVERABLE("DrawVertexBuffer: External VBO has no mapped data");
        return;
    }

    // Get index data from external IBO
    void* externalIndexData = ibo->GetPersistentMappedData();
    if (!externalIndexData)
    {
        ERROR_RECOVERABLE("DrawVertexBuffer: External IBO has no mapped data");
        return;
    }
    // [FIX] Use GetVertexCount() API instead of manual calculation
    size_t vertexCount = vbo->GetVertexCount();

    // Append vertex data to Ring Buffer using ImmediateDrawHelper
    uint32_t startVertex = ImmediateDrawHelper::AppendVertexData(
        m_immediateVBO,
        externalVertexData,
        vertexCount,
        m_currentVertexOffset,
        vbo->GetStride()
    );

    // Append index data to Ring Buffer using ImmediateDrawHelper
    uint32_t startIndex = ImmediateDrawHelper::AppendIndexData(
        m_immediateIBO,
        static_cast<const unsigned*>(externalIndexData),
        indexCount,
        m_currentIndexOffset
    );

    // Use m_immediateVBO and m_immediateIBO for IA calls (not external buffers)
    SetVertexBuffer(m_immediateVBO.get());
    SetIndexBuffer(m_immediateIBO.get());
    DrawIndexed(static_cast<uint32_t>(indexCount), startIndex, static_cast<int32_t>(startVertex));
}

// ============================================================================
// Clear Operations - Flexible RT Management
// ============================================================================

void RendererSubsystem::ClearRenderTarget(uint32_t rtIndex, const Rgba8& clearColor)
{
    // Get active CommandList
    auto* cmdList = D3D12RenderSystem::GetCurrentCommandList();
    if (!cmdList)
    {
        LogError(LogRenderer, "ClearRenderTarget: No active CommandList");
        return;
    }

    // Validate rtIndex range
    if (rtIndex >= static_cast<uint32_t>(m_configuration.gbufferColorTexCount))
    {
        LogError(LogRenderer, "ClearRenderTarget: Invalid rtIndex=%u (max=%d)",
                 rtIndex, m_configuration.gbufferColorTexCount);
        return;
    }

    // Get RTV handle for specified RT
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_renderTargetManager->GetMainRTV(static_cast<int>(rtIndex));

    // Convert Rgba8 to float array
    float clearColorFloat[4] = {
        clearColor.r / 255.0f,
        clearColor.g / 255.0f,
        clearColor.b / 255.0f,
        clearColor.a / 255.0f
    };

    // Clear the RT
    cmdList->ClearRenderTargetView(rtvHandle, clearColorFloat, 0, nullptr);

    LogDebug(LogRenderer, "ClearRenderTarget: Cleared colortex%u to RGBA(%u,%u,%u,%u)",
             rtIndex, clearColor.r, clearColor.g, clearColor.b, clearColor.a);
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
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_depthTextureManager->GetDSV(static_cast<int>(depthIndex));

    // Clear depth and stencil
    cmdList->ClearDepthStencilView(
        dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        clearDepth,
        clearStencil,
        0,
        nullptr
    );
    LogInfo(LogRenderer, "ClearDepthStencil: depthIndex=%u, depth=%.2f, stencil=%u",
            depthIndex, clearDepth, clearStencil);
}

void RendererSubsystem::ClearAllRenderTargets(const Rgba8& clearColor)
{
    // Clear all active colortex (0 to gbufferColorTexCount-1)
    for (int i = 0; i < m_configuration.gbufferColorTexCount; ++i)
    {
        ClearRenderTarget(static_cast<uint32_t>(i), clearColor);
    }

    // Clear all 3 depthtex (0 to 2)
    for (uint32_t i = 0; i < 3; ++i)
    {
        ClearDepthStencil(i, 1.0f, 0);
    }

    LogDebug(LogRenderer, "ClearAllRenderTargets: Cleared %d colortex and 3 depthtex",
             m_configuration.gbufferColorTexCount);
}

#pragma endregion
