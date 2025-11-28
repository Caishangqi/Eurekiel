#include "DebugRenderer.hpp"

#include <Engine/Graphic/Shader/ShaderPack/Properties/PackDirectives.hpp>

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Logger/Logger.hpp"

using namespace enigma::core;

namespace enigma::graphic
{
    /**
     * @brief 构造函数 - RAII模式，构造完成即可用
     *
     * 对应Iris CompositeRenderer构造模式的简化版本。
     * DebugRenderer专注于MVP验证，移除了不必要的参数。
     */
    DebugRenderer::DebugRenderer(
        std::shared_ptr<IWorldRenderingPipeline>    pipeline,
        PackDirectives                              packDirectives,
        std::vector<std::shared_ptr<ShaderSource>>& shaderSources,
        std::shared_ptr<RenderTargets>              renderTargets)
        : m_pipeline(pipeline)
          , m_packDirectives(packDirectives)
          , m_renderTargets(renderTargets)
    {

        UNUSED(shaderSources)
        
        LogInfo("DebugRenderer", "DebugRenderer constructed (RAII mode)");

        // TODO (Milestone 3.2): 从pipeline获取必要的管理器
        // m_commandManager  = pipeline->GetCommandListManager();
        // m_shaderManager   = pipeline->GetShaderPackManager();
        // m_uniformManager  = pipeline->GetUniformManager();
        // m_commandQueue    = pipeline->GetRenderCommandQueue();

        // TODO (Milestone 3.2): 加载DEBUG着色器程序
        // 遍历shaderSources，查找debug.vsh/debug.fsh并编译
    }

    /**
     * @brief 析构函数 - RAII自动清理
     *
     * 智能指针确保所有资源安全释放，无需手动管理。
     */
    DebugRenderer::~DebugRenderer()
    {
        LogInfo("DebugRenderer", "DebugRenderer destroyed");
        Destroy();
    }

    /**
     * @brief 执行所有调试渲染
     *
     * 这是DebugRenderer的主要方法，在WorldRenderingPhase::DEBUG阶段调用。
     */
    void DebugRenderer::RenderAll()
    {
        LogDebug("DebugRenderer", "RenderAll - Start DEBUG rendering");

        // TODO (Milestone 3.2): 完整实现
        // 1. 准备渲染目标 (colortex0)
        // PrepareRenderTargets();

        // 2. 绑定DEBUG着色器程序
        // if (!BindDebugShaderProgram()) return;

        // 3. 更新Debug Uniform变量
        // UpdateDebugUniforms();

        // 4. 渲染测试几何体
        if (m_enableGeometryTest)
        {
            // RenderTestGeometry();
        }

        // 5. 执行Bindless纹理测试
        if (m_enableTextureTest)
        {
            // RenderBindlessTextureTest();
        }

        // 6. 执行Immediate模式指令
        if (m_enableImmediateTest)
        {
            // ExecuteImmediateCommands();
        }

        // 7. 渲染性能统计信息
        if (m_enablePerformanceStats)
        {
            // RenderPerformanceStats();
        }

        LogDebug("DebugRenderer", "RenderAll - DEBUG rendering completed");
    }

    void DebugRenderer::RenderTestGeometry()
    {
        // TODO (Milestone 3.2): 实现测试几何体渲染
        LogDebug("DebugRenderer", "RenderTestGeometry - TODO");
    }

    void DebugRenderer::RenderBindlessTextureTest()
    {
        // TODO (Milestone 3.2): 实现Bindless纹理测试
        LogDebug("DebugRenderer", "RenderBindlessTextureTest - TODO");
    }

    void DebugRenderer::ExecuteImmediateCommands()
    {
        // TODO (Milestone 3.2): 实现Immediate指令执行
        LogDebug("DebugRenderer", "ExecuteImmediateCommands - TODO");
    }

    void DebugRenderer::RenderPerformanceStats()
    {
        // TODO (Milestone 3.2): 实现性能统计渲染
        LogDebug("DebugRenderer", "RenderPerformanceStats - TODO");
    }

    std::string DebugRenderer::GetRenderingStats() const
    {
        return "DrawCalls: " + std::to_string(m_renderStats.geometryDrawCalls) +
            ", Textures: " + std::to_string(m_renderStats.texturesUsed) +
            ", Commands: " + std::to_string(m_renderStats.commandsExecuted) +
            ", Time: " + std::to_string(m_renderStats.totalRenderTime) + "ms";
    }

    void DebugRenderer::ResetStats()
    {
        m_renderStats.geometryDrawCalls = 0;
        m_renderStats.texturesUsed      = 0;
        m_renderStats.commandsExecuted  = 0;
        m_renderStats.totalRenderTime   = 0.0f;
    }

    void DebugRenderer::Destroy()
    {
        LogInfo("DebugRenderer", "Destroy - Cleaning up resources");

        // 智能指针自动清理，无需手动释放
        m_pipeline.reset();
        m_renderTargets.reset();
        m_commandManager.reset();
        m_shaderManager.reset();
        m_uniformManager.reset();
        m_commandQueue.reset();

        ResetStats();
    }

    void DebugRenderer::PrepareRenderTargets()
    {
        // TODO (Milestone 3.2): 准备colortex0渲染目标
        LogDebug("DebugRenderer", "PrepareRenderTargets - TODO");
    }

    bool DebugRenderer::BindDebugShaderProgram()
    {
        // TODO (Milestone 3.2): 绑定debug.vsh/debug.fsh程序
        LogDebug("DebugRenderer", "BindDebugShaderProgram - TODO");
        return true;
    }

    void DebugRenderer::UpdateDebugUniforms()
    {
        // TODO (Milestone 3.2): 更新frameCounter, frameTime等Uniform
        LogDebug("DebugRenderer", "UpdateDebugUniforms - TODO");
    }

    void DebugRenderer::LogDebugInfo(const std::string& message) const
    {
        if (m_debugMode)
        {
            LogDebug("DebugRenderer", "%s", message.c_str());
        }
    }
} // namespace enigma::graphic

