#pragma once
#include <memory>
#include <string>

#include "Engine/Core/SubsystemManager.hpp"
#include "Engine/Core/Event/EventSubsystem.hpp"
#include "Engine/Core/Event/StringEventBus.hpp"
#include "Engine/Core/Logger/LogLevel.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "ConsoleConfig.hpp"
#include "ExternalConsole.hpp"

// Forward declarations
class DevConsole;

// Import EventArgs type alias for legacy compatibility
using enigma::event::EventArgs;

namespace enigma::core
{
    // Simplified console subsystem that only displays content
    // User input is forwarded to DevConsole for command execution
    class ConsoleSubsystem : public EngineSubsystem
    {
    public:
        DECLARE_SUBSYSTEM(ConsoleSubsystem, "Console", 90)

        // Constructors
        ConsoleSubsystem();
        explicit ConsoleSubsystem(const ConsoleConfig& config);
        ~ConsoleSubsystem();

        // EngineSubsystem interface
        void Initialize() override;
        void Startup() override;
        void Shutdown() override;
        void Update(float deltaTime) override; // Process direct console input
        bool RequiresGameLoop() const override { return true; } // Need Update() called
        bool RequiresInitialize() const override { return true; }

        // Simple output interface (immediate output, no threading)
        void WriteLine(const std::string& text, LogLevel level = LogLevel::INFO);
        void WriteLineColored(const std::string& text, const Rgba8& color);
        void WriteFormatted(LogLevel level, const char* format, ...);

        // Event handlers (integrate with InputSystem)
        static bool Event_ConsoleKeyPressed(EventArgs& args);
        static bool Event_ConsoleCharInput(EventArgs& args);
        static bool Event_ConsoleWindowClose(EventArgs& args);

        // Direct console input events (when InputSystem can't reach us)
        static bool Event_ConsoleDirectChar(EventArgs& args);
        static bool Event_ConsoleDirectEnter(EventArgs& args);
        static bool Event_ConsoleDirectBackspace(EventArgs& args);
        static bool Event_ConsoleDirectUpArrow(EventArgs& args);
        static bool Event_ConsoleDirectDownArrow(EventArgs& args);
        static bool Event_ConsoleDirectPaste(EventArgs& args);

        // DevConsole output mirroring event
        static bool Event_DevConsoleOutput(EventArgs& args);

        // Console control
        void SetVisible(bool visible);
        bool IsVisible() const;

        // Status
        bool IsInitialized() const { return m_initialized; }
        bool IsExternalConsoleActive() const;

    private:
        // Initialization helpers
        void CreateExternalConsole();
        void RegisterEventHandlers();
        void UnregisterEventHandlers();

        // Input processing - forward to DevConsole
        void ProcessCharInput(unsigned char character);
        void HandleBackspace();
        void HandleEnter();
        void HandleUpArrow();
        void HandleDownArrow();
        void HandlePaste();
        void HandleEscape();
        void HandleArrowKeys(unsigned char keyCode);

        // Internal helpers
        void UpdateInputDisplay();
        void ForwardCommandToDevConsole(const std::string& command);

        // Configuration
        ConsoleConfig m_config;
        bool          m_initialized = false;
        bool          m_isVisible   = false;

        // Simple external console (no threading)
        std::unique_ptr<ExternalConsole> m_externalConsole;

        // Input state for forwarding to DevConsole
        std::string m_currentInput;
        int         m_cursorPosition = 0;

        // History management (similar to DevConsole)
        std::vector<std::string> m_commandHistory;
        int                      m_historyIndex = -1;
    };
} // namespace enigma::core
