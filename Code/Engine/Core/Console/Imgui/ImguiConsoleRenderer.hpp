#pragma once

struct ImVec2;
struct ImVec4;

namespace enigma::core
{
    class ImguiConsole;
    struct ConsoleMessage;

    // Static-only rendering module for ImGui Console
    // Two rendering modes:
    //   1. Terminal Mode - fixed bottom input bar ("> " prompt)
    //   2. Docked Mode  - input bar docked below MessageLogUI ("Cmd" label)
    // Both modes render only an input line; message display is handled by MessageLogUI in Docked mode.
    class ImguiConsoleRenderer
    {
    public:
        // Static-only class - no instantiation
        ImguiConsoleRenderer()                                         = delete;
        ImguiConsoleRenderer(const ImguiConsoleRenderer&)              = delete;
        ImguiConsoleRenderer& operator=(const ImguiConsoleRenderer&)   = delete;

        // Mode-specific rendering (called from ImguiConsole::Render())
        static void RenderTerminalMode(ImguiConsole& console);
        static void RenderDockedMode(ImguiConsole& console);

        // Bottom position calculation for Terminal mode
        static ImVec2 CalcBottomPosition();
        static ImVec2 CalcBottomSize();

    private:
        // Shared rendering utilities
        static void RenderColoredText(const ConsoleMessage& msg);
        static ImVec4 GetMessageColor(const ConsoleMessage& msg);
    };
} // namespace enigma::core
