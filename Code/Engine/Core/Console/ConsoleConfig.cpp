#include "ConsoleConfig.hpp"
#include "Engine/Core/FileUtils.hpp"

namespace enigma::core
{
    ConsoleConfig ConsoleConfig::LoadFromYaml()
    {
        ConsoleConfig config;

        // Try to load from YAML configuration
        try
        {
            auto yamlConfigOpt = YamlConfiguration::TryLoadFromFile(".enigma/config/console.yml");
            if (yamlConfigOpt.has_value())
            {
                config = LoadFromYaml(yamlConfigOpt.value());
            }
        }
        catch (...)
        {
            // Use default config if loading fails
        }

        return config;
    }

    ConsoleConfig ConsoleConfig::LoadFromYaml(const YamlConfiguration& config)
    {
        ConsoleConfig consoleConfig;

        // Load basic settings
        consoleConfig.enableExternalConsole = config.GetBoolean("console.enabled", consoleConfig.enableExternalConsole);
        consoleConfig.enableAnsiColors      = config.GetBoolean("console.external_console.ansi_colors", consoleConfig.enableAnsiColors);
        consoleConfig.startupVisible        = config.GetBoolean("console.external_console.startup_visible", consoleConfig.startupVisible);

        // Load output mode
        std::string outputModeStr = config.GetString("console.output_mode", "auto");
        if (outputModeStr == "ide_only")
        {
            consoleConfig.outputMode = ConsoleOutputMode::IDE_ONLY;
        }
        else if (outputModeStr == "external_only")
        {
            consoleConfig.outputMode = ConsoleOutputMode::EXTERNAL_ONLY;
        }
        else if (outputModeStr == "both")
        {
            consoleConfig.outputMode = ConsoleOutputMode::BOTH;
        }
        else
        {
            consoleConfig.outputMode = ConsoleOutputMode::AUTO;
        }

        // Load window settings
        consoleConfig.windowTitle  = config.GetString("console.external_console.window.title", consoleConfig.windowTitle);
        consoleConfig.windowWidth  = config.GetInt("console.external_console.window.width", consoleConfig.windowWidth);
        consoleConfig.windowHeight = config.GetInt("console.external_console.window.height", consoleConfig.windowHeight);

        // Load input settings
        consoleConfig.captureInputWhenFocused = config.GetBoolean("console.external_console.input.capture_when_focused", consoleConfig.captureInputWhenFocused);
        consoleConfig.toggleKey               = config.GetBoolean("console.external_console.input.toggle_key", consoleConfig.toggleKey);

        // Load history settings
        consoleConfig.maxCommandHistory = config.GetInt("console.external_console.history.max_commands", consoleConfig.maxCommandHistory);
        consoleConfig.saveHistoryToFile = config.GetBoolean("console.external_console.history.save_to_file", consoleConfig.saveHistoryToFile);
        consoleConfig.historyFilePath   = config.GetString("console.external_console.history.file_path", consoleConfig.historyFilePath);

        // Load Windows-specific settings
        consoleConfig.windows.allocateNewConsole              = config.GetBoolean("console.external_console.windows.allocate_new_console", consoleConfig.windows.allocateNewConsole);
        consoleConfig.windows.enableVirtualTerminalProcessing = config.GetBoolean("console.external_console.windows.enable_vt_processing", consoleConfig.windows.enableVirtualTerminalProcessing);
        consoleConfig.windows.closeAppOnConsoleClose          = config.GetBoolean("console.external_console.windows.close_app_on_console_close", consoleConfig.windows.closeAppOnConsoleClose);
        consoleConfig.windows.redirectStdio                   = config.GetBoolean("console.external_console.windows.redirect_stdio", consoleConfig.windows.redirectStdio);

        return consoleConfig;
    }

    void ConsoleConfig::SaveToYaml() const
    {
        YamlConfiguration config;
        SaveToYaml(config);
        config.SaveToFile(".enigma/config/console.yml");
    }

    void ConsoleConfig::SaveToYaml(YamlConfiguration& config) const
    {
        config.Set("console.enabled", enableExternalConsole);
        config.Set("console.external_console.ansi_colors", enableAnsiColors);
        config.Set("console.external_console.startup_visible", startupVisible);

        config.Set("console.external_console.window.title", windowTitle);
        config.Set("console.external_console.window.width", windowWidth);
        config.Set("console.external_console.window.height", windowHeight);

        config.Set("console.external_console.input.capture_when_focused", captureInputWhenFocused);
        config.Set("console.external_console.input.toggle_key", toggleKey);

        config.Set("console.external_console.history.max_commands", maxCommandHistory);
        config.Set("console.external_console.history.save_to_file", saveHistoryToFile);
        config.Set("console.external_console.history.file_path", historyFilePath);

        config.Set("console.external_console.windows.allocate_new_console", windows.allocateNewConsole);
        config.Set("console.external_console.windows.enable_vt_processing", windows.enableVirtualTerminalProcessing);
        config.Set("console.external_console.windows.close_app_on_console_close", windows.closeAppOnConsoleClose);
        config.Set("console.external_console.windows.redirect_stdio", windows.redirectStdio);
    }
} // namespace enigma::core
