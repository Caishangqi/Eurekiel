#pragma once
#include "../SubsystemManager.hpp"
#include "CommandTypes.hpp"
#include "CommandParser.hpp"
#include "CommandHistory.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>

namespace enigma::core
{
    // ============================================================================
    // Command subsystem - independent command processing system
    // ============================================================================
    class CommandSubsystem : public EngineSubsystem
    {
    public:
        DECLARE_SUBSYSTEM(CommandSubsystem, "Command", 95) // High priority, before DevConsole

        CommandSubsystem();
        ~CommandSubsystem() override;

        // EngineSubsystem interface
        void Initialize() override;
        void Startup() override;
        void Shutdown() override;
        bool RequiresGameLoop() const override { return false; }
        bool RequiresInitialize() const override { return true; }

        // ========================================================================
        // Command registration
        // ========================================================================

        // Register a command with callback (supports Lambda)
        void RegisterCommand(
            const std::string& name,
            CommandCallback callback,
            const std::string& description = "",
            const std::string& usage = "");

        // Unregister a command
        void UnregisterCommand(const std::string& name);

        // Check if a command is registered
        bool IsCommandRegistered(const std::string& name) const;

        // ========================================================================
        // Command execution
        // ========================================================================

        // Execute a command from a command line string
        CommandResult Execute(const std::string& commandLine);

        // Execute a command with pre-parsed arguments
        CommandResult ExecuteWithArgs(const CommandArgs& args);

        // ========================================================================
        // History management
        // ========================================================================

        // Add command to history (called automatically by Execute)
        void AddToHistory(const std::string& command);

        // Get all command history
        const std::vector<std::string> GetHistory() const;

        // Get recent N history entries
        std::vector<std::string> GetRecentHistory(size_t count) const;

        // Clear command history
        void ClearHistory();

        // Navigate history (for console UI)
        std::string NavigateHistoryPrevious();
        std::string NavigateHistoryNext();
        void ResetHistoryNavigation();

        // ========================================================================
        // Auto-completion support
        // ========================================================================

        // Get command suggestions based on partial input
        std::vector<std::string> GetCommandSuggestions(const std::string& partial) const;

        // Get all matching commands (exact prefix match)
        std::vector<std::string> GetMatchingCommands(const std::string& prefix) const;

        // ========================================================================
        // Query interface
        // ========================================================================

        // Get all registered commands
        std::vector<CommandInfo> GetAllCommands() const;

        // Get command info by name
        std::optional<CommandInfo> GetCommandInfo(const std::string& name) const;

        // Get command count
        size_t GetCommandCount() const;

        // ========================================================================
        // Configuration
        // ========================================================================

        // Set maximum history size
        void SetMaxHistorySize(size_t maxSize);

        // Get maximum history size
        size_t GetMaxHistorySize() const;

    private:
        // Internal command registry entry
        struct CommandRegistry
        {
            CommandInfo info;
            // Future: Add metadata like category, tags, etc.

            CommandRegistry() = default;
            CommandRegistry(const CommandInfo& i) : info(i) {}
        };

        // Command storage
        std::unordered_map<std::string, CommandRegistry> m_commands;
        mutable std::mutex                               m_commandsMutex;

        // History manager
        CommandHistory         m_history;
        mutable std::mutex     m_historyMutex;

        // Parser
        std::unique_ptr<CommandParser> m_parser;

        // Helper methods
        void RegisterBuiltInCommands();
        CommandResult ExecuteHelp(const CommandArgs& args);
        CommandResult ExecuteHistory(const CommandArgs& args);
        CommandResult ExecuteClear(const CommandArgs& args);

        // String utility for case-insensitive comparison
        std::string ToLower(const std::string& str) const;
    };
}
