#pragma once

#include <deque>
#include <string>
#include <vector>

#include "Engine/Core/Console/ConsoleConfig.hpp"
#include "Engine/Core/Console/ConsoleMessage.hpp"

namespace enigma::core
{
    // Forward declarations for static modules
    class ImguiConsoleRenderer;
    class ImguiConsoleFullRenderer;
    class ImguiConsoleInput;
    class ImguiConsoleOverlay;

    // Console rendering mode
    enum class ConsoleMode
    {
        Hidden,   // Not displayed
        Terminal, // Fixed bottom input bar (input line only)
        Full,     // Centered input + black message overlay above
        Docked    // Docked below MessageLogUI (MessageLogUI open)
    };

    // Overlay display mode
    enum class OverlayMode
    {
        None,         // Overlay hidden
        History,      // Showing command history (triggered by Up/Down on empty input)
        Autocomplete  // Showing autocomplete suggestions (triggered by typing)
    };

    // ImGui Console core state management class
    // Holds message buffer, input state, and mode state.
    // Dispatches rendering to ImguiConsoleRenderer, input to ImguiConsoleInput,
    // and overlay to ImguiConsoleOverlay (all static-only classes)
    class ImguiConsole
    {
        friend class ImguiConsoleRenderer;
        friend class ImguiConsoleFullRenderer;
        friend class ImguiConsoleInput;
        friend class ImguiConsoleOverlay;

    public:
        explicit ImguiConsole(const ConsoleConfig& config);
        ~ImguiConsole();

        // Main render entry (called from ConsoleSubsystem::Update())
        void Render();

        // Message interface
        void AddMessage(const ConsoleMessage& message);
        void AddConsoleMessage(const ConsoleMessage& message);
        void Clear();

        // State queries (mode-based visibility)
        bool IsVisible() const { return m_mode != ConsoleMode::Hidden; }

        // Mode access
        ConsoleMode GetMode() const { return m_mode; }
        void        SetMode(ConsoleMode mode) { m_mode = mode; }

        // Input state access (for Input module)
        std::string&              GetInputBuffer() { return m_inputBuffer; }
        const std::string&        GetInputBuffer() const { return m_inputBuffer; }
        int&                      GetCursorPosition() { return m_cursorPosition; }
        std::vector<std::string>& GetCommandHistory() { return m_commandHistory; }
        const std::vector<std::string>& GetCommandHistory() const { return m_commandHistory; }
        int&                      GetHistoryIndex() { return m_historyIndex; }
        int                       GetHistoryIndex() const { return m_historyIndex; }

        // Message buffer access (for Renderer module)
        const std::deque<ConsoleMessage>& GetMessages() const { return m_messages; }
        const std::deque<ConsoleMessage>& GetConsoleMessages() const { return m_consoleMessages; }

        // Scroll state access (for Renderer module)
        bool& GetAutoScroll() { return m_autoScroll; }
        bool& GetScrollToBottom() { return m_scrollToBottom; }

        // Overlay state access (for Overlay module)
        bool& GetOverlayVisible() { return m_overlayVisible; }
        int&  GetOverlaySelectedIndex() { return m_overlaySelectedIndex; }
        OverlayMode  GetOverlayMode() const { return m_overlayMode; }
        OverlayMode& GetOverlayMode() { return m_overlayMode; }

        // Config access
        const ConsoleConfig& GetConfig() const { return m_config; }

    private:
        ConsoleMode m_mode = ConsoleMode::Hidden;

        // Message buffer (ring buffer with upper limit) - all messages
        std::deque<ConsoleMessage> m_messages;
        // Console-only messages (command echoes + command results) for Full mode overlay
        std::deque<ConsoleMessage> m_consoleMessages;
        size_t                     m_maxMessages = 10000;

        // Input state
        std::string m_inputBuffer;
        int         m_cursorPosition = 0;

        // Command history
        std::vector<std::string> m_commandHistory;
        int                      m_historyIndex = -1;

        // Overlay state
        bool        m_overlayVisible       = false;
        int         m_overlaySelectedIndex = -1;
        OverlayMode m_overlayMode          = OverlayMode::None;

        // Scroll state
        bool m_autoScroll    = true;
        bool m_scrollToBottom = false;

        ConsoleConfig m_config;
    };
} // namespace enigma::core
