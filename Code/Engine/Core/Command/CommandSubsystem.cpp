#include "CommandSubsystem.hpp"
#include <algorithm>
#include <sstream>
#include <cctype>

namespace enigma::core
{
    // ============================================================================
    // Construction / Destruction
    // ============================================================================

    CommandSubsystem::CommandSubsystem()
        : m_history(1000)
        , m_parser(std::make_unique<CommandParser>())
    {
    }

    CommandSubsystem::~CommandSubsystem()
    {
    }

    // ============================================================================
    // EngineSubsystem interface
    // ============================================================================

    void CommandSubsystem::Initialize()
    {
        // Early initialization - register built-in commands
        RegisterBuiltInCommands();
    }

    void CommandSubsystem::Startup()
    {
        // Main startup - nothing special needed for now
    }

    void CommandSubsystem::Shutdown()
    {
        // Clear all registered commands
        std::lock_guard<std::mutex> lock(m_commandsMutex);
        m_commands.clear();
    }

    // ============================================================================
    // Command registration
    // ============================================================================

    void CommandSubsystem::RegisterCommand(
        const std::string& name,
        CommandCallback callback,
        const std::string& description,
        const std::string& usage)
    {
        std::lock_guard<std::mutex> lock(m_commandsMutex);

        CommandInfo info(name, callback, description, usage);
        m_commands[name] = CommandRegistry(info);
    }

    void CommandSubsystem::UnregisterCommand(const std::string& name)
    {
        std::lock_guard<std::mutex> lock(m_commandsMutex);
        m_commands.erase(name);
    }

    bool CommandSubsystem::IsCommandRegistered(const std::string& name) const
    {
        std::lock_guard<std::mutex> lock(m_commandsMutex);
        return m_commands.find(name) != m_commands.end();
    }

    // ============================================================================
    // Command execution
    // ============================================================================

    CommandResult CommandSubsystem::Execute(const std::string& commandLine)
    {
        // Parse the command line
        auto parseResult = m_parser->Parse(commandLine);

        if (!parseResult)
        {
            return CommandResult::Error(
                "Failed to parse command",
                m_parser->GetLastError());
        }

        // Add to history (before execution)
        AddToHistory(commandLine);

        // Execute with parsed arguments
        return ExecuteWithArgs(*parseResult);
    }

    CommandResult CommandSubsystem::ExecuteWithArgs(const CommandArgs& args)
    {
        // Find the command
        std::lock_guard<std::mutex> lock(m_commandsMutex);

        auto it = m_commands.find(args.commandName);
        if (it == m_commands.end())
        {
            return CommandResult::NotFound(args.commandName);
        }

        // Execute the callback
        try
        {
            return it->second.info.callback(args);
        }
        catch (const std::exception& e)
        {
            return CommandResult::Error(
                "Command execution failed",
                std::string("Exception: ") + e.what());
        }
        catch (...)
        {
            return CommandResult::Error(
                "Command execution failed",
                "Unknown exception");
        }
    }

    // ============================================================================
    // History management
    // ============================================================================

    void CommandSubsystem::AddToHistory(const std::string& command)
    {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        m_history.Add(command);
    }

    const std::vector<std::string> CommandSubsystem::GetHistory() const
    {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        return m_history.GetAll();
    }

    std::vector<std::string> CommandSubsystem::GetRecentHistory(size_t count) const
    {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        return m_history.GetRecent(count);
    }

    void CommandSubsystem::ClearHistory()
    {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        m_history.Clear();
    }

    std::string CommandSubsystem::NavigateHistoryPrevious()
    {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        return m_history.NavigatePrevious();
    }

    std::string CommandSubsystem::NavigateHistoryNext()
    {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        return m_history.NavigateNext();
    }

    void CommandSubsystem::ResetHistoryNavigation()
    {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        m_history.ResetNavigation();
    }

    // ============================================================================
    // Auto-completion support
    // ============================================================================

    std::vector<std::string> CommandSubsystem::GetCommandSuggestions(const std::string& partial) const
    {
        std::lock_guard<std::mutex> lock(m_commandsMutex);

        std::vector<std::string> suggestions;
        std::string partialLower = ToLower(partial);

        for (const auto& [name, registry] : m_commands)
        {
            std::string nameLower = ToLower(name);

            // Check if command starts with the partial input
            if (nameLower.find(partialLower) == 0)
            {
                suggestions.push_back(name);
            }
        }

        // Sort suggestions alphabetically
        std::sort(suggestions.begin(), suggestions.end());

        return suggestions;
    }

    std::vector<std::string> CommandSubsystem::GetMatchingCommands(const std::string& prefix) const
    {
        return GetCommandSuggestions(prefix);
    }

    // ============================================================================
    // Query interface
    // ============================================================================

    std::vector<CommandInfo> CommandSubsystem::GetAllCommands() const
    {
        std::lock_guard<std::mutex> lock(m_commandsMutex);

        std::vector<CommandInfo> commands;
        commands.reserve(m_commands.size());

        for (const auto& [name, registry] : m_commands)
        {
            commands.push_back(registry.info);
        }

        // Sort by name
        std::sort(commands.begin(), commands.end(),
                 [](const CommandInfo& a, const CommandInfo& b) {
                     return a.name < b.name;
                 });

        return commands;
    }

    std::optional<CommandInfo> CommandSubsystem::GetCommandInfo(const std::string& name) const
    {
        std::lock_guard<std::mutex> lock(m_commandsMutex);

        auto it = m_commands.find(name);
        if (it == m_commands.end())
            return std::nullopt;

        return it->second.info;
    }

    size_t CommandSubsystem::GetCommandCount() const
    {
        std::lock_guard<std::mutex> lock(m_commandsMutex);
        return m_commands.size();
    }

    // ============================================================================
    // Configuration
    // ============================================================================

    void CommandSubsystem::SetMaxHistorySize(size_t maxSize)
    {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        m_history.SetMaxSize(maxSize);
    }

    size_t CommandSubsystem::GetMaxHistorySize() const
    {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        return m_history.GetMaxSize();
    }

    // ============================================================================
    // Built-in commands
    // ============================================================================

    void CommandSubsystem::RegisterBuiltInCommands()
    {
        // Help command
        RegisterCommand(
            "help",
            [this](const CommandArgs& args) { return ExecuteHelp(args); },
            "Display help information about commands",
            "help [command_name]");

        // History command
        RegisterCommand(
            "history",
            [this](const CommandArgs& args) { return ExecuteHistory(args); },
            "Display command history",
            "history [count]");

        // Clear history command
        RegisterCommand(
            "clear_history",
            [this](const CommandArgs& args) { return ExecuteClear(args); },
            "Clear command history",
            "clear_history");
    }

    CommandResult CommandSubsystem::ExecuteHelp(const CommandArgs& args)
    {
        std::stringstream ss;

        // If a specific command is requested
        if (args.GetPositionalCount() > 0)
        {
            std::string commandName = args.GetPositional<std::string>(0);
            auto info = GetCommandInfo(commandName);

            if (!info)
            {
                return CommandResult::Error("Command not found: " + commandName);
            }

            ss << "Command: " << info->name << "\n";
            if (!info->description.empty())
                ss << "Description: " << info->description << "\n";
            if (!info->usage.empty())
                ss << "Usage: " << info->usage << "\n";

            return CommandResult::Success(ss.str());
        }

        // List all commands
        auto allCommands = GetAllCommands();

        ss << "Available commands (" << allCommands.size() << "):\n\n";

        for (const auto& cmd : allCommands)
        {
            ss << "  " << cmd.name;

            if (!cmd.description.empty())
                ss << " - " << cmd.description;

            ss << "\n";
        }

        ss << "\nUse 'help <command_name>' for detailed information.";

        return CommandResult::Success(ss.str());
    }

    CommandResult CommandSubsystem::ExecuteHistory(const CommandArgs& args)
    {
        std::stringstream ss;

        // Get history count from argument
        int count = args.GetPositional<int>(0, 20); // Default to last 20 entries

        auto history = count > 0 ? GetRecentHistory(count) : GetHistory();

        if (history.empty())
        {
            return CommandResult::Success("No command history.");
        }

        ss << "Command history (showing " << history.size() << " entries):\n\n";

        for (size_t i = 0; i < history.size(); ++i)
        {
            ss << "  " << (i + 1) << ": " << history[i] << "\n";
        }

        return CommandResult::Success(ss.str());
    }

    CommandResult CommandSubsystem::ExecuteClear(const CommandArgs& args)
    {
        (void)args; // Unused parameter
        ClearHistory();
        return CommandResult::Success("Command history cleared.");
    }

    // ============================================================================
    // Utility functions
    // ============================================================================

    std::string CommandSubsystem::ToLower(const std::string& str) const
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        return result;
    }
}
