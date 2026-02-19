#include "Engine/Core/Console/Imgui/ImguiConsoleInput.hpp"
#include "Engine/Core/Console/Imgui/ImguiConsole.hpp"
#include "Engine/Core/Console/Imgui/ImguiConsoleOverlay.hpp"
#include "Engine/Core/Console/ConsoleSubsystem.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "ThirdParty/imgui/imgui.h"

#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace enigma::core
{
    //=========================================================================
    // ImGui InputText callback dispatcher
    // Routes events based on EventFlag:
    //   - CallbackHistory:   Up/Down arrow -> overlay history navigation
    //   - CallbackCompletion: Tab -> toggle autocomplete
    //   - CallbackEdit:      Text changed -> auto-trigger autocomplete
    //=========================================================================
    int ImguiConsoleInput::InputTextCallback(ImGuiInputTextCallbackData* data)
    {
        auto* console = static_cast<ImguiConsole*>(data->UserData);
        if (!console)
        {
            return 0;
        }

        switch (data->EventFlag)
        {
        case ImGuiInputTextFlags_CallbackHistory:
            HandleHistoryNavigation(data, *console);
            break;

        case ImGuiInputTextFlags_CallbackCompletion:
            HandleAutoComplete(data, *console);
            break;

        case ImGuiInputTextFlags_CallbackEdit:
            HandleEdit(data, *console);
            break;

        case ImGuiInputTextFlags_CallbackCharFilter:
        {
            // Block the toggle key character from being typed
            int toggleKey = console->GetConfig().imguiToggleKey;
            unsigned int toggleChar = MapVirtualKeyA(static_cast<unsigned int>(toggleKey), MAPVK_VK_TO_CHAR);
            if (data->EventChar == static_cast<ImWchar>(toggleChar))
            {
                return 1; // Reject this character
            }
            break;
        }

        default:
            break;
        }

        return 0;
    }

    //=========================================================================
    // Command submission
    // Executes via g_theConsole->Execute(), adds to history, clears input
    //=========================================================================
    void ImguiConsoleInput::SubmitCommand(ImguiConsole& console)
    {
        std::string& inputBuf = console.GetInputBuffer();
        if (inputBuf.empty())
        {
            return;
        }

        // Add to command history (avoid consecutive duplicates)
        auto& history = console.GetCommandHistory();
        if (history.empty() || history.back() != inputBuf)
        {
            history.push_back(inputBuf);
        }

        // Reset history navigation index
        console.GetHistoryIndex() = -1;

        // Execute command through ConsoleSubsystem (sole authority)
        if (g_theConsole)
        {
            g_theConsole->Execute(inputBuf);
        }

        // Clear input buffer and close overlay
        inputBuf.clear();
        console.GetOverlayVisible() = false;
        console.GetOverlayMode()    = OverlayMode::None;
        console.GetOverlaySelectedIndex() = -1;

        // Auto-scroll to see command output
        console.GetScrollToBottom() = true;
    }

    //=========================================================================
    // Clipboard copy
    //=========================================================================
    void ImguiConsoleInput::CopySelection(ImguiConsole& console)
    {
        const std::string& inputBuf = console.GetInputBuffer();
        if (!inputBuf.empty())
        {
            ImGui::SetClipboardText(inputBuf.c_str());
        }
    }

    //=========================================================================
    // History navigation (Up/Down arrow keys)
    // UE style: fills input with selected history entry, cycles at boundaries
    //=========================================================================
    void ImguiConsoleInput::HandleHistoryNavigation(ImGuiInputTextCallbackData* data, ImguiConsole& console)
    {
        auto& history = console.GetCommandHistory();
        if (history.empty())
        {
            return;
        }

        const int historySize = static_cast<int>(history.size());
        int& selectedIndex = console.GetOverlaySelectedIndex();

        // If overlay is not in history mode, open it
        if (!console.GetOverlayVisible() || console.GetOverlayMode() != OverlayMode::History)
        {
            console.GetOverlayVisible() = true;
            console.GetOverlayMode()    = OverlayMode::History;
            selectedIndex = 0; // Most recent
        }
        else
        {
            // Navigate with wrap-around
            if (data->EventKey == ImGuiKey_UpArrow)
            {
                selectedIndex++;
                if (selectedIndex >= historySize)
                {
                    selectedIndex = 0; // Wrap to most recent
                }
            }
            else if (data->EventKey == ImGuiKey_DownArrow)
            {
                selectedIndex--;
                if (selectedIndex < 0)
                {
                    selectedIndex = historySize - 1; // Wrap to oldest
                }
            }
        }

        // Fill input buffer with selected history entry
        int historyIdx = historySize - 1 - selectedIndex;
        if (historyIdx >= 0 && historyIdx < historySize)
        {
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, history[historyIdx].c_str());
            console.GetInputBuffer() = history[historyIdx];
        }
    }

    //=========================================================================
    // Autocomplete trigger (Tab key)
    // Toggles autocomplete overlay when input is non-empty
    //=========================================================================
    void ImguiConsoleInput::HandleAutoComplete(ImGuiInputTextCallbackData* data, ImguiConsole& console)
    {
        if (data->BufTextLen == 0)
        {
            return;
        }

        // Sync input buffer from callback data
        console.GetInputBuffer().assign(data->Buf, data->BufTextLen);

        bool& overlayVisible = console.GetOverlayVisible();
        if (overlayVisible && console.GetOverlayMode() == OverlayMode::Autocomplete)
        {
            // Already showing autocomplete -> close
            overlayVisible = false;
            console.GetOverlayMode() = OverlayMode::None;
            console.GetOverlaySelectedIndex() = -1;
        }
        else
        {
            // Open autocomplete
            overlayVisible = true;
            console.GetOverlayMode() = OverlayMode::Autocomplete;
            console.GetOverlaySelectedIndex() = 0;
        }
    }

    //=========================================================================
    // Edit callback: auto-trigger autocomplete when user types
    // Fires on every text change (character input, backspace, paste, etc.)
    // Skipped when overlay is in History mode (history navigation fills input)
    //=========================================================================
    void ImguiConsoleInput::HandleEdit(ImGuiInputTextCallbackData* data, ImguiConsole& console)
    {
        // Don't switch to autocomplete while navigating history
        // (history navigation fills the input buffer, which triggers CallbackEdit)
        if (console.GetOverlayMode() == OverlayMode::History)
        {
            return;
        }

        // Sync input buffer
        console.GetInputBuffer().assign(data->Buf, data->BufTextLen);

        if (data->BufTextLen > 0)
        {
            // Non-empty input -> show autocomplete only if there are matches
            std::string input(data->Buf, data->BufTextLen);
            bool hasMatches = ImguiConsoleOverlay::HasAutocompleteSuggestions(input);
            if (hasMatches)
            {
                console.GetOverlayVisible() = true;
                console.GetOverlayMode()    = OverlayMode::Autocomplete;
                console.GetOverlaySelectedIndex() = 0;
            }
            else
            {
                console.GetOverlayVisible() = false;
                console.GetOverlayMode()    = OverlayMode::None;
                console.GetOverlaySelectedIndex() = -1;
            }
        }
        else
        {
            // Empty input -> close overlay (user can press Up to open history)
            console.GetOverlayVisible() = false;
            console.GetOverlayMode()    = OverlayMode::None;
            console.GetOverlaySelectedIndex() = -1;
        }

        // Reset history index when user edits text
        console.GetHistoryIndex() = -1;
    }
} // namespace enigma::core
