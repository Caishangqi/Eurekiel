#pragma once
#include "LoggerConfig.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace enigma::core
{
    /**
     * @class LogFileManager
     * @brief Manages log file rotation and directory structure
     * 
     * Handles Minecraft-style log management:
     * - latest.log for current session
     * - Date-based naming for archived logs
     * - Automatic cleanup of old logs
     */
    class LogFileManager
    {
    public:
        LogFileManager(const LoggerConfig& config);
        ~LogFileManager() = default;
        
        // Initialization
        bool Initialize();
        void Shutdown();
        
        // File management
        std::filesystem::path GetCurrentLogPath() const { return m_currentLogPath; }
        std::filesystem::path GetLogDirectory() const { return m_config.logDirectory; }
        
        // Rotation management
        bool RotateLogsIfNeeded();
        void ForceRotation();
        
        // Directory management
        bool CreateLogDirectory();
        void CleanupOldLogs();
        
        // Utility
        std::string GetCurrentDateString() const;
        std::string GetCurrentTimeString() const;
        std::vector<std::filesystem::path> GetExistingLogFiles() const;
        
    private:
        const LoggerConfig& m_config;
        std::filesystem::path m_currentLogPath;
        std::filesystem::path m_logDirectory;
        
        // Internal helpers
        bool FileExists(const std::filesystem::path& path) const;
        bool MoveFile(const std::filesystem::path& from, const std::filesystem::path& to);
        std::vector<std::filesystem::path> GetArchivedLogFiles() const;
        void SortLogFilesByDate(std::vector<std::filesystem::path>& files) const;
        std::filesystem::path ResolveLogDirectory(const std::filesystem::path& configPath) const;
    };
}