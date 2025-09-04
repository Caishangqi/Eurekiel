#pragma once
#include "LogLevel.hpp"
#include <string>
#include <chrono>
#include <thread>

namespace enigma::core
{
    struct LogMessage
    {
        LogLevel                              level;
        std::string                           category;
        std::string                           message;
        std::chrono::system_clock::time_point timestamp;
        std::thread::id                       threadId;
        int                                   frameNumber; // Frame number from g_theDevConsole

        LogMessage(LogLevel lvl, const std::string& cat, const std::string& msg);
        LogMessage(LogLevel lvl, const std::string& cat, const std::string& msg, int frame);

        // Utility methods
        std::string GetFormattedTimestamp() const;
        std::string GetThreadIdString() const;
    };
}
