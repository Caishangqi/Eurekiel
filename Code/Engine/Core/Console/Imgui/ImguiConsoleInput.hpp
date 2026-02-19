#pragma once

struct ImGuiInputTextCallbackData;

namespace enigma::core
{
    class ImguiConsole;

    // Static-only input handling module for ImGui Console
    // Processes keyboard input, command submission, history navigation,
    // and autocomplete triggering. Command execution goes through
    // g_theConsole->Execute() (ConsoleSubsystem is the sole authority).
    class ImguiConsoleInput
    {
    public:
        // Static-only class - no instantiation
        ImguiConsoleInput()                                      = delete;
        ImguiConsoleInput(const ImguiConsoleInput&)              = delete;
        ImguiConsoleInput& operator=(const ImguiConsoleInput&)   = delete;

        // ImGui InputText callback (compatible with ImGuiInputTextCallback signature)
        // UserData must point to the owning ImguiConsole instance
        static int InputTextCallback(ImGuiInputTextCallbackData* data);

        // Command submission: execute via g_theConsole, add to history, clear input
        static void SubmitCommand(ImguiConsole& console);

        // Clipboard copy of selected text
        static void CopySelection(ImguiConsole& console);

    private:
        // History Up/Down arrow navigation within the callback
        static void HandleHistoryNavigation(ImGuiInputTextCallbackData* data, ImguiConsole& console);

        // Tab-triggered autocomplete (delegates rendering to ImguiConsoleOverlay)
        static void HandleAutoComplete(ImGuiInputTextCallbackData* data, ImguiConsole& console);

        // Text edit callback: auto-triggers autocomplete on typing
        static void HandleEdit(ImGuiInputTextCallbackData* data, ImguiConsole& console);
    };
} // namespace enigma::core
