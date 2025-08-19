#pragma once
#include <string>
#include <atomic>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../ConsoleConfig.hpp"
#include "Engine/Core/Rgba8.hpp"

namespace enigma::core {

    // Windows-specific console implementation
    class WindowsConsole {
    public:
        WindowsConsole(const ConsoleConfig& config);
        ~WindowsConsole();

        // Lifecycle
        bool Initialize();
        void Shutdown();

        // Console control
        bool Show();
        bool Hide();
        bool IsVisible() const { return m_isVisible; }
        bool HasFocus() const;

        // Output operations only (input handled by InputSystem)
        void Write(const std::string& text);
        void WriteColored(const std::string& text, const Rgba8& color);
        void WriteLine(const std::string& text);
        void Clear();

        // Cursor and display
        void SetCursorPosition(int x, int y);
        void ShowCursor(bool show);
        void UpdateInputLine(const std::string& input, int cursorPos);
        void SetTitle(const std::string& title);

        // Console properties
        bool SupportsAnsiColors() const { return m_supportsAnsi; }
        void SetSize(int columns, int rows);

        // Window events
        bool IsCloseRequested() const { return m_closeRequested.load(); }
        void ResetCloseRequest() { m_closeRequested.store(false); }

        // Direct console input processing (when InputSystem can't reach us)
        void ProcessConsoleInput();
        bool HasPendingInput() const;

    private:
        // Windows specific implementation
        bool InitializeWindowsConsole();
        void ShutdownWindowsConsole();
        bool EnableVirtualTerminalProcessing();
        void ConfigureConsoleMode();
        bool SetupConsoleCloseHandler();
        void ConfigureStdioRedirection();
        
        // Input handling (removed - using InputSystem instead)

        // Windows console close handler
        static BOOL WINAPI ConsoleCloseHandler(DWORD dwCtrlType);
        static WindowsConsole* s_instance;  // For static callback

        // Handle management
        HANDLE m_consoleInput = INVALID_HANDLE_VALUE;
        HANDLE m_consoleOutput = INVALID_HANDLE_VALUE;
        HWND m_consoleWindow = nullptr;

        // State
        ConsoleConfig m_config;
        bool m_initialized = false;
        bool m_isVisible = false;
        bool m_supportsAnsi = false;
        bool m_ownedConsole = false;
        std::atomic<bool> m_closeRequested{false};

        // Display state
        int m_currentLine = 0;
        int m_maxLines = 30;

        // Original console state (for restoration)
        DWORD m_originalInputMode = 0;
        DWORD m_originalOutputMode = 0;
        bool m_hadConsole = false;
    };

} // namespace enigma::core