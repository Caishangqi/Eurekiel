#pragma once
#include <string>
#include "../Rgba8.hpp"

namespace enigma::core
{
    enum class LogLevel : int {
        TRACE = 0,   // Detailed trace information
        DEBUG = 1,   // Debug information  
        INFO = 2,    // General information
        WARNING = 3, // Warning messages (avoids Windows WARN macro conflict)
        ERROR_ = 4,  // Error messages (avoids Windows ERROR macro conflict)
        FATAL = 5    // Fatal errors
    };

    // Utility functions for LogLevel
    const char* LogLevelToString(LogLevel level);
    LogLevel StringToLogLevel(const std::string& str);
    Rgba8 GetColorForLogLevel(LogLevel level); // Colors for DevConsole rendering

    // Comparison operators for filtering
    constexpr bool operator>=(LogLevel lhs, LogLevel rhs) {
        return static_cast<int>(lhs) >= static_cast<int>(rhs);
    }

    constexpr bool operator<=(LogLevel lhs, LogLevel rhs) {
        return static_cast<int>(lhs) <= static_cast<int>(rhs);
    }

    constexpr bool operator>(LogLevel lhs, LogLevel rhs) {
        return static_cast<int>(lhs) > static_cast<int>(rhs);
    }

    constexpr bool operator<(LogLevel lhs, LogLevel rhs) {
        return static_cast<int>(lhs) < static_cast<int>(rhs);
    }
}