#pragma once
#include "LogLevel.hpp"
#include <string>
#include <filesystem>
#include <unordered_map>

namespace enigma::core
{
    /**
     * @struct LoggerConfig
     * @brief Logger subsystem configuration structure
     * 
     * Contains all configuration parameters for the logging system
     */
    struct LoggerConfig
    {
        // Basic logging configuration
        LogLevel globalLogLevel = LogLevel::INFO;
        std::unordered_map<std::string, LogLevel> categoryLogLevels;
        
        // File logging configuration
        bool enableFileLogging = true;
        std::filesystem::path logDirectory = ".enigma/logs";
        std::string latestLogFileName = "latest.log";
        bool enableLogRotation = true;
        size_t maxLogFiles = 10; // Keep up to 10 historical log files
        
        // Console logging configuration
        bool enableConsoleLogging = true;
        bool enableConsoleColors = true;
        
        // DevConsole logging configuration  
        bool enableDevConsoleLogging = true;
        
        // Log format configuration
        bool includeTimestamp = true;
        bool includeThreadId = false;
        bool includeFrameNumber = true;
        bool includeCategory = true;
        
        // Advanced options
        bool flushImmediately = false; // Flush after every log (performance impact)
        bool enableAsyncLogging = false; // Future feature for Phase 3.4
        size_t logBufferSize = 1024 * 1024; // 1MB buffer for async logging
        
        // Debug configuration
        bool logToStdout = false; // Also log to stdout for debugging
        bool enableLogLevelFiltering = true;
        
        // Constructor with defaults matching module.yml
        LoggerConfig() = default;
        
        // Utility methods
        void SetCategoryLogLevel(const std::string& category, LogLevel level) {
            categoryLogLevels[category] = level;
        }
        
        LogLevel GetCategoryLogLevel(const std::string& category) const {
            auto it = categoryLogLevels.find(category);
            return (it != categoryLogLevels.end()) ? it->second : globalLogLevel;
        }
        
        // File path helpers
        std::filesystem::path GetLatestLogPath() const {
            return logDirectory / latestLogFileName;
        }
        
        std::filesystem::path GetArchivedLogPath(const std::string& dateSuffix) const {
            std::string archivedName = std::filesystem::path(latestLogFileName).stem().string() 
                                     + "_" + dateSuffix + ".log";
            return logDirectory / archivedName;
        }
        
        // Validation
        bool IsValid() const {
            return !logDirectory.empty() && !latestLogFileName.empty();
        }
    };
}