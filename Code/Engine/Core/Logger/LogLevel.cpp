#include "LogLevel.hpp"
#include <algorithm>
#include <cctype>
#include <string>

namespace enigma::core
{
    const char* LogLevelToString(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR_: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
        }
    }

    LogLevel StringToLogLevel(const std::string& str)
    {
        // Convert to uppercase for case-insensitive comparison
        std::string upperStr = str;
        std::transform(upperStr.begin(), upperStr.end(), upperStr.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

        if (upperStr == "TRACE") return LogLevel::TRACE;
        if (upperStr == "DEBUG") return LogLevel::DEBUG;
        if (upperStr == "INFO") return LogLevel::INFO;
        if (upperStr == "WARN") return LogLevel::WARNING;
        if (upperStr == "ERROR") return LogLevel::ERROR_;
        if (upperStr == "FATAL") return LogLevel::FATAL;

        // Default to INFO for unknown levels
        return LogLevel::INFO;
    }

    Rgba8 GetColorForLogLevel(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::TRACE: return Rgba8(128, 128, 128, 255); // Gray
        case LogLevel::DEBUG: return Rgba8(192, 192, 192, 255); // Light Gray  
        case LogLevel::INFO: return Rgba8(255, 255, 255, 255); // White
        case LogLevel::WARNING: return Rgba8(255, 255, 0, 255); // Yellow
        case LogLevel::ERROR_: return Rgba8(255, 128, 0, 255); // Orange
        case LogLevel::FATAL: return Rgba8(255, 0, 0, 255); // Red
        default: return Rgba8(255, 255, 255, 255); // White
        }
    }
}
