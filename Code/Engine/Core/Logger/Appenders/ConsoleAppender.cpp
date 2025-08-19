#include "ConsoleAppender.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

// Include our smart console system
#include "Engine/Core/Console/ConsoleSubsystem.hpp"
#include "Engine/Core/EngineCommon.hpp"

namespace enigma::core
{
    ConsoleAppender::ConsoleAppender(bool enableColors)
        : m_enableColors(enableColors)
    {
        // On Windows, try to enable ANSI color support
#ifdef _WIN32
        if (m_enableColors) {
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hOut != INVALID_HANDLE_VALUE) {
                DWORD dwMode = 0;
                if (GetConsoleMode(hOut, &dwMode)) {
                    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                    SetConsoleMode(hOut, dwMode);
                }
            }
        }
#endif
    }

    void ConsoleAppender::Write(const LogMessage& message)
    {
        if (!IsEnabled()) {
            return;
        }

        std::lock_guard<std::mutex> lock(m_writeMutex);
        
        std::string formattedMessage = FormatLogMessage(message);
        
        // Try to use our smart console system first
        if (::g_theConsole && ::g_theConsole->IsInitialized()) {
            // Use our intelligent output system
            if (m_enableColors) {
                // Convert LogLevel to color
                Rgba8 color = Rgba8::WHITE;
                switch (message.level) {
                    case LogLevel::ERROR_: color = Rgba8::RED; break;
                    case LogLevel::WARNING: color = Rgba8::YELLOW; break;
                    case LogLevel::INFO: color = Rgba8::WHITE; break;
                    case LogLevel::DEBUG: color = Rgba8::GRAY; break;
                    case LogLevel::TRACE: color = Rgba8::GRAY; break;
                }
                ::g_theConsole->WriteLineColored(formattedMessage, color);
            } else {
                ::g_theConsole->WriteLine(formattedMessage, message.level);
            }
        } else {
            // Fallback to standard output with intelligent detection
#ifdef _WIN32
            // In Debug mode with debugger, use OutputDebugString for IDE Console
            if (IsDebuggerPresent()) {
                OutputDebugStringA((formattedMessage + "\r\n").c_str());
            } else {
                // Use standard cout
                if (m_enableColors && SupportsAnsiColors()) {
                    std::cout << GetAnsiColorCode(message.level) 
                              << formattedMessage 
                              << GetAnsiResetCode() 
                              << std::endl;
                } else {
                    std::cout << formattedMessage << std::endl;
                }
            }
#else
            // Non-Windows platforms use standard cout
            if (m_enableColors && SupportsAnsiColors()) {
                std::cout << GetAnsiColorCode(message.level) 
                          << formattedMessage 
                          << GetAnsiResetCode() 
                          << std::endl;
            } else {
                std::cout << formattedMessage << std::endl;
            }
#endif
        }
    }

    void ConsoleAppender::Flush()
    {
        std::lock_guard<std::mutex> lock(m_writeMutex);
        std::cout.flush();
    }

    std::string ConsoleAppender::FormatLogMessage(const LogMessage& message) const
    {
        std::stringstream ss;
        
        // Format: [HH:MM:SS.mmm] [LEVEL] [Category] Message (Frame: 123)
        ss << "[" << message.GetFormattedTimestamp() << "] ";
        ss << "[" << std::setw(5) << std::left << LogLevelToString(message.level) << "] ";
        ss << "[" << message.category << "] ";
        ss << message.message;
        
        if (message.frameNumber > 0) {
            ss << " (Frame: " << message.frameNumber << ")";
        }
        
        return ss.str();
    }

    std::string ConsoleAppender::GetAnsiColorCode(LogLevel level) const
    {
        switch (level) {
        case LogLevel::TRACE:   return "\033[90m";  // Bright Black (Gray)
        case LogLevel::DEBUG:   return "\033[37m";  // White
        case LogLevel::INFO:    return "\033[0m";   // Default
        case LogLevel::WARNING: return "\033[33m";  // Yellow
        case LogLevel::ERROR_:  return "\033[31m";  // Red
        case LogLevel::FATAL:   return "\033[91m";  // Bright Red
        default:                return "\033[0m";   // Default
        }
    }

    std::string ConsoleAppender::GetAnsiResetCode() const
    {
        return "\033[0m";
    }

    bool ConsoleAppender::SupportsAnsiColors() const
    {
#ifdef _WIN32
        // Check if we're running in a terminal that supports ANSI colors
        // This is a simple check - in reality, we might want more sophisticated detection
        return _isatty(_fileno(stdout));
#else
        // On Unix-like systems, assume ANSI support if stdout is a terminal
        return isatty(fileno(stdout));
#endif
    }
}