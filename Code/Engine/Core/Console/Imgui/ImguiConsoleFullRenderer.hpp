#pragma once

struct ImVec2;

namespace enigma::core
{
    class ImguiConsole;
    struct ConsoleMessage;

    // Static-only renderer for Full console mode
    // Renders a single window containing a scrollable message child region + input bar
    // Single-window approach eliminates z-order issues with other ImGui panels
    class ImguiConsoleFullRenderer
    {
    public:
        ImguiConsoleFullRenderer()                                             = delete;
        ImguiConsoleFullRenderer(const ImguiConsoleFullRenderer&)              = delete;
        ImguiConsoleFullRenderer& operator=(const ImguiConsoleFullRenderer&)   = delete;

        // Main render entry (called from ImguiConsole::Render() in Full mode)
        static void Render(ImguiConsole& console);

    private:
        static ImVec2 CalcWindowPosition();
        static ImVec2 CalcWindowSize();

        static void RenderColoredText(const ConsoleMessage& msg);
    };
} // namespace enigma::core
