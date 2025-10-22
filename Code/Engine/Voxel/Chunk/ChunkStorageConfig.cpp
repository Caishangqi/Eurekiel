#include "ChunkStorageConfig.hpp"
#include "Engine/Core/Yaml.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include <sstream>

DEFINE_LOG_CATEGORY(LogChunkSave)

namespace enigma::voxel
{
    using namespace enigma::core;

    //-------------------------------------------------------------------------------------------
    // Default Configuration Path
    //-------------------------------------------------------------------------------------------
    static constexpr const char* DEFAULT_CONFIG_PATH = ".enigma/config/engine/chunkstorage.yml";

    //-------------------------------------------------------------------------------------------
    // Enum to String Conversions
    //-------------------------------------------------------------------------------------------

    const char* ChunkSaveStrategyToString(ChunkSaveStrategy strategy)
    {
        switch (strategy)
        {
        case ChunkSaveStrategy::All: return "All";
        case ChunkSaveStrategy::ModifiedOnly: return "ModifiedOnly";
        case ChunkSaveStrategy::PlayerModifiedOnly: return "PlayerModifiedOnly";
        default: return "Unknown";
        }
    }

    ChunkSaveStrategy StringToChunkSaveStrategy(const std::string& str)
    {
        if (str == "All") return ChunkSaveStrategy::All;
        if (str == "ModifiedOnly") return ChunkSaveStrategy::ModifiedOnly;
        if (str == "PlayerModifiedOnly") return ChunkSaveStrategy::PlayerModifiedOnly;

        LogWarn(LogChunkSave, "Unknown ChunkSaveStrategy: %s, using default ModifiedOnly", str.c_str());
        return ChunkSaveStrategy::ModifiedOnly;
    }

    const char* ChunkStorageFormatToString(ChunkStorageFormat format)
    {
        switch (format)
        {
        case ChunkStorageFormat::ESF: return "ESF";
        case ChunkStorageFormat::ESFS: return "ESFS";
        default: return "Unknown";
        }
    }

    ChunkStorageFormat StringToChunkStorageFormat(const std::string& str)
    {
        if (str == "ESF") return ChunkStorageFormat::ESF;
        if (str == "ESFS") return ChunkStorageFormat::ESFS;

        LogWarn(LogChunkSave, "Unknown ChunkStorageFormat: %s, using default ESFS", str.c_str());
        return ChunkStorageFormat::ESFS;
    }

    //-------------------------------------------------------------------------------------------
    // ChunkStorageConfig Implementation
    //-------------------------------------------------------------------------------------------

    ChunkStorageConfig ChunkStorageConfig::GetDefault()
    {
        ChunkStorageConfig config;
        config.saveStrategy      = ChunkSaveStrategy::PlayerModifiedOnly;
        config.storageFormat     = ChunkStorageFormat::ESFS;
        config.enableCompression = true;
        config.compressionLevel  = 3;
        config.maxCachedRegions  = 16;
        config.autoSaveEnabled   = true;
        config.autoSaveInterval  = 300.0f;
        config.baseSavePath      = ".enigma/saves";
        return config;
    }

    ChunkStorageConfig ChunkStorageConfig::LoadFromYaml(const std::string& filePath)
    {
        std::string actualPath = filePath.empty() ? DEFAULT_CONFIG_PATH : filePath;

        ChunkStorageConfig config = GetDefault();

        try
        {
            // Try to load YAML file
            auto yamlOpt = YamlConfiguration::TryLoadFromFile(actualPath);
            if (!yamlOpt.has_value())
            {
                LogWarn(LogChunkSave, "Failed to load config from '%s', using defaults", actualPath.c_str());
                return config;
            }

            YamlConfiguration yaml = yamlOpt.value();

            // Load chunk_storage section
            if (!yaml.Contains("chunk_storage"))
            {
                LogWarn(LogChunkSave, "Config file missing 'chunk_storage' section, using defaults");
                return config;
            }

            // Save Strategy
            if (yaml.IsSet("chunk_storage.save_strategy"))
            {
                std::string strategyStr = yaml.GetString("chunk_storage.save_strategy", "PlayerModifiedOnly");
                config.saveStrategy     = StringToChunkSaveStrategy(strategyStr);
            }

            // Storage Format
            if (yaml.IsSet("chunk_storage.storage_format"))
            {
                std::string formatStr = yaml.GetString("chunk_storage.storage_format", "ESFS");
                config.storageFormat  = StringToChunkStorageFormat(formatStr);
            }

            // Compression
            if (yaml.IsSet("chunk_storage.compression.enabled"))
            {
                config.enableCompression = yaml.GetBoolean("chunk_storage.compression.enabled", true);
            }
            if (yaml.IsSet("chunk_storage.compression.level"))
            {
                config.compressionLevel = yaml.GetInt("chunk_storage.compression.level", 3);
            }

            // Cache
            if (yaml.IsSet("chunk_storage.cache.max_regions"))
            {
                config.maxCachedRegions = static_cast<size_t>(yaml.GetInt("chunk_storage.cache.max_regions", 16));
            }

            // Auto-Save
            if (yaml.IsSet("chunk_storage.auto_save.enabled"))
            {
                config.autoSaveEnabled = yaml.GetBoolean("chunk_storage.auto_save.enabled", true);
            }
            if (yaml.IsSet("chunk_storage.auto_save.interval"))
            {
                config.autoSaveInterval = yaml.GetFloat("chunk_storage.auto_save.interval", 300.0f);
            }

            // Paths
            if (yaml.IsSet("chunk_storage.paths.base_save_path"))
            {
                config.baseSavePath = yaml.GetString("chunk_storage.paths.base_save_path", ".enigma/saves");
            }

            // Validate loaded config
            if (!config.Validate())
            {
                LogError(LogChunkSave, "Loaded config is invalid, falling back to defaults");
                return GetDefault();
            }

            LogInfo(LogChunkSave, "Successfully loaded config from '%s'", actualPath.c_str());
            LogInfo(LogChunkSave, "Config: %s", config.ToString().c_str());
        }
        catch (const std::exception& e)
        {
            LogError(LogChunkSave, "Exception loading config: %s, using defaults", e.what());
            return GetDefault();
        }

        return config;
    }

    bool ChunkStorageConfig::SaveToYaml(const std::string& filePath) const
    {
        std::string actualPath = filePath.empty() ? DEFAULT_CONFIG_PATH : filePath;

        try
        {
            YamlConfiguration yaml;

            // Build YAML structure using Set() method
            yaml.Set("chunk_storage.save_strategy", ChunkSaveStrategyToString(saveStrategy));
            yaml.Set("chunk_storage.storage_format", ChunkStorageFormatToString(storageFormat));

            yaml.Set("chunk_storage.compression.enabled", enableCompression);
            yaml.Set("chunk_storage.compression.level", compressionLevel);

            yaml.Set("chunk_storage.cache.max_regions", static_cast<int>(maxCachedRegions));

            yaml.Set("chunk_storage.auto_save.enabled", autoSaveEnabled);
            yaml.Set("chunk_storage.auto_save.interval", autoSaveInterval);

            yaml.Set("chunk_storage.paths.base_save_path", baseSavePath);

            // Save to file
            if (!yaml.SaveToFile(actualPath))
            {
                LogError(LogChunkSave, "Failed to save config to '%s'", actualPath.c_str());
                return false;
            }

            LogInfo(LogChunkSave, "Successfully saved config to '%s'", actualPath.c_str());
            return true;
        }
        catch (const std::exception& e)
        {
            LogError(LogChunkSave, "Exception saving config: %s", e.what());
            return false;
        }
    }

    bool ChunkStorageConfig::Validate() const
    {
        // Validate compression level
        if (enableCompression && (compressionLevel < 1 || compressionLevel > 9))
        {
            LogError(LogChunkSave, "Invalid compressionLevel: %d (must be 1-9)", compressionLevel);
            return false;
        }

        // Validate cache size
        if (maxCachedRegions < 1 || maxCachedRegions > 256)
        {
            LogError(LogChunkSave, "Invalid maxCachedRegions: %zu (must be 1-256)", maxCachedRegions);
            return false;
        }

        // Validate auto-save interval
        if (autoSaveEnabled && (autoSaveInterval < 10.0f || autoSaveInterval > 3600.0f))
        {
            LogError(LogChunkSave, "Invalid autoSaveInterval: %.1f (must be 10-3600 seconds)", autoSaveInterval);
            return false;
        }

        // Validate base path
        if (baseSavePath.empty())
        {
            LogError(LogChunkSave, "baseSavePath cannot be empty");
            return false;
        }

        return true;
    }

    std::string ChunkStorageConfig::ToString() const
    {
        std::ostringstream oss;
        oss << "ChunkStorageConfig {\n";
        oss << "  saveStrategy: " << ChunkSaveStrategyToString(saveStrategy) << "\n";
        oss << "  storageFormat: " << ChunkStorageFormatToString(storageFormat) << "\n";
        oss << "  compression: " << (enableCompression ? "enabled" : "disabled")
            << " (level " << compressionLevel << ")\n";
        oss << "  maxCachedRegions: " << maxCachedRegions << "\n";
        oss << "  autoSave: " << (autoSaveEnabled ? "enabled" : "disabled")
            << " (interval " << autoSaveInterval << "s)\n";
        oss << "  baseSavePath: " << baseSavePath << "\n";
        oss << "}";
        return oss.str();
    }
} // namespace enigma::voxel
