#include "DrawCommands.hpp"

#include "Engine/Core/EngineCommon.hpp"

namespace enigma::graphic
{
    DrawIndexedCommand::DrawIndexedCommand(uint32_t indexCount, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation)
        : m_indexCount(indexCount),
          m_instanceCount(instanceCount),
          m_startIndexLocation(startIndexLocation),
          m_baseVertexLocation(baseVertexLocation),
          m_startInstanceLocation(startInstanceLocation)
    {
    }

    RenderCommandType DrawIndexedCommand::GetType() const
    {
        return m_instanceCount > 1 ? RenderCommandType::DRAW_INDEXED_INSTANCED : RenderCommandType::DRAW_INDEXED;
    }

    void DrawIndexedCommand::Execute(std::shared_ptr<CommandListManager> commandManager)
    {
        if (!commandManager)
        {
            return;
        }

        // 执行DirectX 12索引化绘制
        // TODO: 这里调用 D3D12RenderSystem
    }

    std::string DrawIndexedCommand::GetName() const
    {
        return "DrawIndexed(indices=" + std::to_string(m_indexCount) +
            ", instances=" + std::to_string(m_instanceCount) + ")";
    }

    bool DrawIndexedCommand::IsValid() const
    {
        return m_indexCount > 0 && m_instanceCount > 0;
    }

    DrawInstancedCommand::DrawInstancedCommand(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation,
                                               uint32_t startInstanceLocation) : m_vertexCountPerInstance(vertexCountPerInstance), m_instanceCount(instanceCount)
    {
        UNUSED(startVertexLocation)
        UNUSED(startInstanceLocation)
    }

    RenderCommandType DrawInstancedCommand::GetType() const
    {
        return RenderCommandType::DRAW_INSTANCED;
    }

    void DrawInstancedCommand::Execute(std::shared_ptr<CommandListManager> commandManager)
    {
        if (!commandManager)
        {
            return;
        }

        // 执行DirectX 12索引化绘制
        // TODO: 这里调用 D3D12RenderSystem
    }

    std::string DrawInstancedCommand::GetName() const
    {
        return "DrawInstanced(vertices=" + std::to_string(m_vertexCountPerInstance) + ", instances=" + std::to_string(m_instanceCount) + ")";
    }

    bool DrawInstancedCommand::IsValid() const
    {
        return m_vertexCountPerInstance > 0 && m_instanceCount > 0;
    }
}
