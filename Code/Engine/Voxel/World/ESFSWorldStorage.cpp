#include "ESFSWorldStorage.hpp"
#include "../Chunk/Chunk.hpp"
#include "../Chunk/ESFSFile.hpp"
#include "../../Core/Logger/LoggerAPI.hpp"
#include <fstream>

namespace enigma::voxel
{
    using namespace enigma::core;

    //-------------------------------------------------------------------------------------------
    // Constructor & Destructor
    //-------------------------------------------------------------------------------------------
    ESFSChunkStorage::ESFSChunkStorage(
        const std::string&        worldPath,
        const ChunkStorageConfig& config,
        IChunkSerializer*         serializer)
        : m_worldPath(worldPath)
          , m_config(config)
          , m_serializer(serializer)
    {
        // Ensure region directory exists
        if (!ESFSFile::EnsureRegionDirectory(worldPath))
        {
            LogError("esfs_storage", "Failed to create region directory for world: %s", worldPath.c_str());
        }

        LogInfo("esfs_storage", "Initialized ESFS storage for world: %s", worldPath.c_str());
        LogInfo("esfs_storage", "Config: %s", config.ToString().c_str());
    }

    //-------------------------------------------------------------------------------------------
    // IChunkStorage Interface Implementation
    //-------------------------------------------------------------------------------------------
    bool ESFSChunkStorage::SaveChunk(int32_t chunkX, int32_t chunkY, const void* data)
    {
        if (!data)
        {
            LogError("esfs_storage", "Cannot save chunk (%d, %d): data is null", chunkX, chunkY);
            return false;
        }

        if (!m_serializer)
        {
            LogError("esfs_storage", "Cannot save chunk (%d, %d): serializer not configured", chunkX, chunkY);
            return false;
        }

        const Chunk* chunk = static_cast<const Chunk*>(data);

        // Apply save strategy filter
        switch (m_config.saveStrategy)
        {
        case ChunkSaveStrategy::ModifiedOnly:
            if (!chunk->IsModified())
            {
                LogDebug("esfs_storage", "Skipping save for unmodified chunk (%d, %d)", chunkX, chunkY);
                return true; // Not an error, just skipped
            }
            break;

        case ChunkSaveStrategy::PlayerModifiedOnly:
            // Check player-modified flag (only save chunks modified by player actions)
            if (!chunk->IsPlayerModified())
            {
                LogDebug("esfs_storage", "Skipping save for non-player-modified chunk (%d, %d)", chunkX, chunkY);
                return true;
            }
            break;

        case ChunkSaveStrategy::All:
        default:
            // Save all chunks regardless
            break;
        }

        // Serialize chunk using serializer (ESFS format: Header + RLE data)
        std::vector<uint8_t> serializedData;
        if (!m_serializer->SerializeChunk(chunk, serializedData))
        {
            LogError("esfs_storage", "Failed to serialize chunk (%d, %d)", chunkX, chunkY);
            return false;
        }

        // Write serialized data to file
        std::string   filePath = ESFSFile::GetChunkFilePath(m_worldPath, chunkX, chunkY);
        std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
        {
            LogError("esfs_storage", "Failed to open file for writing: %s", filePath.c_str());
            return false;
        }

        file.write(reinterpret_cast<const char*>(serializedData.data()), serializedData.size());
        file.close();

        ++m_chunksSaved;
        LogDebug("esfs_storage", "Saved chunk (%d, %d) to %s (%zu bytes) - Total saved: %zu",
                 chunkX, chunkY, filePath.c_str(), serializedData.size(), m_chunksSaved);

        return true;
    }

    bool ESFSChunkStorage::LoadChunk(int32_t chunkX, int32_t chunkY, void* data)
    {
        if (!data)
        {
            LogError("esfs_storage", "Cannot load chunk (%d, %d): data is null", chunkX, chunkY);
            return false;
        }

        if (!m_serializer)
        {
            LogError("esfs_storage", "Cannot load chunk (%d, %d): serializer not configured", chunkX, chunkY);
            return false;
        }

        Chunk* chunk = static_cast<Chunk*>(data);

        // Check if chunk file exists
        std::string filePath = ESFSFile::GetChunkFilePath(m_worldPath, chunkX, chunkY);
        if (!ESFSFile::ChunkExists(m_worldPath, chunkX, chunkY))
        {
            LogDebug("esfs_storage", "Chunk (%d, %d) does not exist on disk", chunkX, chunkY);
            return false;
        }

        // Read serialized data from file
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open())
        {
            LogError("esfs_storage", "Failed to open file for reading: %s", filePath.c_str());
            return false;
        }

        // Read entire file into buffer
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> serializedData(fileSize);
        file.read(reinterpret_cast<char*>(serializedData.data()), fileSize);
        file.close();

        // Deserialize chunk using serializer
        if (!m_serializer->DeserializeChunk(chunk, serializedData))
        {
            LogError("esfs_storage", "Failed to deserialize chunk (%d, %d)", chunkX, chunkY);
            return false;
        }

        ++m_chunksLoaded;
        LogDebug("esfs_storage", "Loaded chunk (%d, %d) from %s (%zu bytes) - Total loaded: %zu",
                 chunkX, chunkY, filePath.c_str(), fileSize, m_chunksLoaded);

        return true;
    }

    bool ESFSChunkStorage::ChunkExists(int32_t chunkX, int32_t chunkY) const
    {
        return ESFSFile::ChunkExists(m_worldPath, chunkX, chunkY);
    }

    bool ESFSChunkStorage::DeleteChunk(int32_t chunkX, int32_t chunkY)
    {
        bool success = ESFSFile::DeleteChunk(m_worldPath, chunkX, chunkY);

        if (success)
        {
            LogDebug("esfs_storage", "Deleted chunk (%d, %d)", chunkX, chunkY);
        }
        else
        {
            LogError("esfs_storage", "Failed to delete chunk (%d, %d)", chunkX, chunkY);
        }

        return success;
    }

    void ESFSChunkStorage::Flush()
    {
        // ESFS format writes are immediate (no buffering)
        // This method is a no-op for ESFS
        LogDebug("esfs_storage", "Flush() called - ESFS writes are immediate, no action needed");
    }

    void ESFSChunkStorage::Close()
    {
        LogInfo("esfs_storage", "Closing ESFS storage for world: %s", m_worldPath.c_str());
        LogInfo("esfs_storage", "Final statistics: %s", GetStatistics().c_str());
    }

    //-------------------------------------------------------------------------------------------
    // ESFS-Specific Methods
    //-------------------------------------------------------------------------------------------
    std::string ESFSChunkStorage::GetStatistics() const
    {
        std::string stats;
        stats += "ESFS Storage Statistics:\n";
        stats += "  World Path: " + m_worldPath + "\n";
        stats += "  Chunks Loaded: " + std::to_string(m_chunksLoaded) + "\n";
        stats += "  Chunks Saved: " + std::to_string(m_chunksSaved) + "\n";
        stats += "  Storage Format: ESFS (Single-file)\n";
        stats += "  Compression: RLE (Run-Length Encoding)\n";
        stats += "  Save Strategy: " + std::string(ChunkSaveStrategyToString(m_config.saveStrategy));
        return stats;
    }

    //-------------------------------------------------------------------------------------------
    // Job-Compatible Methods (for LoadChunkJob/SaveChunkJob)
    //-------------------------------------------------------------------------------------------
    bool ESFSChunkStorage::LoadChunkData(Chunk* chunk, int32_t chunkX, int32_t chunkY)
    {
        // Delegate to LoadChunk interface method
        return LoadChunk(chunkX, chunkY, chunk);
    }

    bool ESFSChunkStorage::SaveChunkFromSnapshot(int32_t chunkX, int32_t chunkY, const std::vector<BlockState*>& blockData)
    {
        if (!m_serializer)
        {
            LogError("esfs_storage", "Cannot save chunk snapshot (%d, %d): serializer not configured", chunkX, chunkY);
            return false;
        }

        // Create a temporary chunk to hold the snapshot data
        Chunk tempChunk(IntVec2(chunkX, chunkY));

        // Populate chunk with snapshot data
        size_t index = 0;
        for (int32_t z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
        {
            for (int32_t y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
            {
                for (int32_t x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
                {
                    if (index < blockData.size())
                    {
                        tempChunk.SetBlock(x, y, z, blockData[index++]);
                    }
                }
            }
        }

        // Use SaveChunk to write the data
        return SaveChunk(chunkX, chunkY, &tempChunk);
    }
} // namespace enigma::voxel
