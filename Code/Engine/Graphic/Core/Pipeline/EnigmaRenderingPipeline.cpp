#include "EnigmaRenderingPipeline.hpp"
#include "DebugRenderer.hpp"
#include "CompositeRenderer.hpp"
#include "ShadowRenderer.hpp"
#include "../DX12/D3D12RenderSystem.hpp"
#include "Engine/Core/Logger/Logger.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"

namespace enigma::graphic
{
    /**
     * @brief 构造函数 - 创建所有子渲染器
     *
     * 严格按照Iris的构造模式，创建所有必要的渲染器：
     * - beginRenderer, prepareRenderer, deferredRenderer, compositeRenderer (对应Iris)
     * - shadowRenderer (对应Iris)
     * - debugRenderer (Enigma扩展，用于开发验证)
     */
    EnigmaRenderingPipeline::EnigmaRenderingPipeline(
        CommandListManager* commandManager)
        : m_commandManager(commandManager)
          , m_currentPhase(WorldRenderingPhase::NONE)
          , m_isActive(false)
          , m_isInitialized(false)
          , m_debugMode(false)
          , m_shaderPackEnabled(false)
          , m_shaderRenderDistance(-1.0f)
          , m_disableVanillaFog(false)
          , m_disableDirectionalShading(false)
          , m_nextCallbackId(1)
    {
        LogInfo(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline::EnigmaRenderingPipeline: Start");

        // 暂时创建空的渲染器实例，等待完整实现
        InitializeSubRenderers();
        InitializePhaseDispatchMap();

        LogInfo(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline::EnigmaRenderingPipeline: Construction");
    }

    /**
     * @brief 析构函数 - 清理所有资源
     */
    EnigmaRenderingPipeline::~EnigmaRenderingPipeline()
    {
        LogInfo(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline::~EnigmaRenderingPipeline Start");
        Destroy();
        LogInfo(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline::~EnigmaRenderingPipeline Complete");
    }

    /**
     * @brief 初始化所有子渲染器
     *
     * 按照Iris的5参数模式创建渲染器：
     * new CompositeRenderer(this, pass, programSet, renderTargets, textureStage)
     */
    bool EnigmaRenderingPipeline::InitializeSubRenderers()
    {
        LogInfo(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline::InitializeSubRenderers");

        try
        {
            // 为了简化，暂时不创建其他渲染器，专注于DebugRenderer
            // 在后续Milestone中会逐步添加完整的渲染器

            // 创建DebugRenderer - 严格按照Iris的5参数模式
            // 对应Iris调用：new CompositeRenderer(this, CompositePass.DEBUG, programSet, renderTargets, TextureStage.DEBUG)
            m_debugRenderer = std::make_unique<DebugRenderer>(
                this, // 父渲染管线 (对应Iris的this)
                CompositePass::DEBUG, // 合成Pass类型
                nullptr, // 程序集 (暂时nullptr，等待着色器系统实现)
                m_renderTargets, // 渲染目标管理器
                TextureStage::DEBUG // 纹理阶段
            );

            LogInfo(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: DebugRenderer Create Successful");

            // Initialize DebugRenderer
            if (m_debugRenderer && !m_debugRenderer->Initialize())
            {
                LogError(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: DebugRenderer Initialized Failed");
                return false;
            }

            LogInfo(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: Sub Renderers initialized");
            return true;
        }
        catch (const std::exception& e)
        {
            LogError(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: Sub Renderers initialized exception: {}", e.what());
            return false;
        }
    }

    /**
     * @brief 初始化阶段分发映射表
     *
     * 建立从WorldRenderingPhase到具体执行方法的映射关系。
     * 这是EnigmaRenderingPipeline的核心分发机制。
     */
    void EnigmaRenderingPipeline::InitializePhaseDispatchMap()
    {
        LogInfo(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: Initialization phase distribution map table");

        // 清空现有映射
        m_phaseDispatchMap.clear();

        // 只添加我们当前支持的阶段
        // 其他阶段在后续Milestone中逐步添加

        // DEBUG阶段 - 使用DebugRenderer
        m_phaseDispatchMap[WorldRenderingPhase::DEBUG] = [this]()
        {
            ExecuteDebugStage();
        };

        LogInfo(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: The initialization of the phase distribution map table is completed (supports {} stages)",
                m_phaseDispatchMap.size());
    }

    /**
     * @brief 开始世界渲染
     *
     * 对应Iris的beginLevelRendering()方法。
     */
    void EnigmaRenderingPipeline::BeginWorldRendering()
    {
        LogDebug(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: Start World Rendering");

        m_isActive = true;
        m_stats.framesRendered++;

        // 通知所有帧更新监听器
        NotifyFrameUpdateListeners();
    }

    /**
     * @brief 结束世界渲染
     *
     * 对应Iris的finalizeLevelRendering()方法。
     */
    void EnigmaRenderingPipeline::EndWorldRendering()
    {
        LogDebug(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: End World Rendering");

        // 更新性能统计
        UpdatePerformanceStats(16.67f); // 假设60FPS

        m_isActive = false;
    }

    /**
     * @brief 设置当前渲染阶段 - 核心分发方法
     *
     * 这是EnigmaRenderingPipeline的核心方法，负责将不同的渲染阶段
     * 分发给对应的子渲染器处理。严格基于Iris的分发逻辑。
     */
    void EnigmaRenderingPipeline::SetPhase(WorldRenderingPhase phase)
    {
        LogDebug(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: Set the rendering stage: {} -> {}",
                 static_cast<uint32_t>(m_currentPhase), static_cast<uint32_t>(phase));

        m_currentPhase = phase;
        m_stats.phaseSwitches++;

        // 查找并执行对应的阶段处理方法
        auto it = m_phaseDispatchMap.find(phase);
        if (it != m_phaseDispatchMap.end())
        {
            LogDebug(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: Execution phase %d", static_cast<uint32_t>(phase));
            it->second(); // 调用对应的执行方法
        }
        else
        {
            LogWarn(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: Unsupported rendering phase: %d", static_cast<uint32_t>(phase));
        }
    }

    /**
     * @brief 执行Debug阶段 - DebugRenderer的核心调用
     *
     * 这是Enigma扩展的调试渲染阶段，对应Iris中没有的功能。
     * 专门用于验证渲染管线的核心功能。
     */
    void EnigmaRenderingPipeline::ExecuteDebugStage()
    {
        LogDebug(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: Execute the Debug phase");

        if (!m_debugRenderer)
        {
            LogError(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: DebugRenderer Not initialized");
            return;
        }

        if (!m_debugRenderer->IsInitialized())
        {
            LogError(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: DebugRenderer Initialization Not Completed");
            return;
        }

        try
        {
            // 开始调试渲染帧
            m_debugRenderer->BeginFrame(m_stats.framesRendered);

            // 收集DEBUG阶段的immediate模式指令到DebugRenderer (Milestone 2.6新增)
            // 教学要点：在RenderAll之前收集指令，DebugRenderer在RenderAll中按顺序执行
            // 这遵循了Immediate模式的核心原则：收集-排序-执行
            LogDebug(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: Collecting immediate commands for DEBUG phase");

            try
            {
                // 检查是否有待执行的immediate命令
                if (D3D12RenderSystem::HasImmediateCommands(WorldRenderingPhase::DEBUG))
                {
                    size_t commandCount = D3D12RenderSystem::GetImmediateCommandCount(WorldRenderingPhase::DEBUG);
                    LogDebug(RendererSubsystem::GetStaticSubsystemName(),
                             "EnigmaRenderingPipeline: Found {} immediate commands in DEBUG phase, preparing for execution in RenderAll()", commandCount);

                    // 注意：实际的指令执行会在m_debugRenderer->RenderAll()内部处理
                    // 这里只是记录和准备，真正的执行在DebugRenderer中进行
                }
            }
            catch (const std::exception& e)
            {
                LogError(RendererSubsystem::GetStaticSubsystemName(),
                         "EnigmaRenderingPipeline: Failed to collect immediate commands in DEBUG phase: {}", e.what());
            }

            // 执行所有调试渲染 - 对应Iris CompositeRenderer.renderAll()
            // 教学要点：RenderAll()内部会执行收集到的immediate指令
            m_debugRenderer->RenderAll();

            // 结束调试渲染帧
            m_debugRenderer->EndFrame();

            LogDebug(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: Debug phase execution is completed");
        }
        catch (const std::exception& e)
        {
            LogError(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: Debug phase execution exception: {}", e.what());
        }
    }

    // ===========================================
    // IWorldRenderingPipeline接口的简化实现
    // ===========================================

    void EnigmaRenderingPipeline::BeginPass(uint32_t passIndex)
    {
        // 简化实现，后续扩展
        LogDebug(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: BeginPass({})", passIndex);
    }

    void EnigmaRenderingPipeline::EndPass()
    {
        // 简化实现，后续扩展
        LogDebug(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: EndPass()");
    }

    void EnigmaRenderingPipeline::BeginLevelRendering()
    {
        BeginWorldRendering();
    }

    void EnigmaRenderingPipeline::RenderShadows()
    {
        // 暂时空实现，等待ShadowRenderer完整实现
        LogDebug(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: RenderShadows() - 暂时跳过");
    }

    void EnigmaRenderingPipeline::EndLevelRendering()
    {
        EndWorldRendering();
    }

    bool EnigmaRenderingPipeline::ShouldDisableVanillaFog() const
    {
        return m_disableVanillaFog;
    }

    bool EnigmaRenderingPipeline::ShouldDisableDirectionalShading() const
    {
        return m_disableDirectionalShading;
    }

    float EnigmaRenderingPipeline::GetShaderRenderDistance() const
    {
        return m_shaderRenderDistance;
    }

    WorldRenderingPhase EnigmaRenderingPipeline::GetCurrentPhase() const
    {
        return m_currentPhase;
    }

    bool EnigmaRenderingPipeline::IsActive() const
    {
        return m_isActive;
    }

    void EnigmaRenderingPipeline::OnFrameUpdate()
    {
        NotifyFrameUpdateListeners();
    }

    void EnigmaRenderingPipeline::Reload()
    {
        LogInfo(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: Reload");
        // 简化实现，后续扩展
    }

    void EnigmaRenderingPipeline::Destroy()
    {
        LogInfo(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: Destroy resources");

        // 销毁DebugRenderer
        if (m_debugRenderer)
        {
            m_debugRenderer->Destroy();
            m_debugRenderer.reset();
        }

        // 清理其他资源
        m_phaseDispatchMap.clear();
        m_frameUpdateListeners.clear();

        m_isInitialized = false;
        m_isActive      = false;
    }

    // ===========================================
    // IShaderRenderingPipeline接口的简化实现
    // ===========================================

    std::shared_ptr<ShaderPackManager> EnigmaRenderingPipeline::GetShaderPackManager() const
    {
        return m_shaderPackManager;
    }

    std::shared_ptr<UniformManager> EnigmaRenderingPipeline::GetUniformManager() const
    {
        return m_uniformManager;
    }

    bool EnigmaRenderingPipeline::UseProgram(const std::string& programName)
    {
        // 简化实现，后续扩展
        LogDebug(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: UseProgram(%s)", programName.c_str());
        return true;
    }

    bool EnigmaRenderingPipeline::HasProgram(const std::string& programName) const
    {
        // 简化实现，后续扩展
        UNUSED(programName)
        return false;
    }

    bool EnigmaRenderingPipeline::ReloadShaders()
    {
        LogInfo(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: Reload the shader");
        // 简化实现，后续扩展
        return true;
    }

    void EnigmaRenderingPipeline::AddFrameUpdateListener(std::function<void()> callback)
    {
        size_t id = m_nextCallbackId++;
        m_frameUpdateListeners.emplace_back(id, std::move(callback));
        LogDebug(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: Add frame update listener ID: {}", id);
    }

    void EnigmaRenderingPipeline::RemoveFrameUpdateListener(size_t callbackId)
    {
        auto it = std::remove_if(m_frameUpdateListeners.begin(), m_frameUpdateListeners.end(),
                                 [callbackId](const auto& pair) { return pair.first == callbackId; });

        if (it != m_frameUpdateListeners.end())
        {
            m_frameUpdateListeners.erase(it, m_frameUpdateListeners.end());
            LogDebug(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: Remove frame update listener ID: {}", callbackId);
        }
    }

    void* EnigmaRenderingPipeline::GetColorTexture(uint32_t index) const
    {
        UNUSED(index)
        // 简化实现，后续扩展
        return nullptr;
    }

    void* EnigmaRenderingPipeline::GetDepthTexture() const
    {
        // 简化实现，后续扩展
        return nullptr;
    }

    void EnigmaRenderingPipeline::FlipBuffers()
    {
        m_stats.bufferFlips++;
        LogDebug(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: Buffer flip");
    }

    std::string EnigmaRenderingPipeline::GetCurrentShaderPackName() const
    {
        return m_currentShaderPackName;
    }

    bool EnigmaRenderingPipeline::IsShaderPackEnabled() const
    {
        return m_shaderPackEnabled;
    }

    std::string EnigmaRenderingPipeline::GetShaderPackVersion() const
    {
        return "1.0.0"; // 简化实现
    }

    bool EnigmaRenderingPipeline::SetShaderPackOption(const std::string& optionName, const std::string& value)
    {
        UNUSED(optionName)
        UNUSED(value)
        // 简化实现，后续扩展
        return true;
    }

    std::string EnigmaRenderingPipeline::GetShaderPackOption(const std::string& optionName) const
    {
        UNUSED(optionName)
        // 简化实现，后续扩展
        return "";
    }

    void EnigmaRenderingPipeline::SetDebugMode(bool enable)
    {
        m_debugMode = enable;
        LogInfo(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: Debug mode: {}", enable ? "Enable" : "Disable");
    }

    std::string EnigmaRenderingPipeline::GetRenderingStats() const
    {
        return "Frames: " + std::to_string(m_stats.framesRendered) +
            ", Phases: " + std::to_string(m_stats.phaseSwitches) +
            ", Flips: " + std::to_string(m_stats.bufferFlips);
    }

    // ===========================================
    // 内部辅助方法
    // ===========================================

    void EnigmaRenderingPipeline::NotifyFrameUpdateListeners()
    {
        for (const auto& [id, callback] : m_frameUpdateListeners)
        {
            try
            {
                callback();
            }
            catch (const std::exception& e)
            {
                LogError(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline: Frame update listener exception ID {}: {}", id, e.what());
            }
        }
    }

    void EnigmaRenderingPipeline::UpdatePerformanceStats(float frameTime)
    {
        m_stats.totalFrameTime += frameTime;
        m_stats.averageFrameTime = m_stats.totalFrameTime / m_stats.framesRendered;
    }
} // namespace enigma::graphic
