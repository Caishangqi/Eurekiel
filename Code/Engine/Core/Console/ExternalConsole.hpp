#pragma once
#include <memory>
#include "ConsoleConfig.hpp"
#include "Platform/WindowsConsole.hpp"
#include "Engine/Core/Rgba8.hpp"

namespace enigma::core {

    // Cross-platform external console wrapper
    class ExternalConsole {
    public:
        ExternalConsole(const ConsoleConfig& config);
        ~ExternalConsole();

        // Lifecycle
        bool Initialize();
        void Shutdown();

        // Console control
        bool Show();
        bool Hide();
        bool IsVisible() const;
        bool HasFocus() const;

        // Output operations only (input handled by InputSystem)
        void Write(const std::string& text);
        void WriteColored(const std::string& text, const Rgba8& color);
        void WriteLine(const std::string& text);
        void Clear();
        
        // Direct input processing (for when InputSystem can't reach us)
        void ProcessConsoleInput();
        bool HasPendingInput() const;

        // Cursor and display
        void SetCursorPosition(int x, int y);
        void ShowCursor(bool show);
        void UpdateInputLine(const std::string& input, int cursorPos);
        void SetTitle(const std::string& title);

        // Console properties
        bool SupportsAnsiColors() const;
        void SetSize(int columns, int rows);

        // Window events
        bool IsCloseRequested() const;
        void ResetCloseRequest();

    private:
        ConsoleConfig m_config;
        
        // Platform-specific implementation
        std::unique_ptr<WindowsConsole> m_platformConsole;
    };

} // namespace enigma::core