#include "EnigmaRenderingPipeline.hpp"
#include "DebugRenderer.hpp"
#include "CompositeRenderer.hpp"
#include "../../Shadow/ShadowRenderer.hpp"
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
        LogInfo(RendererSubsystem::GetStaticSubsystemName(), "EnigmaRenderingPipeline::EnigmaRenderingPipeline: Construction completed");
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
     * @brief 设置当前渲染阶段 - 简单的标志位设置（严格遵循Iris）
     *
     * Iris架构要点：
     * - SetPhase()只是简单的标志位设置，零开销
     * - 不执行任何渲染逻辑，不分发任何任务
     * - 渲染逻辑由各个Renderer的RenderAll()方法处理
     * - 这是Iris的核心设计：Phase只是状态标记，不是执行触发器
     */
    void EnigmaRenderingPipeline::SetPhase(WorldRenderingPhase phase)
    {
        LogDebug(RendererSubsystem::GetStaticSubsystemName(),
                 "EnigmaRenderingPipeline: SetPhase: {} -> {}",
                 static_cast<uint32_t>(m_currentPhase),
                 static_cast<uint32_t>(phase));

        m_currentPhase = phase;
        m_stats.phaseSwitches++;
    }

    /**
     * @brief 执行Debug阶段 - DebugRenderer的核心调用
     *
     * 这是Enigma扩展的调试渲染阶段，对应Iris中没有的功能。
     * 专门用于验证渲染管线的核心功能。
     */
    void EnigmaRenderingPipeline::ExecuteDebugStage()
    {
    }

    // ===========================================
    // IWorldRenderingPipeline接口的简化实现
    // ===========================================

    /**
     * @brief BeginWorldRendering - Iris接口兼容方法
     *
     * 教学要点：
     * - IWorldRenderingPipeline接口要求实现此方法
     * - 在我们的架构中，实际工作由BeginLevelRendering()完成
     * - 这个方法保持空实现，确保接口兼容性
     */
    void EnigmaRenderingPipeline::BeginWorldRendering()
    {
        // 空实现 - BeginLevelRendering()承担实际职责
    }

    /**
     * @brief EndWorldRendering - Iris接口兼容方法
     *
     * 教学要点：
     * - IWorldRenderingPipeline接口要求实现此方法
     * - 在我们的架构中，实际工作由EndLevelRendering()完成
     * - 这个方法保持空实现，确保接口兼容性
     */
    void EnigmaRenderingPipeline::EndWorldRendering()
    {
        // 空实现 - EndLevelRendering()承担实际职责
    }

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
        // ========================================================================
        // Pipeline 生命周期重构 - BeginLevelRendering (Milestone 3.1)
        // ========================================================================
        // 对应Iris的beginWorldRender()调用
        // 职责: 初始化渲染管线状态 + 准备RenderTargets + 设置全局Uniform
        // ========================================================================

        LogDebug(RendererSubsystem::GetStaticSubsystemName(),
                 "EnigmaRenderingPipeline::BeginLevelRendering - Start level rendering");

        // 1. 标记管线激活
        m_isActive = true;
        m_stats.framesRendered++;

        // 2. 设置初始Phase为NONE
        // 教学要点: Iris在beginWorldRender后会将Phase设置为NONE，
        // 然后由外部调用SetPhase切换到具体阶段
        m_currentPhase = WorldRenderingPhase::NONE;

        // 3. TODO (Milestone 3.2): 准备RenderTargets
        // - 清空或重置所有colortex0-15
        // - 准备depth纹理 (depthtex0/1/2)
        // - 设置RT格式和多重采样

        // 4. TODO (Milestone 3.2): 更新全局Uniform
        // - frameCounter (帧计数器)
        // - frameTime (当前帧时间)
        // - sunAngle, moonAngle (天空光照)
        // - worldTime (世界时间)

        // 5. 通知所有帧更新监听器
        NotifyFrameUpdateListeners();

        LogDebug(RendererSubsystem::GetStaticSubsystemName(),
                 "EnigmaRenderingPipeline::BeginLevelRendering - Level rendering initialized (frame: {})",
                 m_stats.framesRendered);
    }

    void EnigmaRenderingPipeline::RenderShadows()
    {
        // ========================================================================
        // Pipeline 生命周期重构 - RenderShadows (Milestone 3.1)
        // ========================================================================
        // 对应Iris的renderShadows()调用
        // 职责: 执行Shadow阶段的所有渲染指令 + 生成Shadow Map
        // ========================================================================

        LogDebug(RendererSubsystem::GetStaticSubsystemName(),
                 "EnigmaRenderingPipeline::RenderShadows - Rendering shadow pass");

        // 1. 设置Phase为SHADOW (已在RendererSubsystem::EndFrame中调用)
        // m_currentPhase应该已经是WorldRenderingPhase::SHADOW

        // 2. TODO (Milestone 3.2): 准备Shadow Map RenderTarget
        // - 切换到shadowtex0/shadowtex1渲染目标
        // - 清空深度缓冲
        // - 设置Shadow Map视锥体矩阵 (shadowProjection, shadowModelView)

        // 3. TODO (Milestone 3.2): 执行Shadow阶段指令
        // - 从RenderCommandQueue提取SHADOW阶段的所有指令
        // - 使用gbuffers_shadow着色器程序
        // - 只渲染产生阴影的几何体 (Terrain, Entities等)

        // 4. TODO (Milestone 3.2): Shadow Map后处理
        // - 执行shadowcomp0-99 composite pass
        // - 可选：软阴影、阴影过滤

        // 暂时记录日志，等待完整实现
        LogDebug(RendererSubsystem::GetStaticSubsystemName(),
                 "EnigmaRenderingPipeline::RenderShadows - Shadow pass completed (placeholder)");
    }

    void EnigmaRenderingPipeline::EndLevelRendering()
    {
        // ========================================================================
        // Pipeline 生命周期重构 - EndLevelRendering (Milestone 3.1)
        // ========================================================================
        // 对应Iris的finalizeLevelRendering()调用
        // 职责: 执行Composite和Final Pass + 清理状态 + 标记管线结束
        // ========================================================================

        LogDebug(RendererSubsystem::GetStaticSubsystemName(),
                 "EnigmaRenderingPipeline::EndLevelRendering - Finalizing level rendering");

        // 教学要点: Iris架构中finalizeLevelRendering()的真实实现
        // Java源码 (IrisRenderingPipeline.java:1523-1527):
        //   isRenderingWorld = false;
        //   removePhaseIfNeeded();
        //   compositeRenderer.renderAll();      // 执行所有composite0-99.fsh
        //   finalPassRenderer.renderFinalPass(); // 执行final.fsh
        //
        // 关键理解: WorldRenderingPhase枚举中没有FINAL值！
        // Composite和Final Pass是独立的渲染器，不属于Phase系统

        // 1. 重置Phase标志为NONE
        // 教学要点: 对应removePhaseIfNeeded()
        SetPhase(WorldRenderingPhase::NONE);

        // 2. 标记管线非激活状态
        // 教学要点: 对应isRenderingWorld = false
        m_isActive = false;

        // 3. TODO (Milestone 3.2): 执行Composite渲染器
        // compositeRenderer->RenderAll();
        // - 执行composite0.fsh到composite99.fsh
        // - 每个composite pass都是全屏四边形
        // - 用于后处理效果 (Bloom, DOF, Motion Blur等)

        // 4. TODO (Milestone 3.2): 执行Final Pass渲染器
        // finalPassRenderer->RenderFinalPass();
        // - 执行final.fsh着色器
        // - 将最终结果Blit到BackBuffer
        // - 可能包含ColorSpace转换 (sRGB校正)

        // 5. 更新性能统计
        UpdatePerformanceStats(16.67f); // 假设60FPS，后续从实际帧时间获取

        LogDebug(RendererSubsystem::GetStaticSubsystemName(),
                 "EnigmaRenderingPipeline::EndLevelRendering - Level rendering finalized (frame: {})",
                 m_stats.framesRendered);
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

        // 清理帧更新监听器
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
