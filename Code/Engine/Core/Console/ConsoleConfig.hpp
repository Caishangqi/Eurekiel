#pragma once
#include <string>
#include <vector>
#include "Engine/Core/Logger/LogLevel.hpp"
#include "Engine/Core/Yaml.hpp"

namespace enigma::core
{
    // Console output modes for different environments
    enum class ConsoleOutputMode
    {
        AUTO, // Auto-detect: Debug->IDE Console, Release->External Console
        IDE_ONLY, // Output only to IDE/debugger console (OutputDebugString)
        EXTERNAL_ONLY, // Output only to external console window
        BOTH // Output to both IDE and external console
    };

    struct ConsoleConfig
    {
        // Basic settings
        bool     enableExternalConsole = false;
        bool     enableAnsiColors      = true;
        bool     startupVisible        = true;
        LogLevel verbosityLevel        = LogLevel::INFO;

        // Output mode for development vs runtime
        ConsoleOutputMode outputMode = ConsoleOutputMode::AUTO;

        // Window settings
        std::string windowTitle  = "Eurekiel Engine Console";
        int         windowWidth  = 120;
        int         windowHeight = 30;

        // Input settings (integrated with InputSystem)
        bool captureInputWhenFocused = true;
        bool toggleKey               = true; // Use ~ key to toggle visibility

        // History settings
        int         maxCommandHistory = 1000;
        bool        saveHistoryToFile = true;
        std::string historyFilePath   = ".enigma/logs/console_history.log";

        // Logger integration
        bool                     forwardToLogger = true;
        std::vector<std::string> logCategories   = {"Console", "Commands"};

        // Windows specific settings
        struct WindowsSettings
        {
            bool allocateNewConsole              = true;
            bool enableVirtualTerminalProcessing = true;
            bool closeAppOnConsoleClose          = true; // Console close = app exit
            bool redirectStdio                   = true; // Redirect stdout/stderr/stdin to console
        } windows;

        // Static factory methods
        static ConsoleConfig LoadFromYaml();
        static ConsoleConfig LoadFromYaml(const YamlConfiguration& config);
        void                 SaveToYaml() const;
        void                 SaveToYaml(YamlConfiguration& config) const;
    };
} // namespace enigma::core
