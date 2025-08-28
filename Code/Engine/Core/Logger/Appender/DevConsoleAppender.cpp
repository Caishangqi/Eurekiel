#include "DevConsoleAppender.hpp"
#include "../../Console/DevConsole.hpp"
#include <sstream>
#include <iomanip>

#include "Engine/Core/Console/ConsoleSubsystem.hpp"

// Forward declaration - g_theDevConsole will be defined externally
extern DevConsole* g_theDevConsole;

namespace enigma::core
{
    DevConsoleAppender::DevConsoleAppender()
    {
        // Constructor - nothing special needed
    }

    void DevConsoleAppender::Write(const LogMessage& message)
    {
        if (!IsEnabled()) {
            return;
        }

        DevConsole* devConsole = GetDevConsole();
        if (!devConsole) {
            return; // DevConsole not available
        }

        std::string formattedMessage = FormatLogMessage(message);
        Rgba8 color = GetColorForLevel(message.level);
        
        // Use DevConsole's AddLine method to add the log message
        devConsole->AddLine(color, formattedMessage);
    }

    bool DevConsoleAppender::IsEnabled() const
    {
        // Disable DevConsoleAppender when debugger is present to avoid duplicate output
        // (ConsoleAppender will handle IDE Console output directly)
#ifdef _WIN32
        if (IsDebuggerPresent()) {
            return false;  // Let ConsoleAppender handle IDE output
        }
#endif
        // Only enabled if base is enabled AND DevConsole is available
        return ILogAppender::IsEnabled() && GetDevConsole() != nullptr;
    }

    std::string DevConsoleAppender::FormatLogMessage(const LogMessage& message) const
    {
        std::stringstream ss;
        
        // Format: [HH:MM:SS] [LEVEL] [Category] Message
        // Note: DevConsole already tracks frame numbers, so we don't need to add them
        ss << "[" << message.GetFormattedTimestamp() << "] ";
        ss << "[" << std::setw(5) << std::left << LogLevelToString(message.level) << "] ";
        ss << "[" << message.category << "] ";
        ss << message.message;
        
        return ss.str();
    }

    Rgba8 DevConsoleAppender::GetColorForLevel(LogLevel level) const
    {
        // Use the same colors as defined in LogLevel.hpp
        return GetColorForLogLevel(level);
    }

    DevConsole* DevConsoleAppender::GetDevConsole() const
    {
        return g_theDevConsole;
    }
}