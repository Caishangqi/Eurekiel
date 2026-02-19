#include "Engine/Core/Console/Imgui/ImguiConsoleRenderer.hpp"
#include "Engine/Core/Console/Imgui/ImguiConsole.hpp"
#include "Engine/Core/Console/Imgui/ImguiConsoleInput.hpp"
#include "Engine/Core/Console/ImguiConsoleConfig.hpp"
#include "Engine/Core/Console/ConsoleMessage.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Window/Window.hpp"
#include "ThirdParty/imgui/imgui.h"

namespace enigma::core
{
    //=========================================================================
    // Bottom position calculation for Terminal mode
    //=========================================================================
    ImVec2 ImguiConsoleRenderer::CalcBottomPosition()
    {
        float clientW = 1280.0f;
        float clientH = 720.0f;
        if (g_theWindow)
        {
            clientW = g_theWindow->GetClientWidth();
            clientH = g_theWindow->GetClientHeight();
        }
        float consoleH = CONSOLE_INPUT_HEIGHT + ImGui::GetStyle().WindowPadding.y * 2.0f;
        float x = 0.0f;
        float y = clientH - consoleH;
        return ImVec2(x, y);
    }

    ImVec2 ImguiConsoleRenderer::CalcBottomSize()
    {
        float clientW = 1280.0f;
        if (g_theWindow)
        {
            clientW = g_theWindow->GetClientWidth();
        }
        float width  = clientW;
        float height = CONSOLE_INPUT_HEIGHT + ImGui::GetStyle().WindowPadding.y * 2.0f;
        return ImVec2(width, height);
    }

    //=========================================================================
    // Terminal Mode: ">" prompt + input field (screen bottom, no messages)
    //=========================================================================
    void ImguiConsoleRenderer::RenderTerminalMode(ImguiConsole& console)
    {
        ImGui::TextUnformatted(">");
        ImGui::SameLine();

        ImGui::PushItemWidth(-1);
        std::string& inputBuf = console.GetInputBuffer();
        char buf[256] = {};
        size_t copyLen = (inputBuf.size() < sizeof(buf) - 1) ? inputBuf.size() : sizeof(buf) - 1;
        memcpy(buf, inputBuf.c_str(), copyLen);

        ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue
                                       | ImGuiInputTextFlags_CallbackHistory
                                       | ImGuiInputTextFlags_CallbackCompletion
                                       | ImGuiInputTextFlags_CallbackEdit
                                       | ImGuiInputTextFlags_CallbackCharFilter;
        if (ImGui::InputText("##ConsoleInput", buf, sizeof(buf), inputFlags,
                             ImguiConsoleInput::InputTextCallback, &console))
        {
            inputBuf = buf;
            ImguiConsoleInput::SubmitCommand(console);
            ImGui::SetKeyboardFocusHere(-1);
        }
        else
        {
            inputBuf = buf;
        }
        ImGui::PopItemWidth();

        // Auto-focus on first appearance
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere(-1);
        }
    }

    //=========================================================================
    // Docked Mode: "Cmd" label + input field (docked below MessageLogUI)
    //=========================================================================
    void ImguiConsoleRenderer::RenderDockedMode(ImguiConsole& console)
    {
        ImGui::TextUnformatted(DOCKED_INPUT_LABEL);
        ImGui::SameLine();

        ImGui::PushItemWidth(-1);
        std::string& inputBuf = console.GetInputBuffer();
        char buf[256] = {};
        size_t copyLen = (inputBuf.size() < sizeof(buf) - 1) ? inputBuf.size() : sizeof(buf) - 1;
        memcpy(buf, inputBuf.c_str(), copyLen);

        ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue
                                       | ImGuiInputTextFlags_CallbackHistory
                                       | ImGuiInputTextFlags_CallbackCompletion
                                       | ImGuiInputTextFlags_CallbackEdit
                                       | ImGuiInputTextFlags_CallbackCharFilter;
        if (ImGui::InputText("##DockedInput", buf, sizeof(buf), inputFlags,
                             ImguiConsoleInput::InputTextCallback, &console))
        {
            inputBuf = buf;
            ImguiConsoleInput::SubmitCommand(console);
            ImGui::SetKeyboardFocusHere(-1);
        }
        else
        {
            inputBuf = buf;
        }
        ImGui::PopItemWidth();
    }

    //=========================================================================
    // Shared rendering utilities
    //=========================================================================
    ImVec4 ImguiConsoleRenderer::GetMessageColor(const ConsoleMessage& msg)
    {
        switch (msg.level)
        {
        case LogLevel::WARNING:
            return CONSOLE_COLOR_WARNING;
        case LogLevel::ERROR_:
            return CONSOLE_COLOR_ERROR;
        default:
            return CONSOLE_COLOR_LOG;
        }
    }

    void ImguiConsoleRenderer::RenderColoredText(const ConsoleMessage& msg)
    {
        ImVec4 color = GetMessageColor(msg);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(msg.text.c_str());
        ImGui::PopStyleColor();
    }
} // namespace enigma::core
