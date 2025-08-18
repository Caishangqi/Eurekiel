#include "LogMessage.hpp"
#include <sstream>
#include <iomanip>

namespace enigma::core
{
    LogMessage::LogMessage(LogLevel lvl, const std::string& cat, const std::string& msg)
        : level(lvl)
        , category(cat)
        , message(msg)
        , timestamp(std::chrono::system_clock::now())
        , threadId(std::this_thread::get_id())
        , frameNumber(0) // Will be set by LoggerSubsystem from DevConsole
    {
    }

    LogMessage::LogMessage(LogLevel lvl, const std::string& cat, const std::string& msg, int frame)
        : level(lvl)
        , category(cat)
        , message(msg)
        , timestamp(std::chrono::system_clock::now())
        , threadId(std::this_thread::get_id())
        , frameNumber(frame)
    {
    }

    std::string LogMessage::GetFormattedTimestamp() const
    {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()) % 1000;
        
        std::stringstream ss;
#ifdef _WIN32
        std::tm timeinfo;
        localtime_s(&timeinfo, &time_t);
        ss << std::put_time(&timeinfo, "%H:%M:%S");
#else
        ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
#endif
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

    std::string LogMessage::GetThreadIdString() const
    {
        std::stringstream ss;
        ss << threadId;
        return ss.str();
    }
}