#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Engine/Core/SubsystemManager.hpp"
#include "Engine/Core/Event/EventSubsystem.hpp"
#include "Engine/Core/Event/StringEventBus.hpp"
#include "Engine/Core/Event/MulticastDelegate.hpp"
#include "Engine/Core/Logger/LogLevel.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "ConsoleConfig.hpp"
#include "ConsoleMessage.hpp"
#include "ExternalConsole.hpp"

// Import EventArgs type alias for legacy compatibility
using enigma::event::EventArgs;
using enigma::event::MulticastDelegate;
using enigma::event::DelegateHandle;

namespace enigma::core
{
    // Forward declaration
    class ImguiConsole;

    // Console subsystem managing ExternalConsole and ImguiConsole backends.
    // Command execution is the sole authority of this class (DevConsole is deprecated).
    // ImGui rendering is driven from Update() (same pattern as MessageLogSubsystem).
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
        void Update(float deltaTime) override;
        bool RequiresGameLoop() const override { return true; }
        bool RequiresInitialize() const override { return true; }

        // Output interface (broadcasts to all backends via MulticastDelegate)
        void WriteLine(const std::string& text, LogLevel level = LogLevel::INFO);
        void WriteLineColored(const std::string& text, const Rgba8& color);
        void WriteFormatted(LogLevel level, const char* format, ...);

        // Command execution (ConsoleSubsystem is the sole authority)
        void Execute(const std::string& command, bool echoCommand = true);

        // Command registration for autocomplete
        void RegisterCommand(const std::string& name, const std::string& description = "");
        const std::vector<std::string>& GetRegisteredCommands() const;
        const std::string& GetCommandDescription(const std::string& name) const;

        // ImGui Console control
        void ToggleImguiConsole();
        bool IsImguiConsoleVisible() const;

        // External Console control
        void SetVisible(bool visible);
        bool IsVisible() const;

        // Status
        bool IsInitialized() const { return m_initialized; }
        bool IsExternalConsoleActive() const;

        // --- MulticastDelegate events (replacing old static Event_*) ---
        MulticastDelegate<const ConsoleMessage&> OnConsoleOutput;
        MulticastDelegate<const std::string&>    OnCommandExecuted;

    private:
        // Delegate binding (replaces old RegisterEventHandlers/UnregisterEventHandlers)
        void BindDelegates();
        void UnbindDelegates();

        // Member function event handlers (replacing old static Event_* functions)
        void OnKeyPressed(unsigned char keyCode);
        void OnCharInput(unsigned char character);

        // Initialization helpers
        void CreateExternalConsole();

        // External console input processing
        void ProcessCharInput(unsigned char character);
        void HandleBackspace();
        void HandleEnter();
        void HandleUpArrow();
        void HandleDownArrow();
        void HandlePaste();
        void HandleEscape();
        void HandleArrowKeys(unsigned char keyCode);
        void UpdateInputDisplay();

        // Configuration
        ConsoleConfig m_config;
        bool          m_initialized        = false;
        bool          m_isVisible          = false;
        bool          m_isExecutingCommand = false; // True during command execution (for console message routing)

        // ImGui Console backend
        std::unique_ptr<ImguiConsole> m_imguiConsole;

        // External Console backend
        std::unique_ptr<ExternalConsole> m_externalConsole;

        // Delegate handles for cleanup
        DelegateHandle m_keyPressedHandle = 0;
        DelegateHandle m_charInputHandle  = 0;

        // Registered commands for autocomplete
        std::vector<std::string> m_registeredCommands;
        std::unordered_map<std::string, std::string> m_commandDescriptions;

        // External console input state
        std::string m_currentInput;
        int         m_cursorPosition = 0;

        // Command history (shared between backends)
        std::vector<std::string> m_commandHistory;
        int                      m_historyIndex = -1;

        // DockBuilder layout management (Console docked below MessageLogUI)
        void         SetupDockLayout();
        void         CycleTerminalMode();
        bool         m_dockLayoutInitialized = false;
        unsigned int m_dockTopId    = 0; // MessageLog node
        unsigned int m_dockBottomId = 0; // Console node
    };
} // namespace enigma::core
