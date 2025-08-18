#pragma once
#include "../SubsystemManager.hpp"
#include "LogLevel.hpp"
#include "LogMessage.hpp"
#include "Appenders/ILogAppender.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>

namespace enigma::core
{
    class LoggerSubsystem : public EngineSubsystem {
    public:
        DECLARE_SUBSYSTEM(LoggerSubsystem, "Logger", 100)  // Highest priority
        
        // EngineSubsystem interface
        void Startup() override;
        void Shutdown() override;
        bool RequiresGameLoop() const override { return false; }
        
        // Main logging interface
        void Log(LogLevel level, const std::string& category, const std::string& message);
        void LogFormatted(LogLevel level, const std::string& category, const char* format, ...);
        
        // Convenience interface
        void LogTrace(const std::string& category, const std::string& message);
        void LogDebug(const std::string& category, const std::string& message);
        void LogInfo(const std::string& category, const std::string& message);
        void LogWarn(const std::string& category, const std::string& message);
        void LogError(const std::string& category, const std::string& message);
        void LogFatal(const std::string& category, const std::string& message);
        
        // Appender management
        void AddAppender(std::unique_ptr<ILogAppender> appender);
        void RemoveAllAppenders();
        
        // Configuration interface
        void SetGlobalLogLevel(LogLevel level);
        void SetCategoryLogLevel(const std::string& category, LogLevel level);
        LogLevel GetEffectiveLogLevel(const std::string& category) const;
        
        // Thread safety
        void Flush();  // Force process all pending messages

    private:
        // Phase 3.1: Synchronous processing
        void ProcessMessageSync(const LogMessage& message);
        
        // Phase 3.4: Asynchronous processing (future implementation)
        void ProcessMessageAsync(const LogMessage& message);
        void WorkerThreadFunction();
        
        // Check if message should be logged
        bool ShouldLogMessage(LogLevel level, const std::string& category) const;
        
        // Get current frame number (from DevConsole)
        int GetCurrentFrameNumber() const;
        
        // Configuration
        LogLevel m_globalLogLevel = LogLevel::INFO;
        std::unordered_map<std::string, LogLevel> m_categoryLogLevels;
        mutable std::mutex m_configMutex;
        
        // Appenders
        std::vector<std::unique_ptr<ILogAppender>> m_appenders;
        mutable std::mutex m_appendersMutex;
        
        // Phase 3.4: Async queue (to be added later)
        std::queue<LogMessage> m_messageQueue;
        std::mutex m_queueMutex;
        std::condition_variable m_queueCondition;
        std::thread m_workerThread;
        std::atomic<bool> m_shouldStop{false};
        std::atomic<bool> m_asyncMode{false};  // Switch sync/async mode
    };
}