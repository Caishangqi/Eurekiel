#include "RendererSubsystem.hpp"

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Core/Yaml.hpp" // Milestone 3.0 任务3: YAML配置读取
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Graphic/Resource/Buffer/D12VertexBuffer.hpp"
#include "Engine/Graphic/Resource/Buffer/D12IndexBuffer.hpp"
#include "Engine/Graphic/Target/RenderTargetManager.hpp" // Milestone 3.0: GBuffer配置API

using namespace enigma::graphic;
enigma::graphic::RendererSubsystem* g_theRendererSubsystem = nullptr;

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

    // 调用D3D12RenderSystem的完整初始化，包括SwapChain创建
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

    m_isInitialized = true;
    LogInfo(LogRenderer, "D3D12RenderSystem initialized successfully through RendererSubsystem");

    // 显示架构流程确认信息
    LogInfo(LogRenderer, "Initialization flow: RendererSubsystem → D3D12RenderSystem → SwapChain creation completed");
}

void RendererSubsystem::Startup()
{
    LogInfo(LogRenderer, "Starting up...");

    // ==================== ShaderPack Initialization (Dual ShaderPack Architecture - Phase 5.2) ====================
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
    LogInfo(LogRenderer,
            "Loading engine default ShaderPack from: {}",
            engineDefaultPath.string().c_str());

    try
    {
        m_defaultShaderPack = std::make_unique<ShaderPack>(engineDefaultPath);

        if (!m_defaultShaderPack || !m_defaultShaderPack->IsValid())
        {
            ERROR_AND_DIE(Stringf("Engine default ShaderPack loaded but validation failed! Path: %s",
                engineDefaultPath.string().c_str()).c_str());
        }

        LogInfo(LogRenderer,
                "Engine default ShaderPack loaded successfully");
    }
    catch (const std::exception& e)
    {
        ERROR_AND_DIE(Stringf("Failed to load engine default ShaderPack! Path: %s, Error: %s",
            engineDefaultPath.string().c_str(), e.what()).c_str());
    }

    // Step 4: Verify at least one ShaderPack is available
    // Teaching Note: m_defaultShaderPack is always required, m_userShaderPack is optional
    if (!m_defaultShaderPack)
    {
        ERROR_AND_DIE("Both user and engine default ShaderPacks failed to load!");
    }

    // Step 5: Log final ShaderPack system status
    LogInfo(LogRenderer,
            "ShaderPack system initialized (User: {}, Default: {})",
            m_userShaderPack ? "+" : "-",
            m_defaultShaderPack ? "+" : "-");

    // TODO (M2): Initialize flexible rendering architecture
    // Teaching Note: New architecture (BeginFrame/Render/EndFrame + 60+ API)
    // - No PipelineManager (旧架构已删除)
    // - Direct ShaderPack usage (dual pack: user + default)
    // - Implement: BeginCamera, UseProgram, DrawVertexBuffer等
    // =========================================================================================

    // TODO: M2 - 实现新的灵活渲染架构
    // 替代旧的Phase系统（IWorldRenderingPipeline/PipelineManager已删除）

    // ==================== 创建RenderCommandQueue (Milestone 3.1 新增) ====================
    // 初始化immediate模式渲染指令队列
    if (m_configuration.enableImmediateMode)
    {
        try
        {
            RenderCommandQueue::QueueConfig queueConfig;
            queueConfig.maxCommandsPerPhase       = m_configuration.maxCommandsPerPhase;
            queueConfig.enablePhaseDetection      = m_configuration.enablePhaseDetection;
            queueConfig.enableDebugLogging        = true; // 开发阶段启用调试日志
            queueConfig.enablePerformanceCounters = m_configuration.enableCommandProfiling;

            m_renderCommandQueue = std::make_unique<RenderCommandQueue>(queueConfig);
            m_renderCommandQueue->Initialize();

            LogInfo(LogRenderer, "RenderCommandQueue created and initialized (double-buffered, lock-free)");
        }
        catch (const std::exception& e)
        {
            LogError(LogRenderer, "RenderCommandQueue creation exception: %s", e.what());
            m_renderCommandQueue.reset();
        }
    }
    else
    {
        LogInfo(LogRenderer, "Immediate mode rendering is disabled (configuration)");
    }

    // ==================== 创建RenderTargetManager (Milestone 3.0任务4) ====================
    // 初始化GBuffer管理器 - 管理16个colortex RenderTarget（Iris兼容）
    try
    {
        LogInfo(LogRenderer, "Creating RenderTargetManager...");

        // Step 1: 准备RenderTargetSettings配置数组（16个）
        // 参考Iris的colortex配置，使用RGBA16F（高精度）和RGBA8（辅助数据）
        std::array<RenderTargetSettings, 16> rtSettings = {};

        // colortex0-7: RGBA16F格式（高精度渲染，适合HDR/法线/位置等）
        for (int i = 0; i < 8; ++i)
        {
            rtSettings[i].format            = DXGI_FORMAT_R16G16B16A16_FLOAT; // RGBA16F
            rtSettings[i].widthScale        = 1.0f; // 全分辨率
            rtSettings[i].heightScale       = 1.0f;
            rtSettings[i].enableMipmap      = false; // Milestone 3.0: 默认关闭Mipmap
            rtSettings[i].allowLinearFilter = true;
            rtSettings[i].sampleCount       = 1; // 无MSAA
            rtSettings[i].debugName         = "colortex"; // RenderTargetManager会自动追加索引
        }

        // colortex8-15: RGBA8格式（辅助数据，节省内存）
        for (int i = 8; i < 16; ++i)
        {
            rtSettings[i].format            = DXGI_FORMAT_R8G8B8A8_UNORM; // RGBA8
            rtSettings[i].widthScale        = 1.0f;
            rtSettings[i].heightScale       = 1.0f;
            rtSettings[i].enableMipmap      = false;
            rtSettings[i].allowLinearFilter = true;
            rtSettings[i].sampleCount       = 1;
            rtSettings[i].debugName         = "colortex"; // RenderTargetManager会自动追加索引
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
            rtSettings,
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
                 "Failed to create RenderTargetManager: {}",
                 e.what());
        ERROR_AND_DIE(Stringf("RenderTargetManager initialization failed! Error: %s", e.what()).c_str());
    }
}

void RendererSubsystem::Shutdown()
{
    LogInfo(LogRenderer, "Shutting down...");

    // Step 1: Unload ShaderPack before other resources (Phase 5.3)
    // Teaching Note: Iris destroys ShaderPack in destroyEverything()
    // Reference: Iris.java Line 576-593
    UnloadShaderPack();

    // TODO: M2 - 清理新的灵活渲染架构资源

    // Step 3: 最后关闭D3D12RenderSystem
    D3D12RenderSystem::Shutdown();
}

// TODO: M2 - 移除PreparePipeline，实现新的灵活渲染接口

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
    // 2. 获取/切换当前维度的 Pipeline
    // 3. 清屏操作 (对应 Minecraft CLEAR 注入点之前)
    // ========================================================================
    // 教学要点: BeginFrame 不包含 Iris 的任何渲染阶段
    // - BeginLevelRendering 在 EndFrame 中调用 (AFTER CLEAR)
    // - Setup/Begin 阶段也在 EndFrame 中调用
    // ========================================================================

    // 1. DirectX 12 帧准备 - 获取下一帧的后台缓冲区
    D3D12RenderSystem::PrepareNextFrame();
    LogDebug(LogRenderer, "BeginFrame - D3D12 next frame prepared");

    // TODO: M2 - 准备当前维度的渲染资源
    // 替代 PreparePipeline 调用
    LogDebug(LogRenderer, "BeginFrame - Render resources prepared for current dimension");

    // 3. 执行清屏操作 (对应 Minecraft CLEAR 注入点)
    // 教学要点: 这是 MixinLevelRenderer 中 CLEAR target 所在位置
    if (m_configuration.enableAutoClearColor)
    {
        // TODO (Milestone 3.2): 调用 D3D12RenderSystem::ClearRenderTarget()
        bool success = D3D12RenderSystem::BeginFrame(
            m_configuration.defaultClearColor, // 配置的默认颜色
            m_configuration.defaultClearDepth, // 深度清除值
            m_configuration.defaultClearStencil // 模板清除值
        );

        if (!success)
        {
            LogWarn(LogRenderer, "BeginFrame - D3D12 frame clear failed");
        }

        LogDebug(LogRenderer, "BeginFrame - Render target cleared");
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
std::filesystem::path RendererSubsystem::SelectShaderPackPath() const
{
    // Priority 1: User selected pack (from config file or future GUI)
    if (!m_currentPackName.empty())
    {
        std::filesystem::path userPackPath =
            std::filesystem::path(USER_SHADERPACK_SEARCH_PATH) / m_currentPackName;

        if (std::filesystem::exists(userPackPath))
        {
            // User pack found, use it
            return userPackPath;
        }

        // User pack not found, log warning and fall back
        LogWarn(LogRenderer,
                "User ShaderPack '{}' not found at '{}', falling back to engine default",
                m_currentPackName.c_str(),
                userPackPath.string().c_str());
    }

    // Priority 2: Engine default pack (always exists after build)
    // Teaching Note:
    // - Engine assets are copied from Engine/.enigma/ to Run/.enigma/ during build
    // - This ensures the default pack is always available
    return std::filesystem::path(ENGINE_DEFAULT_SHADERPACK_PATH);
}

//-----------------------------------------------------------------------------------------------
void RendererSubsystem::EndFrame()
{
    // ========================================================================
    // ✅ SIMPLIFIED (2025-10-21): 简单委托给 D3D12RenderSystem
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

    // ✅ 简单委托给 D3D12RenderSystem（完整的 EndFrame 逻辑）
    D3D12RenderSystem::EndFrame();

    LogInfo(LogRenderer, "RendererSubsystem::EndFrame() completed");

    // TODO (M2): 恢复 RenderCommandQueue 管理（在新的灵活渲染架构中）
    // - 双缓冲队列交换
    // - Setup/Begin/End 阶段
    // - WorldRenderingPhase 执行
    // - 当前简化版本只负责委托，业务逻辑在 D3D12RenderSystem
}

//-----------------------------------------------------------------------------------------------
// ShaderPack Lifecycle Management (Phase 5.2)
// Teaching Note: Based on Iris.java lifecycle methods
// Reference: F:\github\Iris\common\src\main\java\net\irisshaders\iris\Iris.java
//-----------------------------------------------------------------------------------------------

bool RendererSubsystem::LoadShaderPack(const std::string& packPath)
{
    // Teaching Note: Public interface - converts string to filesystem::path
    // Delegates to private LoadShaderPackInternal() for actual loading
    return LoadShaderPackInternal(std::filesystem::path(packPath));
}

bool RendererSubsystem::LoadShaderPackInternal(const std::filesystem::path& packPath)
{
    // Teaching Note: Based on Iris.java::loadExternalShaderpack() implementation
    // Reference: Iris.java Line 248-348
    // Phase 5.2: Load user ShaderPack only (engine default ShaderPack remains unchanged)

    // Step 1: Unload existing user ShaderPack (if any)
    // Teaching Note: Only unload user pack, keep engine default pack intact
    if (m_userShaderPack)
    {
        LogInfo(LogRenderer, "Unloading previous user ShaderPack before attempting new load");
        m_userShaderPack.reset();
    }

    // Step 2: Validate path exists
    if (!std::filesystem::exists(packPath))
    {
        LogError(LogRenderer,
                 "ShaderPack path does not exist: {}", packPath.string().c_str());
        return false;
    }

    // Step 3: Validate ShaderPack structure (has "shaders/" subdirectory)
    // Bug Fix (2025-10-19): This validation is redundant
    // ShaderPack constructor will validate and error out if shaders/ directory is missing
    // Teaching Note: Iris uses isValidShaderpack() to check for "shaders/" directory
    // Reference: Iris.java Line 467-506
    std::filesystem::path shadersDir = packPath / "shaders";
    if (!std::filesystem::exists(shadersDir) || !std::filesystem::is_directory(shadersDir))
    {
        LogError(LogRenderer,
                 "Invalid ShaderPack structure - missing 'shaders/' directory: {}",
                 packPath.string().c_str());
        return false;
    }

    // Step 4: Load new user ShaderPack
    // Teaching Note: Iris constructs ShaderPack with path, user options, and environment defines
    // Reference: Iris.java Line 324 (new ShaderPack(...))
    try
    {
        LogInfo(LogRenderer,
                "Loading user ShaderPack from: {}", packPath.string().c_str());

        // Bug Fix (2025-10-19): Pass ShaderPack root directory, NOT "shaders/" subdirectory
        // ShaderPack constructor expects the root directory containing "shaders/" folder
        // It will internally append "shaders/" when accessing files
        // Previous bug: Passing shadersDir caused path duplication (.../shaders/shaders/...)
        m_userShaderPack = std::make_unique<ShaderPack>(packPath);

        // Step 5: Validate ShaderPack loaded successfully
        // Teaching Note: Check both pointer and IsValid() for robustness
        if (!m_userShaderPack || !m_userShaderPack->IsValid())
        {
            LogError(LogRenderer, "User ShaderPack loaded but validation failed");
            m_userShaderPack.reset();
            return false;
        }

        // Step 6: Log success
        LogInfo(LogRenderer,
                "User ShaderPack loaded successfully: {}", packPath.string().c_str());

        // TODO (M2): Rebuild rendering resources with new ShaderPack
        // Teaching Note: New architecture doesn't need PipelineManager
        // - m_userShaderPack已更新（自动生效）
        // - GetShaderProgram()会自动使用新的user pack
        // - M2实现时：重新编译PSOs（如果需要）

        return true;
    }
    catch (const std::exception& e)
    {
        // Teaching Note: Iris catches all exceptions and shows user-friendly error
        // Reference: Iris.java Line 334-338 (handleException())
        LogError(LogRenderer,
                 "Exception loading user ShaderPack: {}", e.what());
        m_userShaderPack.reset();
        return false;
    }
}

//-----------------------------------------------------------------------------------------------
void RendererSubsystem::UnloadShaderPack()
{
    // Teaching Note: Based on Iris.java::destroyEverything() implementation
    // Reference: Iris.java Line 576-593
    // Phase 5.2: Dual ShaderPack system - unload both user and default packs

    bool hadUserPack    = (m_userShaderPack != nullptr);
    bool hadDefaultPack = (m_defaultShaderPack != nullptr);

    if (!hadUserPack && !hadDefaultPack)
    {
        LogWarn(LogRenderer, "No ShaderPack to unload");
        return;
    }

    // ✅ IMPROVED: Clarify we're only unloading user ShaderPack
    LogInfo(LogRenderer, "Unloading user ShaderPack (engine default remains)");

    // TODO (M2): Clean up rendering resources before unloading ShaderPack
    // Teaching Note: New architecture uses direct ShaderPack management
    // - Release PSOs (Pipeline State Objects) - M2实现时
    // - Clear shader program references
    // - Unbind all descriptor heap slots (Bindless resources)
    // - No PipelineManager (旧架构已删除)

    // ✅ Release User ShaderPack (if loaded)
    if (m_userShaderPack)
    {
        LogInfo(LogRenderer, "Unloading user ShaderPack");
        m_userShaderPack.reset();
    }

    // ✅ FIXED (Phase 5.2 - Bug Fix 2025-10-19): Do NOT unload engine default ShaderPack
    // Teaching Note: Engine default ShaderPack is PERSISTENT across user pack changes
    // - It provides fallback shader programs when user pack is not loaded
    // - Only destroyed during Shutdown() (engine termination)
    // - Iris equivalent: Default internal shaders always available
    //
    // Dual ShaderPack Architecture:
    // - User ShaderPack: Unloaded here (m_userShaderPack.reset())
    // - Default ShaderPack: Persistent (unloaded only in Shutdown)

    LogInfo(LogRenderer, "ShaderPack(s) unloaded successfully");
    // Note: m_defaultShaderPack remains valid (fallback layer)
}

//-----------------------------------------------------------------------------------------------
bool RendererSubsystem::ReloadShaderPack()
{
    // Teaching Note: Based on Iris.java::reload() implementation
    // Reference: Iris.java Line 556-571

    LogInfo(LogRenderer, "Reloading ShaderPack (hot-reload)");

    // Step 1: Get current ShaderPack path (before unloading)
    // Teaching Note: Iris uses SelectShaderPackPath() to resolve path priority
    std::filesystem::path currentPath = SelectShaderPackPath();

    // Step 2: Unload current ShaderPack
    // Teaching Note: Iris calls destroyEverything() first
    // Reference: Iris.java Line 561
    UnloadShaderPack();

    // Step 3: Reload from same path
    // Teaching Note: Iris calls loadShaderpack() after destruction
    // Reference: Iris.java Line 564
    bool success = LoadShaderPackInternal(currentPath);

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
// GBuffer配置API (Milestone 3.0 任务2)
// Teaching Note: 提供运行时配置GBuffer colortex数量的能力，优化内存使用
//-----------------------------------------------------------------------------------------------

void RendererSubsystem::ConfigureGBuffer(int colorTexCount)
{
    // 参数验证 - 使用RenderTargetManager的常量
    constexpr int MIN = RenderTargetManager::MIN_COLOR_TEXTURES;
    constexpr int MAX = RenderTargetManager::MAX_COLOR_TEXTURES;

    if (colorTexCount < MIN || colorTexCount > MAX)
    {
        LogError(LogRenderer,
                 "ConfigureGBuffer: colorTexCount {} out of range [{}, {}]. Using default 8.",
                 colorTexCount, MIN, MAX);
        colorTexCount = 8; // Milestone 3.0: 使用默认值8（平衡内存和功能）
    }

    // Milestone 3.0: 更新配置对象（从m_config改为m_configuration）
    m_configuration.gbufferColorTexCount = colorTexCount;

    // 计算内存优化百分比
    float optimization = (1.0f - static_cast<float>(m_configuration.gbufferColorTexCount) / static_cast<float>(MAX)) * 100.0f;

    LogInfo(LogRenderer,
            "GBuffer configured: {} colortex (max: {}). Memory optimization: ~{:.1f}%",
            m_configuration.gbufferColorTexCount, MAX, optimization);
}

//-----------------------------------------------------------------------------------------------
// M2 灵活渲染接口实现 (Milestone 2)
// Teaching Note: 提供高层API，封装DirectX 12复杂性
// Reference: DX11Renderer.cpp中的类似方法
//-----------------------------------------------------------------------------------------------

void RendererSubsystem::BeginCamera(const Camera& camera)
{
    UNUSED(camera);
    // TODO (M2.1): 实现Camera设置
    // 1. 提取View和Projection矩阵
    // 2. 更新Camera Uniform Buffer（通过UniformManager）
    // 3. 对应Iris的setCamera()
    LogWarn(LogRenderer, "BeginCamera: Not implemented yet (M2.1)");
}

void RendererSubsystem::EndCamera()
{
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
    auto vertexBuffer = D3D12RenderSystem::CreateVertexBufferTyped(size, stride, nullptr, "AppVertexBuffer");

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

    // 教学要点：
    // - 非索引绘制，使用当前绑定的VertexBuffer
    // - 委托给D3D12RenderSystem的底层Draw API
    // - 对应OpenGL的glDrawArrays

    D3D12RenderSystem::Draw(vertexCount, startVertex);

    LogDebug(LogRenderer,
             "Draw: Drew {} vertices starting from vertex {}",
             vertexCount, startVertex);
}

void RendererSubsystem::DrawIndexed(uint32_t indexCount, uint32_t startIndex, int32_t baseVertex)
{
    if (indexCount == 0)
    {
        LogWarn(LogRenderer, "DrawIndexed: indexCount is 0, nothing to draw");
        return;
    }

    // 教学要点：
    // - 索引绘制，使用当前绑定的IndexBuffer和VertexBuffer
    // - 委托给D3D12RenderSystem的底层DrawIndexed API
    // - 对应OpenGL的glDrawElements

    D3D12RenderSystem::DrawIndexed(indexCount, startIndex, baseVertex);

    LogDebug(LogRenderer,
             "DrawIndexed: Drew {} indices starting from index {} with base vertex {}",
             indexCount, startIndex, baseVertex);
}

void RendererSubsystem::DrawFullscreenQuad()
{
    // TODO (M2.3): 实现全屏四边形绘制
    // 1. 创建或使用缓存的全屏四边形VertexBuffer
    // 2. 绑定并绘制（6个顶点，2个三角形）
    // 教学要点：常用于后处理Pass
    LogWarn(LogRenderer, "DrawFullscreenQuad: Not implemented yet (M2.3)");
}

void RendererSubsystem::SetBlendMode(BlendMode mode)
{
    UNUSED(mode);
    // TODO (M2.4): 实现BlendMode设置
    // 1. 需要PSO支持（Graphics Pipeline State）
    // 2. 在Milestone 3.0 PSO系统实现后启用
    // 教学要点：DirectX 12的混合状态是PSO的一部分，不能动态修改
    LogWarn(LogRenderer, "SetBlendMode: Not implemented yet (M2.4, requires PSO system)");
}

void RendererSubsystem::SetDepthMode(DepthMode mode)
{
    UNUSED(mode);
    // TODO (M2.4): 实现DepthMode设置
    // 1. 需要PSO支持（Graphics Pipeline State）
    // 2. 在Milestone 3.0 PSO系统实现后启用
    // 教学要点：DirectX 12的深度状态是PSO的一部分，不能动态修改
    LogWarn(LogRenderer, "SetDepthMode: Not implemented yet (M2.4, requires PSO system)");
}
