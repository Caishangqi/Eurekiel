#include "DebugDrawTextCommand.hpp"
#include <algorithm>

#include "Engine/Math/Vec2.hpp"

#undef min
#undef max
using namespace enigma::core;

namespace enigma::graphic
{
    DebugDrawTextCommand::DebugDrawTextCommand(const std::string& text, Vec2 position, Rgba8 color, const std::string& debugTag) : m_text(text), m_position(position), m_color(color),
        m_debugTag(debugTag)
    {
    }

    RenderCommandType DebugDrawTextCommand::GetType() const
    {
        return RenderCommandType::DRAW_INSTANCED;
    }

    void DebugDrawTextCommand::Execute(std::shared_ptr<CommandListManager> commandManager)
    {
        // 详细的日志输出，用于验证调用链
        LogInfo("DebugDrawTextCommand", "=== Executing Debug Text Command ===");
        LogInfo("DebugDrawTextCommand", "Text: '{}'", m_text);
        LogInfo("DebugDrawTextCommand", "Position: ({:.2f}, {:.2f})", m_position.x, m_position.y);
        LogInfo("DebugDrawTextCommand", "Color: 0x{:08X}", m_color);

        if (!m_debugTag.empty())
        {
            LogInfo("DebugDrawTextCommand", "Debug Tag: '{}'", m_debugTag);
        }

        // 验证参数有效性
        if (commandManager)
        {
            LogInfo("DebugDrawTextCommand", "CommandList: Valid (ID3D12GraphicsCommandList available)");
        }
        else
        {
            LogWarn("DebugDrawTextCommand", "CommandList: Invalid (nullptr)");
        }

        if (commandManager)
        {
            LogInfo("DebugDrawTextCommand", "CommandManager: Valid (CommandListManager available)");
        }
        else
        {
            LogWarn("DebugDrawTextCommand", "CommandManager: Invalid (nullptr)");
        }

        // 模拟文本渲染过程（仅日志输出）
        LogInfo("DebugDrawTextCommand", "Step 1: [Simulated] Generate text geometry for '{}'", m_text);
        LogInfo("DebugDrawTextCommand", "Step 2: [Simulated] Set viewport and scissor rect");
        LogInfo("DebugDrawTextCommand", "Step 3: [Simulated] Bind font texture and text shader");
        LogInfo("DebugDrawTextCommand", "Step 4: [Simulated] Draw text vertices");

        LogInfo("DebugDrawTextCommand", "=== Debug Text Command Completed Successfully ===");
    }

    std::string DebugDrawTextCommand::GetName() const
    {
        if (!m_debugTag.empty())
        {
            return "DebugDrawTextCommand[" + m_debugTag + "]: " + m_text;
        }
        return "DebugDrawTextCommand: " + m_text;
    }

    bool DebugDrawTextCommand::IsValid() const
    {
        // 基本验证：文本不为空
        return !m_text.empty();
    }

    void DebugDrawTextCommand::SetText(const std::string& newText)
    {
        m_text = newText.empty() ? "[Empty Text]" : newText;
    }

    void DebugDrawTextCommand::SetPosition(float x, float y)
    {
        m_position.x = std::max(0.0f, std::min(1.0f, x));
        m_position.y = std::max(0.0f, std::min(1.0f, y));
    }
}
