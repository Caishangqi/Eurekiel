#pragma once
#include <string>
#include <cstdint>

namespace enigma::voxel
{
    // Forward declaration
    namespace core
    {
        class YamlConfiguration;
    }

    /**
     * @brief Chunk save strategy configuration
     */
    enum class ChunkSaveStrategy
    {
        All, // Save all generated chunks
        ModifiedOnly, // Save only modified chunks (recommended)
        PlayerModifiedOnly // Save only player-modified chunks (future)
    };

    /**
     * @brief Chunk storage format selection
     */
    enum class ChunkStorageFormat
    {
        ESF, // Region file format (multiple chunks per file)
        ESFS // Single-file format (one chunk per file, ID-only)
    };

    /**
     * @brief Chunk Storage Configuration
     *
     * Loadable from YAML file: Run/.enigma/config/chunk_storage.yml
     */
    struct ChunkStorageConfig
    {
        //-------------------------------------------------------------------------------------------
        // Save Strategy
        //-------------------------------------------------------------------------------------------
        ChunkSaveStrategy saveStrategy = ChunkSaveStrategy::PlayerModifiedOnly;

        //-------------------------------------------------------------------------------------------
        // Storage Format
        //-------------------------------------------------------------------------------------------
        ChunkStorageFormat storageFormat = ChunkStorageFormat::ESFS;

        //-------------------------------------------------------------------------------------------
        // Compression
        //-------------------------------------------------------------------------------------------
        bool enableCompression = true;
        int  compressionLevel  = 3; // 1-9

        //-------------------------------------------------------------------------------------------
        // Cache (ESF format only)
        //-------------------------------------------------------------------------------------------
        size_t maxCachedRegions = 16;

        //-------------------------------------------------------------------------------------------
        // Auto-Save
        //-------------------------------------------------------------------------------------------
        bool  autoSaveEnabled  = true;
        float autoSaveInterval = 300.0f; // 5 minutes

        //-------------------------------------------------------------------------------------------
        // Paths
        //-------------------------------------------------------------------------------------------
        std::string baseSavePath = ".enigma/saves";

        //-------------------------------------------------------------------------------------------
        // Methods
        //-------------------------------------------------------------------------------------------
        static ChunkStorageConfig LoadFromYaml(const std::string& filePath);
        bool                      SaveToYaml(const std::string& filePath) const;
        static ChunkStorageConfig GetDefault();
        bool                      Validate() const;
        std::string               ToString() const;
    };

    //-------------------------------------------------------------------------------------------
    // Helper Functions
    //-------------------------------------------------------------------------------------------
    const char*        ChunkSaveStrategyToString(ChunkSaveStrategy strategy);
    ChunkSaveStrategy  StringToChunkSaveStrategy(const std::string& str);
    const char*        ChunkStorageFormatToString(ChunkStorageFormat format);
    ChunkStorageFormat StringToChunkStorageFormat(const std::string& str);
} // namespace enigma::voxel
