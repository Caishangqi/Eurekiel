#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <functional>

namespace enigma::core
{
    // Forward declaration
    struct CommandArgs;

    // ============================================================================
    // Command argument value types
    // ============================================================================
    using CommandValue = std::variant<std::string, int, float, bool>;

    // ============================================================================
    // Command arguments structure
    // ============================================================================
    struct CommandArgs
    {
        std::string                                  commandName;     // Name of the command
        std::vector<CommandValue>                    positionalArgs;  // Positional arguments
        std::unordered_map<std::string, CommandValue> namedArgs;      // Named arguments (--key=value)

        // Helper methods for argument access
        size_t GetPositionalCount() const { return positionalArgs.size(); }

        bool HasNamedArg(const std::string& key) const
        {
            return namedArgs.find(key) != namedArgs.end();
        }

        // Get positional argument with type conversion
        template<typename T>
        T GetPositional(size_t index, const T& defaultValue = T()) const
        {
            if (index >= positionalArgs.size())
                return defaultValue;

            const auto& value = positionalArgs[index];

            if constexpr (std::is_same_v<T, std::string>)
            {
                if (std::holds_alternative<std::string>(value))
                    return std::get<std::string>(value);
                // Convert other types to string if needed
                if (std::holds_alternative<int>(value))
                    return std::to_string(std::get<int>(value));
                if (std::holds_alternative<float>(value))
                    return std::to_string(std::get<float>(value));
                if (std::holds_alternative<bool>(value))
                    return std::get<bool>(value) ? "true" : "false";
            }
            else if constexpr (std::is_same_v<T, int>)
            {
                if (std::holds_alternative<int>(value))
                    return std::get<int>(value);
                if (std::holds_alternative<std::string>(value))
                {
                    try { return std::stoi(std::get<std::string>(value)); }
                    catch (...) { return defaultValue; }
                }
            }
            else if constexpr (std::is_same_v<T, float>)
            {
                if (std::holds_alternative<float>(value))
                    return std::get<float>(value);
                if (std::holds_alternative<int>(value))
                    return static_cast<float>(std::get<int>(value));
                if (std::holds_alternative<std::string>(value))
                {
                    try { return std::stof(std::get<std::string>(value)); }
                    catch (...) { return defaultValue; }
                }
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                if (std::holds_alternative<bool>(value))
                    return std::get<bool>(value);
                if (std::holds_alternative<std::string>(value))
                {
                    const auto& str = std::get<std::string>(value);
                    return str == "true" || str == "1" || str == "yes";
                }
                if (std::holds_alternative<int>(value))
                    return std::get<int>(value) != 0;
            }

            return defaultValue;
        }

        // Get named argument with type conversion
        template<typename T>
        T GetNamed(const std::string& key, const T& defaultValue = T()) const
        {
            auto it = namedArgs.find(key);
            if (it == namedArgs.end())
                return defaultValue;

            const auto& value = it->second;

            if constexpr (std::is_same_v<T, std::string>)
            {
                if (std::holds_alternative<std::string>(value))
                    return std::get<std::string>(value);
                if (std::holds_alternative<int>(value))
                    return std::to_string(std::get<int>(value));
                if (std::holds_alternative<float>(value))
                    return std::to_string(std::get<float>(value));
                if (std::holds_alternative<bool>(value))
                    return std::get<bool>(value) ? "true" : "false";
            }
            else if constexpr (std::is_same_v<T, int>)
            {
                if (std::holds_alternative<int>(value))
                    return std::get<int>(value);
                if (std::holds_alternative<std::string>(value))
                {
                    try { return std::stoi(std::get<std::string>(value)); }
                    catch (...) { return defaultValue; }
                }
            }
            else if constexpr (std::is_same_v<T, float>)
            {
                if (std::holds_alternative<float>(value))
                    return std::get<float>(value);
                if (std::holds_alternative<int>(value))
                    return static_cast<float>(std::get<int>(value));
                if (std::holds_alternative<std::string>(value))
                {
                    try { return std::stof(std::get<std::string>(value)); }
                    catch (...) { return defaultValue; }
                }
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                if (std::holds_alternative<bool>(value))
                    return std::get<bool>(value);
                if (std::holds_alternative<std::string>(value))
                {
                    const auto& str = std::get<std::string>(value);
                    return str == "true" || str == "1" || str == "yes";
                }
                if (std::holds_alternative<int>(value))
                    return std::get<int>(value) != 0;
            }

            return defaultValue;
        }
    };

    // ============================================================================
    // Command execution result
    // ============================================================================
    struct CommandResult
    {
        enum class Status
        {
            Success,
            Warning,
            Error,
            NotFound,
            InvalidArgs
        };

        Status      status = Status::Success;
        std::string message;
        std::string details; // Additional information for debugging

        // Factory methods for convenience
        static CommandResult Success(const std::string& msg = "")
        {
            return CommandResult{Status::Success, msg, ""};
        }

        static CommandResult Warning(const std::string& msg)
        {
            return CommandResult{Status::Warning, msg, ""};
        }

        static CommandResult Error(const std::string& msg, const std::string& details = "")
        {
            return CommandResult{Status::Error, msg, details};
        }

        static CommandResult NotFound(const std::string& commandName)
        {
            return CommandResult{Status::NotFound,
                               "Command not found: " + commandName,
                               "Use 'help' to see available commands"};
        }

        static CommandResult InvalidArgs(const std::string& msg)
        {
            return CommandResult{Status::InvalidArgs, msg, ""};
        }

        // Check status
        bool IsSuccess() const { return status == Status::Success; }
        bool IsError() const { return status == Status::Error || status == Status::NotFound || status == Status::InvalidArgs; }
        bool IsWarning() const { return status == Status::Warning; }
    };

    // ============================================================================
    // Command callback function type
    // ============================================================================
    using CommandCallback = std::function<CommandResult(const CommandArgs&)>;

    // ============================================================================
    // Command metadata information
    // ============================================================================
    struct CommandInfo
    {
        std::string       name;        // Command name
        std::string       description; // Brief description
        std::string       usage;       // Usage syntax (e.g., "command <arg1> [arg2]")
        CommandCallback   callback;    // Execution callback

        CommandInfo() = default;
        CommandInfo(const std::string& n, CommandCallback cb,
                   const std::string& desc = "", const std::string& u = "")
            : name(n), description(desc), usage(u), callback(std::move(cb))
        {}
    };
}
