#pragma once
#include "../Chunk/ChunkSerializationInterfaces.hpp"
#include "../Chunk/ESFRegionFile.hpp"
#include "../Chunk/BlockStateSerializer.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include <string>
#include <memory>
#include <fstream>
#include <unordered_map>
#include <mutex>
#include <cstdint>

namespace enigma::voxel::world
{
    /**
     * @brief ESF-based chunk storage implementation
     *
     * Implements IChunkStorage interface using ESF region files for persistent chunk storage.
     * Integrates with World class to provide automatic chunk save/load functionality.
     */
    class ESFChunkStorage : public enigma::voxel::chunk::IChunkStorage
    {
    public:
        explicit ESFChunkStorage(const std::string& worldPath);
        ~ESFChunkStorage() override = default;

        // IChunkStorage interface implementation
        bool SaveChunk(int32_t chunkX, int32_t chunkY, const void* data) override;
        bool LoadChunk(int32_t chunkX, int32_t chunkY, void* data) override;
        bool ChunkExists(int32_t chunkX, int32_t chunkY) const override;
        bool DeleteChunk(int32_t chunkX, int32_t chunkY) override;

        // ESF specific methods with proper types
        bool SaveChunkData(enigma::voxel::chunk::Chunk* chunk, int32_t chunkX, int32_t chunkY);
        bool LoadChunkData(enigma::voxel::chunk::Chunk* chunk, int32_t chunkX, int32_t chunkY);

        // Storage management
        void Flush(); // Force write all pending data to disk
        void Close(); // Close all open region files

        // Statistics and info
        size_t      GetLoadedRegionCount() const;
        std::string GetStorageInfo() const;

    private:
        std::string                                              m_worldPath;
        enigma::voxel::chunk::BlockStateSerializer::StateMapping m_stateMapping;

        // Region file caching for performance
        struct RegionFileCache
        {
            std::string  regionPath;
            std::fstream fileStream;
            bool         isDirty        = false;
            uint64_t     lastAccessTime = 0;
        };

        mutable std::unordered_map<std::string, std::unique_ptr<RegionFileCache>> m_regionCache;
        static constexpr size_t                                                   MAX_CACHED_REGIONS = 16; // Maximum number of cached region files
        mutable std::mutex                                                        m_cacheMutex;

        // Helper methods
        std::vector<uint32_t> SerializeChunkBlocks(enigma::voxel::chunk::Chunk* chunk);
        bool                  DeserializeChunkBlocks(enigma::voxel::chunk::Chunk* chunk, const std::vector<uint32_t>& blockData);
        std::string           GetWorldSavePath() const;
        void                  EnsureWorldDirectoryExists();

        // Cache management methods
        RegionFileCache* GetOrCreateRegionFile(const std::string& regionPath) const;
        void             EvictLeastRecentlyUsedRegion() const;
        std::string      GetRegionFilePath(int32_t regionX, int32_t regionY) const;
    };

    /**
     * @brief ESF-based chunk serializer implementation
     *
     * Implements IChunkSerializer interface using BlockStateSerializer for chunk data conversion.
     */
    class ESFChunkSerializer : public enigma::voxel::chunk::IChunkSerializer
    {
    public:
        ESFChunkSerializer()           = default;
        ~ESFChunkSerializer() override = default;

        // IChunkSerializer interface implementation
        bool SerializeChunk(const enigma::voxel::chunk::Chunk* chunk) override;
        bool DeserializeChunk(enigma::voxel::chunk::Chunk* chunk) override;

        // ESF specific methods with data handling
        std::vector<uint8_t> SerializeChunkData(enigma::voxel::chunk::Chunk* chunk);
        bool                 DeserializeChunkData(enigma::voxel::chunk::Chunk* chunk, const std::vector<uint8_t>& data);

        // Utility methods
        size_t GetSerializedSize(enigma::voxel::chunk::Chunk* chunk) const;
        bool   ValidateSerializedData(const std::vector<uint8_t>& data) const;

    private:
        enigma::voxel::chunk::BlockStateSerializer::StateMapping m_stateMapping;
    };

    /**
     * @brief World file manager for ESF-based worlds
     *
     * Manages world-level metadata and coordinates chunk storage operations.
     */
    class ESFWorldManager
    {
    public:
        struct WorldInfo
        {
            std::string worldName;
            uint64_t    worldSeed    = 0;
            int32_t     worldVersion = 1;
            int64_t     lastPlayed   = 0;
            int32_t     spawnX       = 0, spawnY = 0, spawnZ = 128;

            std::string ToString() const;
        };

        explicit ESFWorldManager(const std::string& worldPath);

        // World metadata management
        bool SaveWorldInfo(const WorldInfo& info);
        bool LoadWorldInfo(WorldInfo& info);
        bool WorldExists() const;

        // World directory management
        bool                     CreateWorld(const WorldInfo& info);
        bool                     DeleteWorld();
        std::vector<std::string> ListWorlds(const std::string& savesPath);

        // Utility
        std::string GetWorldPath() const { return m_worldPath; }
        std::string GetWorldInfoPath() const;

    private:
        std::string m_worldPath;

        void EnsureWorldDirectoryExists();
        bool WriteWorldInfoFile(const WorldInfo& info);
        bool ReadWorldInfoFile(WorldInfo& info);
    };
}
