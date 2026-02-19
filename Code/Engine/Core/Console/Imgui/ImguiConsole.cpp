#include "Engine/Core/Console/Imgui/ImguiConsole.hpp"
#include "Engine/Core/Console/Imgui/ImguiConsoleRenderer.hpp"
#include "Engine/Core/Console/Imgui/ImguiConsoleFullRenderer.hpp"
#include "Engine/Core/Console/Imgui/ImguiConsoleOverlay.hpp"
#include "Engine/Core/Console/ImguiConsoleConfig.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_internal.h"

namespace enigma::core
{
    ImguiConsole::ImguiConsole(const ConsoleConfig& config)
        : m_config(config)
    {
        m_maxMessages = static_cast<size_t>(config.maxImguiMessages);
    }

    ImguiConsole::~ImguiConsole() = default;

    void ImguiConsole::Render()
    {
        if (m_mode == ConsoleMode::Hidden || !ImGui::GetCurrentContext())
        {
            return;
        }

        if (m_mode == ConsoleMode::Full)
        {
            ImguiConsoleFullRenderer::Render(*this);
            ImguiConsoleOverlay::Render(*this);
            return;
        }

        if (m_mode == ConsoleMode::Terminal)
        {
            // Terminal: fixed bottom input bar, no title bar, no drag
            ImVec2 pos  = ImguiConsoleRenderer::CalcBottomPosition();
            ImVec2 size = ImguiConsoleRenderer::CalcBottomSize();
            ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(size, ImGuiCond_Always);

            ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
                                   | ImGuiWindowFlags_NoResize
                                   | ImGuiWindowFlags_NoMove
                                   | ImGuiWindowFlags_NoScrollbar
                                   | ImGuiWindowFlags_NoDocking
                                   | ImGuiWindowFlags_NoCollapse;

            if (ImGui::Begin("Console", nullptr, flags))
            {
                ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
                ImguiConsoleRenderer::RenderTerminalMode(*this);
            }
            ImGui::End();
        }
        else // Docked
        {
            // Docked: normal window, DockBuilder controls position
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse
                                   | ImGuiWindowFlags_NoMove
                                   | ImGuiWindowFlags_NoTitleBar
                                   | ImGuiWindowFlags_NoScrollbar;

            if (ImGui::Begin("Console", nullptr, flags))
            {
                ImguiConsoleRenderer::RenderDockedMode(*this);
            }
            ImGui::End();
        }

        // Overlay (Terminal and Docked modes)
        ImguiConsoleOverlay::Render(*this);
    }

    void ImguiConsole::AddMessage(const ConsoleMessage& message)
    {
        m_messages.push_back(message);

        // Enforce max message cap
        while (m_messages.size() > m_maxMessages)
        {
            m_messages.pop_front();
        }

        // Auto-scroll to bottom on new message
        if (m_autoScroll)
        {
            m_scrollToBottom = true;
        }
    }

    void ImguiConsole::AddConsoleMessage(const ConsoleMessage& message)
    {
        m_consoleMessages.push_back(message);

        // Enforce max message cap
        while (m_consoleMessages.size() > m_maxMessages)
        {
            m_consoleMessages.pop_front();
        }

        // Also add to general messages buffer
        AddMessage(message);
    }

    void ImguiConsole::Clear()
    {
        m_messages.clear();
        m_consoleMessages.clear();
    }
} // namespace enigma::core
