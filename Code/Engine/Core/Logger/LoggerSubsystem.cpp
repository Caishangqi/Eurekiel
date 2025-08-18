#include "LoggerSubsystem.hpp"
#include "../Console/DevConsole.hpp"
#include "../Engine.hpp"
#include "../Yaml.hpp"
#include "Appenders/ConsoleAppender.hpp"
#include "Appenders/DevConsoleAppender.hpp"
#include "Appenders/FileAppender.hpp"
#include <stdarg.h>
#include <stdio.h>
#include <iostream>
#include <algorithm>
#include <filesystem>

// Forward declaration for g_theDevConsole (will be properly included later)
extern DevConsole* g_theDevConsole;

namespace enigma::core
{
    LoggerSubsystem::LoggerSubsystem()
        : m_config() // Default configuration
        , m_fileManager(nullptr)
    {
        // Default constructor - configuration will be loaded in Initialize()
    }
    
    LoggerSubsystem::LoggerSubsystem(const LoggerConfig& config)
        : m_config(config)
        , m_fileManager(std::make_unique<LogFileManager>(config))
    {
        // Configuration provided directly
    }
    
    void LoggerSubsystem::Initialize()
    {
        // Load configuration from YAML if not already configured
        if (m_config.logDirectory.empty()) {
            LoadConfigurationFromYaml();
        }
        
        // Initialize file manager if file logging is enabled
        if (m_config.enableFileLogging && !m_fileManager) {
            m_fileManager = std::make_unique<LogFileManager>(m_config);
        }
        
        if (m_fileManager && !m_fileManager->Initialize()) {
            std::cerr << "Failed to initialize LogFileManager" << std::endl;
        }
        
        // Apply configuration settings
        ApplyConfiguration();
        
        // Create default appenders
        CreateDefaultAppenders();
    }
    
    void LoggerSubsystem::Startup()
    {
        // Initialize with default settings
        m_globalLogLevel = LogLevel::INFO;
        m_asyncMode = false; // Start with synchronous mode for Phase 3.1
        
        // Don't log during startup to avoid circular dependencies
        // Startup message will be logged after configuration is complete
    }

    void LoggerSubsystem::Shutdown()
    {
        // Don't log during shutdown to avoid issues with destroyed components
        
        // If async mode is active, stop worker thread
        if (m_asyncMode && m_workerThread.joinable()) {
            m_shouldStop = true;
            m_queueCondition.notify_all();
            m_workerThread.join();
        }
        
        // Flush all appenders
        Flush();
        
        // Clear all appenders
        RemoveAllAppenders();
    }

    void LoggerSubsystem::Log(LogLevel level, const std::string& category, const std::string& message)
    {
        // Quick level check to avoid unnecessary work
        if (!ShouldLogMessage(level, category)) {
            return;
        }

        // Create log message with current frame number
        LogMessage logMessage(level, category, message, GetCurrentFrameNumber());
        
        // Process based on current mode
        if (m_asyncMode) {
            ProcessMessageAsync(logMessage);
        } else {
            ProcessMessageSync(logMessage);
        }
    }

    void LoggerSubsystem::LogFormatted(LogLevel level, const std::string& category, const char* format, ...)
    {
        // Quick level check
        if (!ShouldLogMessage(level, category)) {
            return;
        }

        // Format the message
        va_list args;
        va_start(args, format);
        
        // First, determine the size needed
        va_list args_copy;
        va_copy(args_copy, args);
        int size = vsnprintf(nullptr, 0, format, args_copy);
        va_end(args_copy);
        
        if (size > 0) {
            std::string formatted(size + 1, '\0');
            vsnprintf(&formatted[0], size + 1, format, args);
            formatted.resize(size); // Remove the null terminator
            
            Log(level, category, formatted);
        }
        
        va_end(args);
    }

    // Convenience methods
    void LoggerSubsystem::LogTrace(const std::string& category, const std::string& message) {
        Log(LogLevel::TRACE, category, message);
    }

    void LoggerSubsystem::LogDebug(const std::string& category, const std::string& message) {
        Log(LogLevel::DEBUG, category, message);
    }

    void LoggerSubsystem::LogInfo(const std::string& category, const std::string& message) {
        Log(LogLevel::INFO, category, message);
    }

    void LoggerSubsystem::LogWarn(const std::string& category, const std::string& message) {
        Log(LogLevel::WARNING, category, message);
    }

    void LoggerSubsystem::LogError(const std::string& category, const std::string& message) {
        Log(LogLevel::ERROR_, category, message);
    }

    void LoggerSubsystem::LogFatal(const std::string& category, const std::string& message) {
        Log(LogLevel::FATAL, category, message);
    }

    // Appender management
    void LoggerSubsystem::AddAppender(std::unique_ptr<ILogAppender> appender)
    {
        std::lock_guard<std::mutex> lock(m_appendersMutex);
        m_appenders.push_back(std::move(appender));
    }

    void LoggerSubsystem::RemoveAllAppenders()
    {
        std::lock_guard<std::mutex> lock(m_appendersMutex);
        m_appenders.clear();
    }

    // Configuration
    void LoggerSubsystem::SetGlobalLogLevel(LogLevel level)
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        m_globalLogLevel = level;
    }

    void LoggerSubsystem::SetCategoryLogLevel(const std::string& category, LogLevel level)
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        m_categoryLogLevels[category] = level;
    }

    LogLevel LoggerSubsystem::GetEffectiveLogLevel(const std::string& category) const
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        
        auto it = m_categoryLogLevels.find(category);
        if (it != m_categoryLogLevels.end()) {
            return it->second;
        }
        
        return m_globalLogLevel;
    }

    void LoggerSubsystem::Flush()
    {
        std::lock_guard<std::mutex> lock(m_appendersMutex);
        
        for (auto& appender : m_appenders) {
            if (appender && appender->IsEnabled()) {
                appender->Flush();
            }
        }
    }

    // Private methods
    void LoggerSubsystem::ProcessMessageSync(const LogMessage& message)
    {
        std::lock_guard<std::mutex> lock(m_appendersMutex);
        
        for (auto& appender : m_appenders) {
            if (appender && appender->IsEnabled()) {
                try {
                    appender->Write(message);
                } catch (const std::exception& e) {
                    // Prevent logging failures from crashing the application
                    // In a real implementation, we might want to disable the failing appender
                    std::cerr << "Logger appender failed: " << e.what() << std::endl;
                }
            }
        }
    }

    void LoggerSubsystem::ProcessMessageAsync(const LogMessage& message)
    {
        // Phase 3.4 implementation - for now, fall back to sync
        ProcessMessageSync(message);
    }

    void LoggerSubsystem::WorkerThreadFunction()
    {
        // Phase 3.4 implementation - placeholder
    }

    bool LoggerSubsystem::ShouldLogMessage(LogLevel level, const std::string& category) const
    {
        LogLevel effectiveLevel = GetEffectiveLogLevel(category);
        return level >= effectiveLevel;
    }

    int LoggerSubsystem::GetCurrentFrameNumber() const
    {
        // Try to get frame number from DevConsole if available
        if (g_theDevConsole) {
            return g_theDevConsole->GetFrameNumber();
        }
        return 0;
    }
    
    void LoggerSubsystem::LoadConfigurationFromYaml()
    {
        try {
            m_config = LoadLoggerConfigFromYaml();
        } catch (const std::exception& e) {
            std::cerr << "Failed to load logger configuration: " << e.what() << std::endl;
            // Use default configuration
            m_config = LoggerConfig();
        }
    }
    
    LoggerConfig LoggerSubsystem::LoadLoggerConfigFromYaml() const
    {
        LoggerConfig config;
        
        // Try to load from module.yml - check multiple possible paths
        std::vector<std::string> possiblePaths = {
            "../Engine/.enigma/config/engine/module.yml",  // From Run/ directory
            "F:/p4/Personal/SD/Engine/.enigma/config/engine/module.yml",  // Absolute path
            ".enigma/config/engine/module.yml"  // Relative to current dir
        };
        
        std::string modulePath;
        for (const auto& path : possiblePaths) {
            if (std::filesystem::exists(path)) {
                modulePath = path;
                break;
            }
        }
        
        if (!modulePath.empty()) {
            std::cout << "Loading logger configuration from: " << modulePath << std::endl;
            try {
                auto yamlConfig = YamlConfiguration::LoadFromFile(modulePath);
                
                // Load logger-specific configuration
                if (yamlConfig.Contains("logger")) {
                    config.globalLogLevel = StringToLogLevel(
                        yamlConfig.GetString("logger.globalLogLevel", "INFO")
                    );
                    config.enableFileLogging = yamlConfig.GetBoolean("logger.enableFileLogging", true);
                    config.logDirectory = yamlConfig.GetString("logger.logDirectory", "Run/.enigma/logs");
                    config.latestLogFileName = yamlConfig.GetString("logger.latestLogFileName", "latest.log");
                    config.enableLogRotation = yamlConfig.GetBoolean("logger.enableLogRotation", true);
                    config.maxLogFiles = yamlConfig.GetInt("logger.maxLogFiles", 10);
                    
                    // Load category-specific log levels
                    if (yamlConfig.Contains("logger.categoryLogLevels")) {
                        auto categorySection = yamlConfig.GetConfigurationSection("logger.categoryLogLevels");
                        auto keys = categorySection.GetKeys();
                        for (const auto& category : keys) {
                            std::string levelStr = categorySection.GetString(category, "INFO");
                            config.categoryLogLevels[category] = StringToLogLevel(levelStr);
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error parsing logger configuration from " << modulePath << ": " << e.what() << std::endl;
            }
        } else {
            std::cout << "No module.yml found, using default logger configuration" << std::endl;
        }
        
        // Debug output for loaded configuration
        std::cout << "Logger config - directory: " << config.logDirectory 
                  << ", filename: " << config.latestLogFileName 
                  << ", fileLogging: " << (config.enableFileLogging ? "enabled" : "disabled") << std::endl;
        
        return config;
    }
    
    void LoggerSubsystem::ApplyConfiguration()
    {
        // Apply global log level
        SetGlobalLogLevel(m_config.globalLogLevel);
        
        // Apply category-specific log levels
        for (const auto& pair : m_config.categoryLogLevels) {
            SetCategoryLogLevel(pair.first, pair.second);
        }
    }
    
    void LoggerSubsystem::CreateDefaultAppenders()
    {
        // Always add console appender
        AddAppender(std::make_unique<ConsoleAppender>());
        
        // Add DevConsole appender if available
        if (g_theDevConsole) {
            AddAppender(std::make_unique<DevConsoleAppender>());
        }
        
        // Add file appender if file logging is enabled
        if (m_config.enableFileLogging && m_fileManager) {
            AddAppender(std::make_unique<FileAppender>(m_config.GetLatestLogPath().string()));
        }
    }
}