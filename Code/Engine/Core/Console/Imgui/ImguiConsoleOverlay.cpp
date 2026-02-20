#include "Engine/Core/Console/Imgui/ImguiConsoleOverlay.hpp"
#include "Engine/Core/Console/Imgui/ImguiConsole.hpp"
#include "Engine/Core/Console/ImguiConsoleConfig.hpp"
#include "Engine/Core/Console/ConsoleSubsystem.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Game/GameCommon.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_internal.h"

#include <algorithm>
#include <cctype>

namespace enigma::core
{
    //=========================================================================
    // Main render entry - positions overlay popup above the console input line
    //=========================================================================
    void ImguiConsoleOverlay::Render(ImguiConsole& console)
    {
        if (!console.GetOverlayVisible())
        {
            return;
        }

        OverlayMode mode = console.GetOverlayMode();
        if (mode == OverlayMode::None)
        {
            return;
        }

        // Calculate popup position above the console input line
        // Use stored input bar screen position (set by renderers each frame)
        float inputBarX = console.m_inputBarScreenX;
        float inputBarY = console.m_inputBarScreenY;
        float inputBarW = console.m_inputBarWidth;

        if (inputBarW <= 0.0f)
        {
            return; // Input bar not rendered yet
        }

        float popupWidth   = inputBarW * 0.9f;
        float itemHeight   = ImGui::GetTextLineHeightWithSpacing();
        int   maxVisible   = OVERLAY_MAX_VISIBLE_ITEMS;

        // Autocomplete mode: size popup to fit the longest command + description
        if (mode == OverlayMode::Autocomplete)
        {
            const std::string& input = console.GetInputBuffer();
            auto suggestions = GetAutocompleteSuggestions(input);
            float maxTextWidth = 0.0f;
            for (const auto& cmd : suggestions)
            {
                float w = ImGui::CalcTextSize(cmd.c_str()).x;
                // Account for description text width
                if (g_theConsole)
                {
                    const std::string& desc = g_theConsole->GetCommandDescription(cmd);
                    if (!desc.empty())
                    {
                        w += ImGui::CalcTextSize(("  " + desc).c_str()).x;
                    }
                }
                if (w > maxTextWidth)
                {
                    maxTextWidth = w;
                }
            }
            float padding = ImGui::GetStyle().WindowPadding.x * 2.0f + ImGui::GetStyle().FramePadding.x * 2.0f;
            float contentWidth = maxTextWidth + padding;
            float minWidth = 200.0f;
            float maxWidth = inputBarW;
            popupWidth = (std::clamp)(contentWidth, minWidth, maxWidth);
        }

        // History mode: size popup to fit the longest history entry
        if (mode == OverlayMode::History)
        {
            const auto& history = console.GetCommandHistory();
            float maxTextWidth = 0.0f;
            for (const auto& entry : history)
            {
                float w = ImGui::CalcTextSize(entry.c_str()).x;
                if (w > maxTextWidth)
                {
                    maxTextWidth = w;
                }
            }
            // Add padding for window margins
            float padding = ImGui::GetStyle().WindowPadding.x * 2.0f + ImGui::GetStyle().FramePadding.x * 2.0f;
            float contentWidth = maxTextWidth + padding;
            // Clamp: at least 200px, at most input bar width
            float minWidth = 200.0f;
            float maxWidth = inputBarW;
            popupWidth = (std::clamp)(contentWidth, minWidth, maxWidth);
        }

        float popupHeight  = itemHeight * static_cast<float>(maxVisible)
                           + ImGui::GetStyle().WindowPadding.y * 2.0f;

        // Position: above the input line, left-aligned with input bar
        float popupX = inputBarX;
        float popupY = inputBarY - popupHeight - ImGui::GetStyle().ItemSpacing.y;

        ImGui::SetNextWindowPos(ImVec2(popupX, popupY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(popupWidth, popupHeight), ImGuiCond_Always);

        ImGuiWindowFlags popupFlags = ImGuiWindowFlags_NoTitleBar
                                    | ImGuiWindowFlags_NoResize
                                    | ImGuiWindowFlags_NoMove
                                    | ImGuiWindowFlags_NoSavedSettings
                                    | ImGuiWindowFlags_NoFocusOnAppearing
                                    | ImGuiWindowFlags_NoDocking;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, OVERLAY_BG_COLOR);
        if (ImGui::Begin("##ConsoleOverlay", nullptr, popupFlags))
        {
            // Force overlay to topmost z-order
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
            if (mode == OverlayMode::Autocomplete)
            {
                RenderAutocompleteList(console);
            }
            else if (mode == OverlayMode::History)
            {
                RenderHistoryList(console);
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();
    }

    //=========================================================================
    // Trigger conditions (kept for external queries, but Render uses OverlayMode)
    //=========================================================================
    bool ImguiConsoleOverlay::ShouldShowAutocomplete(const ImguiConsole& console)
    {
        return console.GetOverlayMode() == OverlayMode::Autocomplete;
    }

    bool ImguiConsoleOverlay::ShouldShowHistory(const ImguiConsole& console)
    {
        return console.GetOverlayMode() == OverlayMode::History;
    }

    //=========================================================================
    // Quick check for autocomplete matches (avoids overlay flicker)
    //=========================================================================
    bool ImguiConsoleOverlay::HasAutocompleteSuggestions(const std::string& input)
    {
        if (input.empty() || !g_theConsole)
        {
            return false;
        }

        const auto& commands = g_theConsole->GetRegisteredCommands();
        for (const auto& cmd : commands)
        {
            if (FuzzyMatch(cmd, input))
            {
                return true;
            }
        }
        return false;
    }

    //=========================================================================
    // Autocomplete list with fuzzy match highlighting
    //=========================================================================
    void ImguiConsoleOverlay::RenderAutocompleteList(ImguiConsole& console)
    {
        const std::string& input = console.GetInputBuffer();
        std::vector<std::string> suggestions = GetAutocompleteSuggestions(input);

        if (suggestions.empty())
        {
            // No matches -> close overlay entirely
            console.GetOverlayVisible() = false;
            console.GetOverlayMode()    = OverlayMode::None;
            console.GetOverlaySelectedIndex() = -1;
            return;
        }

        int& selectedIndex = console.GetOverlaySelectedIndex();
        selectedIndex = (std::clamp)(selectedIndex, 0, static_cast<int>(suggestions.size()) - 1);

        // Handle navigation (PageUp/Down, Enter, Escape)
        HandleOverlayNavigation(console, static_cast<int>(suggestions.size()));

        for (int i = 0; i < static_cast<int>(suggestions.size()); ++i)
        {
            ImGui::PushID(i);

            bool isSelected = (i == selectedIndex);

            if (isSelected)
            {
                ImGui::PushStyleColor(ImGuiCol_Header, OVERLAY_SELECTED_COLOR);
            }

            if (ImGui::Selectable("##item", isSelected, ImGuiSelectableFlags_AllowOverlap))
            {
                HandleOverlayMouseInteraction(console, i, suggestions[i]);
            }

            if (ImGui::IsItemHovered() && !isSelected)
            {
                ImVec2 itemMin = ImGui::GetItemRectMin();
                ImVec2 itemMax = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddRectFilled(itemMin, itemMax,
                    ImGui::ColorConvertFloat4ToU32(OVERLAY_HOVER_COLOR));
            }

            if (isSelected)
            {
                ImGui::PopStyleColor();
            }

            // Render fuzzy-highlighted text on same line
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x);
            RenderFuzzyMatchHighlight(suggestions[i], input);

            // Render command description in dimmed color after the command name
            if (g_theConsole)
            {
                const std::string& desc = g_theConsole->GetCommandDescription(suggestions[i]);
                if (!desc.empty())
                {
                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, OVERLAY_DESCRIPTION_COLOR);
                    ImGui::TextUnformatted(("  " + desc).c_str());
                    ImGui::PopStyleColor();
                }
            }

            ImGui::PopID();
        }
    }

    //=========================================================================
    // Get autocomplete suggestions via fuzzy matching
    //=========================================================================
    std::vector<std::string> ImguiConsoleOverlay::GetAutocompleteSuggestions(const std::string& input)
    {
        std::vector<std::string> results;

        if (g_theConsole)
        {
            const auto& commands = g_theConsole->GetRegisteredCommands();
            for (const auto& cmd : commands)
            {
                if (FuzzyMatch(cmd, input))
                {
                    results.push_back(cmd);
                }
            }
        }

        return results;
    }

    //=========================================================================
    // Fuzzy subsequence matching (case-insensitive)
    //=========================================================================
    bool ImguiConsoleOverlay::FuzzyMatch(const std::string& text, const std::string& pattern,
                                          std::vector<int>* outMatchedIndices)
    {
        if (pattern.empty())
        {
            return true;
        }
        if (text.empty())
        {
            return false;
        }

        int patternIdx = 0;
        int patternLen = static_cast<int>(pattern.size());

        for (int i = 0; i < static_cast<int>(text.size()) && patternIdx < patternLen; ++i)
        {
            if (std::tolower(static_cast<unsigned char>(text[i])) ==
                std::tolower(static_cast<unsigned char>(pattern[patternIdx])))
            {
                if (outMatchedIndices)
                {
                    outMatchedIndices->push_back(i);
                }
                patternIdx++;
            }
        }

        return patternIdx == patternLen;
    }

    // APPEND_OVERLAY_2

    //=========================================================================
    // Render text with fuzzy-matched characters highlighted
    //=========================================================================
    void ImguiConsoleOverlay::RenderFuzzyMatchHighlight(const std::string& text, const std::string& pattern)
    {
        std::vector<int> matchedIndices;
        FuzzyMatch(text, pattern, &matchedIndices);

        std::vector<bool> isMatched(text.size(), false);
        for (int idx : matchedIndices)
        {
            if (idx >= 0 && idx < static_cast<int>(text.size()))
            {
                isMatched[idx] = true;
            }
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImU32 highlightColor  = ImGui::ColorConvertFloat4ToU32(OVERLAY_MATCH_HIGHLIGHT);
        ImU32 normalTextColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImU32 matchTextColor  = ImGui::ColorConvertFloat4ToU32(ImVec4(0.4f, 1.0f, 0.4f, 1.0f));

        float x = pos.x;
        float y = pos.y;
        float charHeight = ImGui::GetTextLineHeight();

        for (int i = 0; i < static_cast<int>(text.size()); ++i)
        {
            char ch[2] = { text[i], '\0' };
            float charWidth = ImGui::CalcTextSize(ch).x;

            if (isMatched[i])
            {
                drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + charWidth, y + charHeight), highlightColor);
                drawList->AddText(ImVec2(x, y), matchTextColor, ch);
            }
            else
            {
                drawList->AddText(ImVec2(x, y), normalTextColor, ch);
            }

            x += charWidth;
        }

        ImGui::Dummy(ImVec2(x - pos.x, charHeight));
    }

    //=========================================================================
    // History list rendering (most recent first)
    //=========================================================================
    void ImguiConsoleOverlay::RenderHistoryList(ImguiConsole& console)
    {
        const auto& history = console.GetCommandHistory();
        if (history.empty())
        {
            ImGui::TextDisabled("No command history");
            return;
        }

        int& selectedIndex = console.GetOverlaySelectedIndex();
        int historySize = static_cast<int>(history.size());
        selectedIndex = (std::clamp)(selectedIndex, 0, historySize - 1);

        // Handle navigation (PageUp/Down, Enter, Escape - no Up/Down here)
        HandleOverlayNavigation(console, historySize);

        // Render in reverse order (most recent first)
        for (int i = historySize - 1; i >= 0; --i)
        {
            ImGui::PushID(i);

            int displayIdx = historySize - 1 - i;
            bool isSelected = (displayIdx == selectedIndex);

            if (isSelected)
            {
                ImGui::PushStyleColor(ImGuiCol_Header, OVERLAY_SELECTED_COLOR);
            }

            if (ImGui::Selectable(history[i].c_str(), isSelected))
            {
                HandleOverlayMouseInteraction(console, displayIdx, history[i]);
            }

            if (isSelected)
            {
                ImGui::PopStyleColor();
            }

            if (ImGui::IsItemHovered() && !isSelected)
            {
                ImVec2 itemMin = ImGui::GetItemRectMin();
                ImVec2 itemMax = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddRectFilled(itemMin, itemMax,
                    ImGui::ColorConvertFloat4ToU32(OVERLAY_HOVER_COLOR));
            }

            ImGui::PopID();
        }
    }

    //=========================================================================
    // Overlay navigation: PageUp/Down, Enter, Escape, custom accept key
    // Up/Down is handled by InputText callback (HandleHistoryNavigation)
    //=========================================================================
    void ImguiConsoleOverlay::HandleOverlayNavigation(ImguiConsole& console, int itemCount)
    {
        if (itemCount <= 0)
        {
            return;
        }

        int& selectedIndex = console.GetOverlaySelectedIndex();

        if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
        {
            selectedIndex = (std::max)(0, selectedIndex - OVERLAY_MAX_VISIBLE_ITEMS);
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
        {
            selectedIndex = (std::min)(itemCount - 1, selectedIndex + OVERLAY_MAX_VISIBLE_ITEMS);
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            console.GetOverlayVisible() = false;
            console.GetOverlayMode()    = OverlayMode::None;
            console.GetOverlaySelectedIndex() = -1;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)
                 || ShouldAcceptByCustomKey(console))
        {
            if (selectedIndex >= 0 && selectedIndex < itemCount)
            {
                if (console.GetOverlayMode() == OverlayMode::Autocomplete)
                {
                    auto suggestions = GetAutocompleteSuggestions(console.GetInputBuffer());
                    if (selectedIndex < static_cast<int>(suggestions.size()))
                    {
                        console.GetInputBuffer() = suggestions[selectedIndex];
                    }
                }
                else if (console.GetOverlayMode() == OverlayMode::History)
                {
                    const auto& history = console.GetCommandHistory();
                    int historyIdx = static_cast<int>(history.size()) - 1 - selectedIndex;
                    if (historyIdx >= 0 && historyIdx < static_cast<int>(history.size()))
                    {
                        console.GetInputBuffer() = history[historyIdx];
                    }
                }
            }
            console.GetOverlayVisible() = false;
            console.GetOverlayMode()    = OverlayMode::None;
            console.GetOverlaySelectedIndex() = -1;
        }
    }

    //=========================================================================
    // Check if the custom autocomplete accept key (non-Tab) was pressed.
    // Only active when autocompleteAcceptKey differs from VK_TAB (0x09),
    // since Tab is already handled by ImGui's CallbackCompletion pathway.
    // Only triggers in Autocomplete mode (not History mode).
    //=========================================================================
    bool ImguiConsoleOverlay::ShouldAcceptByCustomKey(const ImguiConsole& console)
    {
        if (console.GetOverlayMode() != OverlayMode::Autocomplete)
        {
            return false;
        }

        int acceptKey = console.GetConfig().autocompleteAcceptKey;
        if (acceptKey == 0x09) // VK_TAB - already handled via CallbackCompletion
        {
            return false;
        }

        if (g_theInput)
        {
            return g_theInput->WasKeyJustPressed(static_cast<unsigned char>(acceptKey));
        }

        return false;
    }

    //=========================================================================
    // Mouse interaction: click fills input and closes overlay
    //=========================================================================
    void ImguiConsoleOverlay::HandleOverlayMouseInteraction(ImguiConsole& console, int itemIndex,
                                                             const std::string& itemText)
    {
        (void)itemIndex;
        console.GetInputBuffer() = itemText;
        console.GetOverlayVisible() = false;
        console.GetOverlayMode()    = OverlayMode::None;
        console.GetOverlaySelectedIndex() = -1;
    }
} // namespace enigma::core
