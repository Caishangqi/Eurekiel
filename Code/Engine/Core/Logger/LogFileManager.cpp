#include "LogFileManager.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace enigma::core
{
    LogFileManager::LogFileManager(const LoggerConfig& config)
        : m_config(config)
    {
        try {
            m_logDirectory = ResolveLogDirectory(config.logDirectory);
            
            // Ensure we have a valid log file name
            std::string logFileName = config.latestLogFileName.empty() ? "latest.log" : config.latestLogFileName;
            m_currentLogPath = m_logDirectory / logFileName;
        } catch (const std::exception& e) {
            std::cerr << "Error initializing LogFileManager: " << e.what() << std::endl;
            // Fall back to safe defaults
            m_logDirectory = std::filesystem::current_path() / "logs";
            m_currentLogPath = m_logDirectory / "latest.log";
        }
    }

    bool LogFileManager::Initialize()
    {
        // Create log directory if it doesn't exist
        if (!CreateLogDirectory()) {
            std::cerr << "Failed to create log directory: " << m_logDirectory << std::endl;
            return false;
        }
        
        // Rotate existing logs if needed
        if (m_config.enableLogRotation) {
            RotateLogsIfNeeded();
        }
        
        return true;
    }

    void LogFileManager::Shutdown()
    {
        // Nothing special needed for shutdown
    }

    bool LogFileManager::CreateLogDirectory()
    {
        try {
            if (!std::filesystem::exists(m_logDirectory)) {
                return std::filesystem::create_directories(m_logDirectory);
            }
            return true;
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Filesystem error creating log directory: " << e.what() << std::endl;
            return false;
        }
    }

    bool LogFileManager::RotateLogsIfNeeded()
    {
        // If latest.log exists, move it to a dated archive
        if (FileExists(m_currentLogPath)) {
            std::string dateString = GetCurrentDateString();
            std::filesystem::path archivedPath = m_config.GetArchivedLogPath(dateString);
            
            // If archived file already exists, add time to make it unique
            if (FileExists(archivedPath)) {
                std::string timeString = GetCurrentTimeString();
                archivedPath = m_config.GetArchivedLogPath(dateString + "_" + timeString);
            }
            
            if (MoveFile(m_currentLogPath, archivedPath)) {
                // Clean up old logs after rotation
                CleanupOldLogs();
                return true;
            }
            return false;
        }
        
        return true; // No rotation needed
    }

    void LogFileManager::ForceRotation()
    {
        RotateLogsIfNeeded();
    }

    void LogFileManager::CleanupOldLogs()
    {
        if (m_config.maxLogFiles == 0) {
            return; // No cleanup if maxLogFiles is 0 (unlimited)
        }
        
        auto archivedLogs = GetArchivedLogFiles();
        if (archivedLogs.size() <= m_config.maxLogFiles) {
            return; // Within limit
        }
        
        // Sort by date (oldest first)
        SortLogFilesByDate(archivedLogs);
        
        // Remove oldest files
        size_t filesToRemove = archivedLogs.size() - m_config.maxLogFiles;
        for (size_t i = 0; i < filesToRemove; ++i) {
            try {
                std::filesystem::remove(archivedLogs[i]);
            } catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "Failed to remove old log file " << archivedLogs[i] << ": " << e.what() << std::endl;
            }
        }
    }

    std::string LogFileManager::GetCurrentDateString() const
    {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
#ifdef _WIN32
        std::tm timeinfo;
        localtime_s(&timeinfo, &time_t);
        ss << std::put_time(&timeinfo, "%Y-%m-%d");
#else
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
#endif
        return ss.str();
    }

    std::string LogFileManager::GetCurrentTimeString() const
    {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
#ifdef _WIN32
        std::tm timeinfo;
        localtime_s(&timeinfo, &time_t);
        ss << std::put_time(&timeinfo, "%H-%M-%S");
#else
        ss << std::put_time(std::localtime(&time_t), "%H-%M-%S");
#endif
        return ss.str();
    }

    std::vector<std::filesystem::path> LogFileManager::GetExistingLogFiles() const
    {
        std::vector<std::filesystem::path> logFiles;
        
        try {
            if (std::filesystem::exists(m_logDirectory)) {
                for (const auto& entry : std::filesystem::directory_iterator(m_logDirectory)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".log") {
                        logFiles.push_back(entry.path());
                    }
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Error reading log directory: " << e.what() << std::endl;
        }
        
        return logFiles;
    }

    bool LogFileManager::FileExists(const std::filesystem::path& path) const
    {
        return std::filesystem::exists(path);
    }

    bool LogFileManager::MoveFile(const std::filesystem::path& from, const std::filesystem::path& to)
    {
        try {
            std::filesystem::rename(from, to);
            return true;
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Failed to move log file from " << from << " to " << to << ": " << e.what() << std::endl;
            return false;
        }
    }

    std::vector<std::filesystem::path> LogFileManager::GetArchivedLogFiles() const
    {
        std::vector<std::filesystem::path> archivedLogs;
        auto allLogs = GetExistingLogFiles();
        
        for (const auto& logFile : allLogs) {
            // Skip the current latest.log file
            if (logFile != m_currentLogPath) {
                archivedLogs.push_back(logFile);
            }
        }
        
        return archivedLogs;
    }

    void LogFileManager::SortLogFilesByDate(std::vector<std::filesystem::path>& files) const
    {
        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
            try {
                auto timeA = std::filesystem::last_write_time(a);
                auto timeB = std::filesystem::last_write_time(b);
                return timeA < timeB; // Oldest first
            } catch (const std::filesystem::filesystem_error&) {
                return false;
            }
        });
    }
    
    std::filesystem::path LogFileManager::ResolveLogDirectory(const std::filesystem::path& configPath) const
    {
        try {
            // Debug output
            std::filesystem::path currentPath = std::filesystem::current_path();
            std::cout << "ResolveLogDirectory - Current working directory: " << currentPath << std::endl;
            std::cout << "ResolveLogDirectory - Input config path: " << configPath << std::endl;
            
            // Handle empty or invalid paths
            if (configPath.empty()) {
                std::filesystem::path fallback = currentPath / "logs";
                std::cout << "ResolveLogDirectory - Empty path, using fallback: " << fallback << std::endl;
                return fallback;
            }
            
            // If it's already an absolute path, return as-is
            if (configPath.is_absolute()) {
                std::cout << "ResolveLogDirectory - Absolute path, returning as-is: " << configPath << std::endl;
                return configPath;
            }
            
            // For relative paths, resolve them relative to the working directory
            // The working directory should be set to Run/ by the application
            std::filesystem::path resolvedPath = currentPath / configPath;
            std::filesystem::path normalizedPath = resolvedPath.lexically_normal();
            
            std::cout << "ResolveLogDirectory - Resolved path: " << resolvedPath << std::endl;
            std::cout << "ResolveLogDirectory - Normalized path: " << normalizedPath << std::endl;
            
            return normalizedPath;
        } catch (const std::exception& e) {
            std::cerr << "Error resolving log directory path '" << configPath << "': " << e.what() << std::endl;
            // Return a safe fallback path
            std::filesystem::path fallback = std::filesystem::current_path() / "logs";
            std::cout << "ResolveLogDirectory - Exception, using fallback: " << fallback << std::endl;
            return fallback;
        }
    }
}