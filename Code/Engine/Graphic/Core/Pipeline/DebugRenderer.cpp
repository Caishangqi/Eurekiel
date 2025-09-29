#include "DebugRenderer.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "../DX12/D3D12RenderSystem.hpp"
#include "Engine/Core/Logger/Logger.hpp"

using namespace enigma::core;

namespace enigma::graphic
{
    DebugRenderer::DebugRenderer(
        IShaderRenderingPipeline*         parentPipeline,
        CompositePass                     compositePass,
        ProgramSet*                       programSet,
        std::shared_ptr<D12RenderTargets> renderTargets,
        TextureStage                      textureStage) :
        m_parentPipeline(parentPipeline), m_compositePass(compositePass), m_programSet(programSet), m_renderTargets(renderTargets), m_textureStage(textureStage)
    {
    }

    DebugRenderer::~DebugRenderer()
    {
    }

    bool DebugRenderer::Initialize()
    {
        m_isInitialized = true;
        return true;
    }

    void DebugRenderer::Destroy()
    {
    }

    void DebugRenderer::RenderAll()
    {
        // 执行DEBUG阶段的immediate模式指令 (Milestone 2.6新增)
        // 教学要点：这是Immediate模式的核心执行点
        // 在这里执行所有收集到的DEBUG阶段immediate指令
        LogDebug("DebugRenderer", "RenderAll: Executing immediate commands for DEBUG phase");

        try
        {
            // 通过D3D12RenderSystem执行DEBUG阶段的immediate命令
            // 遵循分层架构：DebugRenderer → D3D12RenderSystem → RenderCommandQueue
            size_t commandCount = D3D12RenderSystem::ExecuteImmediateCommands(WorldRenderingPhase::DEBUG);

            if (commandCount > 0)
            {
                LogDebug("DebugRenderer", "RenderAll: Successfully executed {} immediate commands", commandCount);
            }
            else
            {
                LogDebug("DebugRenderer", "RenderAll: No immediate commands to execute in DEBUG phase");
            }
        }
        catch (const std::exception& e)
        {
            LogError("DebugRenderer", "RenderAll: Failed to execute immediate commands: %s", e.what());
        }
    }

    void DebugRenderer::BeginFrame(uint64_t frameIndex)
    {
        UNUSED(frameIndex)
    }

    void DebugRenderer::EndFrame()
    {
    }

    void DebugRenderer::RenderDebugTriangle(uint32_t bindlessTextureIndex, const void* transform)
    {
        UNUSED(bindlessTextureIndex)
        UNUSED(transform)
        ERROR_AND_DIE("DebugRenderer::RenderDebugTriangle not implemented")
    }

    void DebugRenderer::RenderDebugQuad(uint32_t bindlessTextureIndex, const void* transform)
    {
        UNUSED(bindlessTextureIndex)
        UNUSED(transform)
        ERROR_AND_DIE("DebugRenderer::RenderDebugQuad not implemented")
    }

    void DebugRenderer::RenderCustomVertices(const std::vector<Vertex_PCU>& vertices, uint32_t bindlessTextureIndex)
    {
        UNUSED(vertices)
        UNUSED(bindlessTextureIndex)
        ERROR_AND_DIE("DebugRenderer::RenderCustomVertices not implemented")
    }

    void DebugRenderer::ProcessImmediateCommands(RenderCommandQueue* globalQueue)
    {
        UNUSED(globalQueue)
        ERROR_AND_DIE("DebugRenderer::ProcessImmediateCommands not implemented")
    }

    void DebugRenderer::SubmitDebugCommand(RenderCommandPtr command)
    {
        UNUSED(command)
        ERROR_AND_DIE("DebugRenderer::SubmitDebugCommand not implemented")
    }

    bool DebugRenderer::CreateDebugGeometry()
    {
        ERROR_AND_DIE("DebugRenderer::CreateDebugGeometry not implemented")
    }

    bool DebugRenderer::CreateD3D12Buffers()
    {
        ERROR_AND_DIE("DebugRenderer::CreateD3D12Buffers not implemented")
    }

    void DebugRenderer::ExecuteRenderCommand(const IRenderCommand* command)
    {
        UNUSED(command)
        ERROR_AND_DIE("DebugRenderer::ExecuteRenderCommand not implemented")
    }

    void DebugRenderer::UpdateStats()
    {
    }

    void DebugRenderer::LogDebugInfo(const std::string& message) const
    {
        UNUSED(message)
    }

    bool DebugRenderer::ValidateRenderState() const
    {
        ERROR_AND_DIE("DebugRenderer::ValidateRenderState not implemented")
    }
}
