#include "RendererSubsystem.hpp"

#include "Engine/Core/Logger/LoggerAPI.hpp"
using namespace enigma::graphic;
enigma::graphic::RendererSubsystem* g_theRendererSubsystem = nullptr;

RendererSubsystem::RendererSubsystem(Configuration& config)
{
    m_configuration = config;
}

RendererSubsystem::~RendererSubsystem()
{
}

void RendererSubsystem::Initialize()
{
    LogInfo(GetStaticSubsystemName(), "Initializing D3D12 rendering system...");

    // 获取窗口句柄（通过配置参数）
    HWND hwnd = nullptr;
    if (m_configuration.targetWindow)
    {
        hwnd = static_cast<HWND>(m_configuration.targetWindow->GetWindowHandle());
        LogInfo(GetStaticSubsystemName(), "Window handle obtained from configuration for SwapChain creation");
    }
    else
    {
        LogWarn(GetStaticSubsystemName(), "No window provided in configuration - initializing in headless mode");
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
        LogError(GetStaticSubsystemName(), "Failed to initialize D3D12RenderSystem");
        m_isInitialized = false;
        return;
    }

    m_isInitialized = true;
    LogInfo(GetStaticSubsystemName(), "D3D12RenderSystem initialized successfully through RendererSubsystem");

    // 显示架构流程确认信息
    LogInfo(GetStaticSubsystemName(), "Initialization flow: RendererSubsystem → D3D12RenderSystem → SwapChain creation completed");
}

void RendererSubsystem::Startup()
{
    LogInfo(GetStaticSubsystemName(), "Starting up...");

    // 创建EnigmaRenderingPipeline实例 (Milestone 2.6新增)
    // 这是验证DirectX 12渲染管线的核心组件

    // 获取CommandListManager (假设D3D12RenderSystem已经创建)
    auto commandManager = D3D12RenderSystem::GetCommandListManager();
    if (!commandManager)
    {
        LogError(GetStaticSubsystemName(), "无法获取CommandListManager，EnigmaRenderingPipeline创建失败");
        return;
    }

    try
    {
        // 创建EnigmaRenderingPipeline实例
        m_currentPipeline = std::make_unique<EnigmaRenderingPipeline>(commandManager);

        LogInfo(GetStaticSubsystemName(), "EnigmaRenderingPipeline was created successfully");

        // 启用调试模式用于开发验证
        m_currentPipeline->SetDebugMode(true);

        LogInfo(GetStaticSubsystemName(), "EnigmaRenderingPipeline debug mode is enabled");
    }
    catch (const std::exception& e)
    {
        LogError(GetStaticSubsystemName(), "EnigmaRenderingPipeline creates exception: {}", e.what());
        m_currentPipeline.reset();
    }

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

            LogInfo(GetStaticSubsystemName(), "RenderCommandQueue created and initialized (double-buffered, lock-free)");
        }
        catch (const std::exception& e)
        {
            LogError(GetStaticSubsystemName(), "RenderCommandQueue creation exception: {}", e.what());
            m_renderCommandQueue.reset();
        }
    }
    else
    {
        LogInfo(GetStaticSubsystemName(), "Immediate mode rendering is disabled (configuration)");
    }
}

void RendererSubsystem::Shutdown()
{
    LogInfo(GetStaticSubsystemName(), "Shutting down...");

    // 清理EnigmaRenderingPipeline (Milestone 2.6新增)
    if (m_currentPipeline)
    {
        LogInfo(GetStaticSubsystemName(), "Destroy EnigmaRenderingPipeline...");
        m_currentPipeline->Destroy();
        m_currentPipeline.reset();
        LogInfo(GetStaticSubsystemName(), "EnigmaRenderingPipeline destroyed");
    }

    // 最后关闭D3D12RenderSystem
    D3D12RenderSystem::Shutdown();
}

EnigmaRenderingPipeline* RendererSubsystem::PreparePipeline(const ResourceLocation& dimensionId)
{
    UNUSED(dimensionId)
    return GetCurrentPipeline();
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
    LogDebug(GetStaticSubsystemName(), "BeginFrame - D3D12 next frame prepared");

    // 2. 获取/切换当前维度的 Pipeline
    // 教学要点: 对应 Iris 的 PipelineManager.preparePipeline(dimensionId)
    PreparePipeline(ResourceLocation("simpleminer", "world"));
    LogDebug(GetStaticSubsystemName(), "BeginFrame - Pipeline prepared for current dimension");

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
            LogWarn(GetStaticSubsystemName(), "BeginFrame - D3D12 frame clear failed");
        }

        LogDebug(GetStaticSubsystemName(), "BeginFrame - Render target cleared");
    }

    LogInfo(GetStaticSubsystemName(), "BeginFrame - Frame preparation completed (ready for game update)");
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

    LogDebug(GetStaticSubsystemName(), "RenderFrame - Game update completed (commands submitted to queue)");
}

void RendererSubsystem::EndFrame()
{
    // ========================================================================
    // Pipeline 生命周期重构 - EndFrame 阶段
    // ========================================================================
    // 职责: 执行完整的 Iris 渲染管线
    // 1. 双缓冲队列交换 (将 Game 线程提交的指令移入执行队列)
    // 2. Setup 阶段 (setup1-99.fsh)
    // 3. BeginLevelRendering (对应 Iris AFTER CLEAR 注入点)
    // 4. 执行所有 WorldRenderingPhase (24个阶段)
    // 5. EndLevelRendering (finalizeLevelRendering)
    // 6. GPU 命令提交和 Present
    // ========================================================================
    // 教学要点: EndFrame 对应 Minecraft renderLevel() 的主体部分
    // - CLEAR 之后开始执行 Iris 渲染管线
    // - Setup 阶段在最开始 (初始化全局 Uniform)
    // - BeginLevelRendering 紧随其后 (begin1-99.fsh)
    // ========================================================================

    if (!m_currentPipeline)
    {
        LogWarn(GetStaticSubsystemName(), "EndFrame - No active pipeline, skipping rendering");
        return;
    }

    // ==================== 阶段 0: 双缓冲队列交换 ====================
    // 教学要点: 将上一帧 Game 线程提交的指令移入执行队列
    // - submitQueue (Game 线程写入) → executeQueue (Render 线程读取)
    // - 无锁双缓冲设计，完全分离 Game 和 Render 线程
    if (m_renderCommandQueue)
    {
        m_renderCommandQueue->SwapBuffers();
        m_renderStatistics.frameIndex++;
        m_renderStatistics.Reset(); // 重置本帧统计
        LogDebug(GetStaticSubsystemName(), "EndFrame - Command queue buffers swapped (frame: {})",
                 m_renderStatistics.frameIndex);
    }

    // ==================== 阶段 1: Setup 阶段 (setup1-99.fsh) ====================
    // 教学要点: Iris 渲染管线的第一个阶段
    // - 初始化全局 Uniform 变量
    // - 设置渲染目标清除值
    // - 准备 Shadow Map 和其他资源
    // TODO (Milestone 3.2): m_currentPipeline->ExecuteSetupStage();
    LogDebug(GetStaticSubsystemName(), "EndFrame - Setup stage completed (setup1-99)");

    // ==================== 阶段 2: BeginLevelRendering (begin1-99.fsh) ====================
    // 教学要点: 对应 Iris 的 beginWorldRender() 调用
    // - 注入点: @Inject(method = "renderLevel", at = @At(value = "INVOKE", target = CLEAR, shift = At.Shift.AFTER))
    // - 位置: Minecraft CLEAR 操作之后
    // - 职责: 执行 begin1-99.fsh 着色器程序
    m_currentPipeline->BeginLevelRendering();
    LogDebug(GetStaticSubsystemName(), "EndFrame - BeginLevelRendering completed (begin1-99)");

    // ==================== 阶段 3: Shadow Pass (shadow.vsh/shadow.fsh) ====================
    // 教学要点: Shadow 阶段不是 WorldRenderingPhase 枚举值
    // - 通过单独的 renderShadows() 方法处理
    // - Iris 使用 ShadowRenderer，独立于 WorldRenderingPhase 系统
    // - 渲染到 Shadow Map (shadowtex0, shadowtex1, shadowcolor0, shadowcolor1)
    m_currentPipeline->RenderShadows();
    LogDebug(GetStaticSubsystemName(), "EndFrame - Shadow pass completed (shadow.vsh/fsh)");

    // ==================== 阶段 4-27: 执行完整的 WorldRenderingPhase (24个阶段) ====================
    // 教学要点: 严格按照 Iris 的 24 个 WorldRenderingPhase 顺序执行
    // 每个 Phase 的执行流程:
    // 1. SetPhase(phase) - 设置当前阶段标志 (零开销，只是flag)
    // 2. CompositeRenderer/DebugRenderer 的 RenderAll() 方法:
    //    - 遍历该 Phase 的所有 Pass (例如 composite1.fsh, composite2.fsh, ...)
    //    - 每个 Pass 执行: 绑定Program → 更新Uniform → Flip Buffer → 渲染全屏四边形
    // 3. 从 executeQueue 提取该 Phase 的所有 Immediate 指令并执行
    // ========================================================================

    // 天空和环境阶段
    const WorldRenderingPhase skyPhases[] = {
        WorldRenderingPhase::SKY,
        WorldRenderingPhase::SUNSET,
        WorldRenderingPhase::CUSTOM_SKY,
        WorldRenderingPhase::SUN,
        WorldRenderingPhase::MOON,
        WorldRenderingPhase::STARS,
        WorldRenderingPhase::VOID_ENV
    };

    for (auto phase : skyPhases)
    {
        // TODO (Milestone 3.2): 实现正确的 Phase 执行流程
        // 1. SetPhase(phase) - 只是设置标志位
        // 2. 找到对应的 Renderer (例如 SkyRenderer)
        // 3. 调用 Renderer->RenderAll() 方法
        //    - RenderAll() 内部 loop 所有 Pass
        //    - 每个 Pass 对应一个着色器程序 (例如 gbuffers_skybasic.vsh/fsh)
        // 4. 从 executeQueue 提取该 Phase 的 Immediate 指令并执行

        if (m_renderCommandQueue && m_renderCommandQueue->GetCommandCount(phase) > 0)
        {
            m_currentPipeline->SetPhase(phase);
            // TODO: m_currentPipeline->ExecutePhaseRenderer(phase);
            LogDebug(GetStaticSubsystemName(), "EndFrame - Phase {} executed ({} commands)",
                     static_cast<uint32_t>(phase), m_renderCommandQueue->GetCommandCount(phase));
        }
    }

    // G-Buffer 填充阶段 (Terrain, Entities, 等)
    const WorldRenderingPhase gbufferPhases[] = {
        WorldRenderingPhase::TERRAIN_SOLID,
        WorldRenderingPhase::TERRAIN_CUTOUT_MIPPED,
        WorldRenderingPhase::TERRAIN_CUTOUT,
        WorldRenderingPhase::ENTITIES,
        WorldRenderingPhase::BLOCK_ENTITIES,
        WorldRenderingPhase::DESTROY
    };

    for (auto phase : gbufferPhases)
    {
        // TODO (Milestone 3.2): 同上，执行对应的 Renderer
        // - TERRAIN_SOLID/CUTOUT/CUTOUT_MIPPED: TerrainRenderer (gbuffers_terrain_solid.vsh/fsh)
        // - ENTITIES: EntityRenderer (gbuffers_entities.vsh/fsh)
        // - BLOCK_ENTITIES: BlockEntityRenderer (gbuffers_block.vsh/fsh)

        if (m_renderCommandQueue && m_renderCommandQueue->GetCommandCount(phase) > 0)
        {
            m_currentPipeline->SetPhase(phase);
            // TODO: m_currentPipeline->ExecutePhaseRenderer(phase);
            LogDebug(GetStaticSubsystemName(), "EndFrame - Phase {} executed ({} commands)",
                     static_cast<uint32_t>(phase), m_renderCommandQueue->GetCommandCount(phase));
        }
    }

    // 半透明和特效阶段
    const WorldRenderingPhase translucentPhases[] = {
        WorldRenderingPhase::TERRAIN_TRANSLUCENT,
        WorldRenderingPhase::TRIPWIRE,
        WorldRenderingPhase::PARTICLES,
        WorldRenderingPhase::CLOUDS,
        WorldRenderingPhase::RAIN_SNOW,
        WorldRenderingPhase::WORLD_BORDER
    };

    for (auto phase : translucentPhases)
    {
        // TODO (Milestone 3.2): 同上
        // - TERRAIN_TRANSLUCENT: TerrainRenderer (gbuffers_water.vsh/fsh)
        // - PARTICLES: ParticleRenderer (gbuffers_textured.vsh/fsh)

        if (m_renderCommandQueue && m_renderCommandQueue->GetCommandCount(phase) > 0)
        {
            m_currentPipeline->SetPhase(phase);
            // TODO: m_currentPipeline->ExecutePhaseRenderer(phase);
            LogDebug(GetStaticSubsystemName(), "EndFrame - Phase {} executed ({} commands)",
                     static_cast<uint32_t>(phase), m_renderCommandQueue->GetCommandCount(phase));
        }
    }

    // 手部渲染阶段
    const WorldRenderingPhase handPhases[] = {
        WorldRenderingPhase::HAND_SOLID,
        WorldRenderingPhase::HAND_TRANSLUCENT
    };

    for (auto phase : handPhases)
    {
        // TODO (Milestone 3.2): 同上
        // - HAND_SOLID/TRANSLUCENT: HandRenderer (gbuffers_hand.vsh/fsh, gbuffers_hand_water.vsh/fsh)

        if (m_renderCommandQueue && m_renderCommandQueue->GetCommandCount(phase) > 0)
        {
            m_currentPipeline->SetPhase(phase);
            // TODO: m_currentPipeline->ExecutePhaseRenderer(phase);
            LogDebug(GetStaticSubsystemName(), "EndFrame - Phase {} executed ({} commands)",
                     static_cast<uint32_t>(phase), m_renderCommandQueue->GetCommandCount(phase));
        }
    }

    // OUTLINE 阶段 (outline.fsh)
    if (m_renderCommandQueue && m_renderCommandQueue->GetCommandCount(WorldRenderingPhase::OUTLINE) > 0)
    {
        m_currentPipeline->SetPhase(WorldRenderingPhase::OUTLINE);
        // TODO (Milestone 3.2): m_currentPipeline->ExecuteOutlineStage();
        LogDebug(GetStaticSubsystemName(), "EndFrame - OUTLINE phase executed ({} commands)",
                 m_renderCommandQueue->GetCommandCount(WorldRenderingPhase::OUTLINE));
    }

    // DEBUG 阶段 - 显式调用 DebugRenderer (debug.vsh/debug.fsh)
    if (m_renderCommandQueue && m_renderCommandQueue->GetCommandCount(WorldRenderingPhase::DEBUG) > 0)
    {
        m_currentPipeline->SetPhase(WorldRenderingPhase::DEBUG);
        // 教学要点: ExecuteDebugStage() 内部调用 DebugRenderer->RenderAll()
        // - RenderAll() 遍历所有 debug Pass (debug1.fsh, debug2.fsh, ...)
        // - 每个 Pass 执行完整的渲染流程
        m_currentPipeline->ExecuteDebugStage();
        LogDebug(GetStaticSubsystemName(), "EndFrame - DEBUG phase executed ({} commands)",
                 m_renderCommandQueue->GetCommandCount(WorldRenderingPhase::DEBUG));
    }

    // ==================== 阶段 28: EndLevelRendering (finalize.fsh) ====================
    // 教学要点: 对应 Iris 的 finalizeLevelRendering() 调用
    // - 执行最终的后处理 Pass (final.vsh/fsh)
    // - 合成所有渲染目标到最终输出
    m_currentPipeline->EndLevelRendering();
    LogDebug(GetStaticSubsystemName(), "EndFrame - EndLevelRendering completed (final.fsh)");

    // ==================== GPU 命令提交和 Present ====================
    // 1. 回收已完成的命令列表
    auto* commandListManager = D3D12RenderSystem::GetCommandListManager();
    if (commandListManager)
    {
        commandListManager->UpdateCompletedCommandLists();
    }

    // 2. 呈现到屏幕 (Present)
    D3D12RenderSystem::Present(true); // 启用垂直同步

    LogInfo(GetStaticSubsystemName(), "EndFrame - Frame {} completed and presented",
            m_renderStatistics.frameIndex);
}
