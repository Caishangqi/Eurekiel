#include "Engine/Core/Console/Imgui/ImguiConsoleFullRenderer.hpp"
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
    // Helpers
    //=========================================================================
    static void GetClientSize(float& outW, float& outH)
    {
        outW = 1280.0f;
        outH = 720.0f;
        if (g_theWindow)
        {
            outW = g_theWindow->GetClientWidth();
            outH = g_theWindow->GetClientHeight();
        }
    }

    ImVec2 ImguiConsoleFullRenderer::CalcWindowPosition()
    {
        return ImVec2(0.0f, 0.0f);
    }

    ImVec2 ImguiConsoleFullRenderer::CalcWindowSize()
    {
        float clientW, clientH;
        GetClientSize(clientW, clientH);
        float inputBarH = CONSOLE_INPUT_HEIGHT + ImGui::GetStyle().WindowPadding.y * 2.0f;
        float totalH    = clientH * FULL_MODE_INPUT_Y_RATIO + inputBarH;
        return ImVec2(clientW, totalH);
    }

    //=========================================================================
    // Main entry - single window with child scroll region + input bar
    //=========================================================================
    void ImguiConsoleFullRenderer::Render(ImguiConsole& console)
    {
        ImVec2 pos  = CalcWindowPosition();
        ImVec2 size = CalcWindowSize();

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, CONSOLE_COLOR_BG);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
                               | ImGuiWindowFlags_NoResize
                               | ImGuiWindowFlags_NoMove
                               | ImGuiWindowFlags_NoDocking
                               | ImGuiWindowFlags_NoCollapse
                               | ImGuiWindowFlags_NoScrollbar;

        if (ImGui::Begin("Console", nullptr, flags))
        {
            // --- Scrollable message region (child window) ---
            float inputBarH = CONSOLE_INPUT_HEIGHT + ImGui::GetStyle().ItemSpacing.y;
            float childH    = ImGui::GetContentRegionAvail().y - inputBarH;
            if (childH < 0.0f) childH = 0.0f;

            if (ImGui::BeginChild("##ConsoleMessages", ImVec2(0.0f, childH), false))
            {
                const auto& msgs = console.GetConsoleMessages();

                // Push messages to bottom when content is shorter than region
                float lineH    = ImGui::GetTextLineHeightWithSpacing();
                float contentH = static_cast<float>(msgs.size()) * lineH;
                float availH   = ImGui::GetContentRegionAvail().y;
                if (contentH < availH)
                {
                    ImGui::Dummy(ImVec2(0.0f, availH - contentH));
                }

                for (const auto& msg : msgs)
                {
                    RenderColoredText(msg);
                }

                // Auto-scroll to bottom
                if (console.GetScrollToBottom())
                {
                    ImGui::SetScrollHereY(1.0f);
                    console.GetScrollToBottom() = false;
                }
            }
            ImGui::EndChild();

            // --- Input bar ---
            // Record input bar screen position for overlay positioning
            ImVec2 inputScreenPos = ImGui::GetCursorScreenPos();
            console.m_inputBarScreenX = inputScreenPos.x;
            console.m_inputBarScreenY = inputScreenPos.y;
            console.m_inputBarWidth   = ImGui::GetContentRegionAvail().x;

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
        ImGui::End();
        ImGui::PopStyleColor();
    }

    //=========================================================================
    // Colored text rendering
    //=========================================================================
    void ImguiConsoleFullRenderer::RenderColoredText(const ConsoleMessage& msg)
    {
        ImVec4 color = CONSOLE_COLOR_LOG;
        switch (msg.level)
        {
        case LogLevel::WARNING: color = CONSOLE_COLOR_WARNING; break;
        case LogLevel::ERROR_:  color = CONSOLE_COLOR_ERROR;   break;
        default: break;
        }

        // Use custom color if message has non-white color
        if (msg.color != Rgba8::WHITE)
        {
            color = ImVec4(msg.color.r / 255.0f, msg.color.g / 255.0f,
                           msg.color.b / 255.0f, msg.color.a / 255.0f);
        }

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(msg.text.c_str());
        ImGui::PopStyleColor();
    }
} // namespace enigma::core
