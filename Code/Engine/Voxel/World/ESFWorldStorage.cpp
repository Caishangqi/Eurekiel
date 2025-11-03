#include "ESFWorldStorage.hpp"
#include "../Chunk/Chunk.hpp"
#include "../Chunk/ESFRegionFile.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <ctime>

#include "Engine/Voxel/Chunk/ChunkManager.hpp"

namespace enigma::voxel
{
    using namespace enigma::core;

    // ESFChunkStorage implementation
    ESFChunkStorage::ESFChunkStorage(const std::string& worldPath)
        : m_worldPath(worldPath)
    {
        EnsureWorldDirectoryExists();
        core::LogInfo("world_storage", "ESF chunk storage initialized for world: %s", worldPath.c_str());
    }

    bool ESFChunkStorage::SaveChunk(int32_t chunkX, int32_t chunkY, const void* data)
    {
        // Interface implementation - cast data to Chunk* for ESF-specific method
        if (!data)
            return false;

        const Chunk* chunk = static_cast<const Chunk*>(data);
        return SaveChunkData(const_cast<Chunk*>(chunk), chunkX, chunkY);
    }

    bool ESFChunkStorage::SaveChunkData(Chunk* chunk, int32_t chunkX, int32_t chunkY)
    {
        if (!chunk)
            return false;

        try
        {
            // Serialize chunk blocks to uint32_t array
            std::vector<uint32_t> blockData = SerializeChunkBlocks(chunk);
            if (blockData.empty())
            {
                core::LogError("world_storage", "Failed to serialize chunk (%d, %d) - empty block data", chunkX, chunkY);
                return false;
            }

            // Serialize StateMapping data
            const size_t         MAX_STATE_MAPPING_SIZE = 64 * 1024; // 64KB should be enough for state mapping
            std::vector<uint8_t> stateMappingBuffer(MAX_STATE_MAPPING_SIZE);
            size_t               stateMappingSize = m_stateMapping.SerializeStates(stateMappingBuffer.data(), stateMappingBuffer.size());

            if (stateMappingSize == 0)
            {
                core::LogError("world_storage", "Failed to serialize StateMapping for chunk (%d, %d)", chunkX, chunkY);
                return false;
            }

            core::LogInfo("world_storage", "Serialized StateMapping for chunk (%d, %d): %zu bytes, %zu states",
                          chunkX, chunkY, stateMappingSize, m_stateMapping.GetStateCount());

            // Create combined data: [Magic(4 bytes)][StateMappingSize(4 bytes)][StateMapping][BlockData]
            size_t               totalSize = sizeof(uint32_t) + sizeof(uint32_t) + stateMappingSize + blockData.size() * sizeof(uint32_t);
            std::vector<uint8_t> combinedData(totalSize);

            size_t offset = 0;

            // Write format magic number
            std::memcpy(combinedData.data() + offset, &ESF_V2_MAGIC, sizeof(uint32_t));
            offset += sizeof(uint32_t);

            // Write StateMapping size
            uint32_t mappingSize32 = static_cast<uint32_t>(stateMappingSize);
            std::memcpy(combinedData.data() + offset, &mappingSize32, sizeof(uint32_t));
            offset += sizeof(uint32_t);

            // Write StateMapping data
            std::memcpy(combinedData.data() + offset, stateMappingBuffer.data(), stateMappingSize);
            offset += stateMappingSize;

            // Write block data
            std::memcpy(combinedData.data() + offset, blockData.data(), blockData.size() * sizeof(uint32_t));

            // Save using ESF region files
            ESFError error = ESFError::None;
            try
            {
                // For now, use a simple approach - direct region file access
                // In production, this should use a proper ChunkFileManager
                int32_t regionX, regionY;
                ESFLayout::WorldChunkToRegion(chunkX, chunkY, regionX, regionY);

                std::string   regionPath = GetWorldSavePath() + "/" + ESFLayout::GenerateRegionFileName(regionX, regionY);
                ESFRegionFile regionFile(regionPath, regionX, regionY);

                if (!regionFile.IsValid())
                {
                    error = regionFile.GetLastError();
                }
                else
                {
                    int32_t localX, localY;
                    ESFLayout::WorldChunkToLocal(chunkX, chunkY, regionX, regionY, localX, localY);
                    error = regionFile.WriteChunk(localX, localY, combinedData.data(), combinedData.size());
                    if (error == ESFError::None)
                    {
                        error = regionFile.Flush();
                    }
                }
            }
            catch (...)
            {
                error = ESFError::FileIOError;
            }

            if (error == ESFError::None)
            {
                core::LogDebug("world_storage", "Chunk (%d, %d) saved successfully", chunkX, chunkY);
                return true;
            }
            else
            {
                core::LogError("world_storage", "Failed to save chunk (%d, %d): %s", chunkX, chunkY, ESFErrorToString(error));
                return false;
            }
        }
        catch (const std::exception& e)
        {
            core::LogError("world_storage", "Exception saving chunk (%d, %d): %s", chunkX, chunkY, e.what());
            return false;
        }
    }

    bool ESFChunkStorage::SaveChunkFromSnapshot(int32_t                         chunkX, int32_t chunkY,
                                                const std::vector<BlockState*>& blockData)
    {
        // Validate snapshot data
        if (blockData.size() != Chunk::BLOCKS_PER_CHUNK)
        {
            core::LogError("world_storage", "SaveChunkFromSnapshot: Invalid snapshot size %zu, expected %d",
                           blockData.size(), Chunk::BLOCKS_PER_CHUNK);
            return false;
        }

        try
        {
            // Convert BlockState pointers to state IDs
            std::vector<uint32_t> serializedBlocks;
            serializedBlocks.reserve(Chunk::BLOCKS_PER_CHUNK);

            for (size_t i = 0; i < blockData.size(); ++i)
            {
                uint32_t stateID = m_stateMapping.GetStateID(blockData[i]);
                serializedBlocks.push_back(stateID);
            }

            // Serialize StateMapping data
            const size_t         MAX_STATE_MAPPING_SIZE = 64 * 1024;
            std::vector<uint8_t> stateMappingBuffer(MAX_STATE_MAPPING_SIZE);
            size_t               stateMappingSize = m_stateMapping.SerializeStates(stateMappingBuffer.data(), stateMappingBuffer.size());

            if (stateMappingSize == 0)
            {
                core::LogError("world_storage", "Failed to serialize StateMapping for chunk (%d, %d)", chunkX, chunkY);
                return false;
            }

            // Create combined data: [Magic][StateMappingSize][StateMapping][BlockData]
            size_t               totalSize = sizeof(uint32_t) + sizeof(uint32_t) + stateMappingSize + serializedBlocks.size() * sizeof(uint32_t);
            std::vector<uint8_t> combinedData(totalSize);

            size_t offset = 0;

            // Write format magic number
            std::memcpy(combinedData.data() + offset, &ESF_V2_MAGIC, sizeof(uint32_t));
            offset += sizeof(uint32_t);

            // Write StateMapping size
            uint32_t mappingSize32 = static_cast<uint32_t>(stateMappingSize);
            std::memcpy(combinedData.data() + offset, &mappingSize32, sizeof(uint32_t));
            offset += sizeof(uint32_t);

            // Write StateMapping data
            std::memcpy(combinedData.data() + offset, stateMappingBuffer.data(), stateMappingSize);
            offset += stateMappingSize;

            // Write block data
            std::memcpy(combinedData.data() + offset, serializedBlocks.data(), serializedBlocks.size() * sizeof(uint32_t));

            // Save using ESF region files
            ESFError error = ESFError::None;
            try
            {
                int32_t regionX, regionY;
                ESFLayout::WorldChunkToRegion(chunkX, chunkY, regionX, regionY);

                std::string   regionPath = GetWorldSavePath() + "/" + ESFLayout::GenerateRegionFileName(regionX, regionY);
                ESFRegionFile regionFile(regionPath, regionX, regionY);

                if (!regionFile.IsValid())
                {
                    error = regionFile.GetLastError();
                }
                else
                {
                    int32_t localX, localY;
                    ESFLayout::WorldChunkToLocal(chunkX, chunkY, regionX, regionY, localX, localY);
                    error = regionFile.WriteChunk(localX, localY, combinedData.data(), combinedData.size());
                    if (error == ESFError::None)
                    {
                        error = regionFile.Flush();
                    }
                }
            }
            catch (...)
            {
                error = ESFError::FileIOError;
            }

            if (error == ESFError::None)
            {
                core::LogDebug("world_storage", "Chunk snapshot (%d, %d) saved successfully", chunkX, chunkY);
                return true;
            }
            else
            {
                core::LogError("world_storage", "Failed to save chunk snapshot (%d, %d): %s", chunkX, chunkY, ESFErrorToString(error));
                return false;
            }
        }
        catch (const std::exception& e)
        {
            core::LogError("world_storage", "Exception saving chunk snapshot (%d, %d): %s", chunkX, chunkY, e.what());
            return false;
        }
    }

    bool ESFChunkStorage::LoadChunk(int32_t chunkX, int32_t chunkY, void* data)
    {
        // Interface implementation - cast data to Chunk* for ESF-specific method
        if (!data)
            return false;

        Chunk* chunk = static_cast<Chunk*>(data);
        return LoadChunkData(chunk, chunkX, chunkY);
    }

    bool ESFChunkStorage::LoadChunkData(Chunk* chunk, int32_t chunkX, int32_t chunkY)
    {
        if (!chunk)
            return false;

        try
        {
            core::LogDebug("world_storage", "Attempting to load chunk (%d, %d) from ESF storage", chunkX, chunkY);

            // Load chunk data using ESF region files
            // Use larger buffer to accommodate StateMapping + BlockData
            const size_t         MAX_CHUNK_DATA_SIZE = Chunk::BLOCKS_PER_CHUNK * sizeof(uint32_t) + 64 * 1024; // Block data + 64KB for StateMapping
            std::vector<uint8_t> chunkBytes(MAX_CHUNK_DATA_SIZE);
            size_t               bytesRead = 0;
            ESFError             error     = ESFError::None;

            try
            {
                int32_t regionX, regionY;
                ESFLayout::WorldChunkToRegion(chunkX, chunkY, regionX, regionY);
                core::LogDebug("world_storage", "Chunk (%d, %d) maps to region (%d, %d)", chunkX, chunkY, regionX, regionY);

                std::string regionPath = GetWorldSavePath() + "/" + ESFLayout::GenerateRegionFileName(regionX, regionY);
                core::LogDebug("world_storage", "Looking for region file: %s", regionPath.c_str());

                if (!std::filesystem::exists(regionPath))
                {
                    core::LogDebug("world_storage", "Region file does not exist: %s", regionPath.c_str());
                    error = ESFError::ChunkNotFound;
                }
                else
                {
                    ESFRegionFile regionFile(regionPath, regionX, regionY);
                    if (!regionFile.IsValid())
                    {
                        error = regionFile.GetLastError();
                    }
                    else
                    {
                        int32_t localX, localY;
                        ESFLayout::WorldChunkToLocal(chunkX, chunkY, regionX, regionY, localX, localY);
                        error = regionFile.ReadChunk(localX, localY, chunkBytes.data(), chunkBytes.size(), bytesRead);
                    }
                }
            }
            catch (...)
            {
                error = ESFError::FileIOError;
            }

            if (error != ESFError::None)
            {
                if (error == ESFError::ChunkNotFound)
                {
                    core::LogDebug("world_storage", "Chunk (%d, %d) not found in storage", chunkX, chunkY);
                }
                else
                {
                    core::LogError("world_storage", "Failed to load chunk (%d, %d): %s", chunkX, chunkY, ESFErrorToString(error));
                }
                return false;
            }

            // Detect file format and handle backward compatibility
            size_t expectedBlockDataSize = Chunk::BLOCKS_PER_CHUNK * sizeof(uint32_t);

            // Format detection: check for magic number in first 4 bytes
            bool isNewFormat = false;
            if (bytesRead >= sizeof(uint32_t))
            {
                uint32_t magic;
                std::memcpy(&magic, chunkBytes.data(), sizeof(uint32_t));
                isNewFormat = (magic == ESF_V2_MAGIC);
            }

            if (!isNewFormat)
            {
                // Old format detection: check if data size matches old format (only block data)
                bool isOldFormat = (bytesRead == expectedBlockDataSize);

                if (isOldFormat)
                {
                    core::LogInfo("world_storage", "Loading chunk (%d, %d) using old format (no StateMapping): %zu bytes",
                                  chunkX, chunkY, bytesRead);

                    // Old format: [BlockData] only - no StateMapping
                    std::vector<uint32_t> blockData(bytesRead / sizeof(uint32_t));
                    std::memcpy(blockData.data(), chunkBytes.data(), bytesRead);

                    // Clear StateMapping for old format (blocks will be resolved on-the-fly)
                    m_stateMapping.Clear();

                    // For old format, we need to rebuild StateMapping during deserialization
                    // This is a fallback mode - blocks might not be perfectly preserved
                    core::LogWarn("world_storage", "Loading old format file for chunk (%d, %d) - StateMapping will be rebuilt",
                                  chunkX, chunkY);

                    // Deserialize blocks into chunk (this will rebuild StateMapping)
                    bool success = DeserializeChunkBlocks(chunk, blockData);
                    if (success)
                    {
                        core::LogInfo("world_storage", "Chunk (%d, %d) loaded successfully from old format ESF storage", chunkX, chunkY);
                    }
                    else
                    {
                        core::LogError("world_storage", "Failed to deserialize old format chunk (%d, %d)", chunkX, chunkY);
                    }

                    return success;
                }
                else
                {
                    core::LogError("world_storage", "Unknown file format for chunk (%d, %d): %zu bytes (expected %zu for old format)",
                                   chunkX, chunkY, bytesRead, expectedBlockDataSize);
                    return false;
                }
            }
            else
            {
                // New format: [Magic(4 bytes)][StateMappingSize(4 bytes)][StateMapping][BlockData]
                if (bytesRead < sizeof(uint32_t) * 2)
                {
                    core::LogError("world_storage", "New format chunk data too small for chunk (%d, %d): %zu bytes", chunkX, chunkY, bytesRead);
                    return false;
                }

                size_t offset = sizeof(uint32_t); // Skip magic number

                // Read StateMapping size
                uint32_t stateMappingSize;
                std::memcpy(&stateMappingSize, chunkBytes.data() + offset, sizeof(uint32_t));
                offset += sizeof(uint32_t);

                core::LogInfo("world_storage", "Loading chunk (%d, %d) using new format (ESF2): StateMapping size=%u, total data=%zu",
                              chunkX, chunkY, stateMappingSize, bytesRead);

                // Sanity check for StateMapping size (should be reasonable)
                if (stateMappingSize > 64 * 1024) // More than 64KB seems excessive
                {
                    core::LogError("world_storage", "StateMapping size too large for chunk (%d, %d): %u bytes - possibly corrupted file",
                                   chunkX, chunkY, stateMappingSize);
                    return false;
                }

                // Check if we have enough data for StateMapping + BlockData
                if (offset + stateMappingSize + expectedBlockDataSize > bytesRead)
                {
                    core::LogError("world_storage", "New format chunk data corruption for chunk (%d, %d): expected %zu bytes, got %zu",
                                   chunkX, chunkY, offset + stateMappingSize + expectedBlockDataSize, bytesRead);
                    return false;
                }

                // Clear existing StateMapping and deserialize
                m_stateMapping.Clear();
                bool stateMappingSuccess = m_stateMapping.DeserializeStates(chunkBytes.data() + offset, stateMappingSize);
                offset += stateMappingSize;

                if (!stateMappingSuccess)
                {
                    core::LogError("world_storage", "Failed to deserialize StateMapping for chunk (%d, %d)", chunkX, chunkY);
                    return false;
                }

                core::LogInfo("world_storage", "Successfully deserialized StateMapping for chunk (%d, %d): %zu states",
                              chunkX, chunkY, m_stateMapping.GetStateCount());

                // Parse block data
                size_t blockDataSize = bytesRead - offset;
                if (blockDataSize != expectedBlockDataSize)
                {
                    core::LogError("world_storage", "Invalid block data size for chunk (%d, %d): expected %zu, got %zu",
                                   chunkX, chunkY, expectedBlockDataSize, blockDataSize);
                    return false;
                }

                std::vector<uint32_t> blockData(blockDataSize / sizeof(uint32_t));
                std::memcpy(blockData.data(), chunkBytes.data() + offset, blockDataSize);

                // Deserialize blocks into chunk
                bool success = DeserializeChunkBlocks(chunk, blockData);
                if (success)
                {
                    core::LogInfo("world_storage", "Chunk (%d, %d) loaded successfully from new format ESF storage", chunkX, chunkY);
                }
                else
                {
                    core::LogError("world_storage", "Failed to deserialize new format chunk (%d, %d)", chunkX, chunkY);
                }

                return success;
            }
        }
        catch (const std::exception& e)
        {
            core::LogError("world_storage", "Exception loading chunk (%d, %d): %s", chunkX, chunkY, e.what());
            return false;
        }
    }

    bool ESFChunkStorage::ChunkExists(int32_t chunkX, int32_t chunkY) const
    {
        try
        {
            int32_t regionX, regionY;
            ESFLayout::WorldChunkToRegion(chunkX, chunkY, regionX, regionY);

            std::string regionPath = GetWorldSavePath() + "/" + ESFLayout::GenerateRegionFileName(regionX, regionY);
            if (!std::filesystem::exists(regionPath))
                return false;

            ESFRegionFile regionFile(regionPath, regionX, regionY);
            if (!regionFile.IsValid())
                return false;

            int32_t localX, localY;
            ESFLayout::WorldChunkToLocal(chunkX, chunkY, regionX, regionY, localX, localY);
            return regionFile.HasChunk(localX, localY);
        }
        catch (...)
        {
            return false;
        }
    }

    bool ESFChunkStorage::DeleteChunk(int32_t chunkX, int32_t chunkY)
    {
        // ESF format doesn't support individual chunk deletion efficiently
        // This would require rewriting entire region files
        core::LogError("world_storage", "Chunk deletion not supported in ESF format for chunk (%d, %d)", chunkX, chunkY);
        return false;
    }

    void ESFChunkStorage::Flush()
    {
        // ChunkFileManager handles flushing automatically
        core::LogDebug("world_storage", "Flushing chunk storage for world: %s", m_worldPath.c_str());
    }

    void ESFChunkStorage::Close()
    {
        core::LogInfo("world_storage", "Closing chunk storage for world: %s", m_worldPath.c_str());
    }

    size_t ESFChunkStorage::GetLoadedRegionCount() const
    {
        // Simple implementation - always return 1 for now
        // In a full implementation, this would track open region files
        return 1;
    }

    std::string ESFChunkStorage::GetStorageInfo() const
    {
        return Stringf("ESF Storage - World: %s, Path: %s", m_worldPath.c_str(), GetWorldSavePath().c_str());
    }

    std::vector<uint32_t> ESFChunkStorage::SerializeChunkBlocks(Chunk* chunk)
    {
        std::vector<uint32_t> blockData;
        blockData.reserve(Chunk::BLOCKS_PER_CHUNK);

        // Convert chunk blocks to state IDs
        size_t nonAirBlocksFound = 0;
        for (int z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
        {
            for (int y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
            {
                for (int x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
                {
                    BlockState* state   = chunk->GetBlock(x, y, z);
                    uint32_t    stateID = m_stateMapping.GetStateID(state);

                    // Debug: Log first few non-air blocks during serialization
                    if (state != nullptr && nonAirBlocksFound < 5)
                    {
                        core::LogInfo("world_storage", "Serializing non-air block at (%d,%d,%d): state=%p, stateID=%u", x, y, z, (void*)state, stateID);
                        if (stateID == 0xFFFFFFFF || stateID == 0x80000000)
                        {
                            core::LogError("world_storage", "GetStateID returned invalid ID %u for state %p", stateID, (void*)state);
                        }
                        nonAirBlocksFound++;
                    }

                    blockData.push_back(stateID);
                }
            }
        }

        core::LogInfo("world_storage", "Serialization complete: %zu non-air blocks found", nonAirBlocksFound);
        return blockData;
    }

    bool ESFChunkStorage::DeserializeChunkBlocks(Chunk* chunk, const std::vector<uint32_t>& blockData)
    {
        core::LogInfo("world_storage", "Starting deserialization: blockData size=%zu, expected=%d", blockData.size(), Chunk::BLOCKS_PER_CHUNK);

        if (blockData.size() != Chunk::BLOCKS_PER_CHUNK)
        {
            core::LogError("world_storage", "Invalid block data size: %zu, expected: %d", blockData.size(), Chunk::BLOCKS_PER_CHUNK);
            return false;
        }

        size_t index        = 0;
        size_t nonAirBlocks = 0;
        for (int z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
        {
            for (int y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
            {
                for (int x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
                {
                    uint32_t    stateID = blockData[index++];
                    BlockState* state   = m_stateMapping.GetState(stateID);

                    // Debug: Log first few non-air blocks to verify data
                    if (stateID != 0 && nonAirBlocks < 5)
                    {
                        core::LogInfo("world_storage", "Found non-air block at (%d,%d,%d): stateID=%u, state=%p", x, y, z, stateID, (void*)state);
                        if (state == nullptr)
                        {
                            core::LogError("world_storage", "StateMapping returned null for stateID=%u", stateID);
                        }
                        nonAirBlocks++;
                    }

                    chunk->SetBlock(x, y, z, state);
                }
            }
        }

        core::LogInfo("world_storage", "Deserialization complete: %zu non-air blocks found", nonAirBlocks);

        return true;
    }

    std::string ESFChunkStorage::GetWorldSavePath() const
    {
        return m_worldPath + "/region";
    }

    void ESFChunkStorage::EnsureWorldDirectoryExists()
    {
        std::filesystem::create_directories(GetWorldSavePath());
    }

    // ESFChunkSerializer implementation
    bool ESFChunkSerializer::SerializeChunk(const Chunk* chunk, std::vector<uint8_t>& outData)
    {
        // Interface implementation
        if (!chunk)
            return false;

        try
        {
            // Convert chunk blocks to state IDs
            std::vector<uint32_t> blockData;
            blockData.reserve(Chunk::BLOCKS_PER_CHUNK);

            for (int z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
            {
                for (int y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
                {
                    for (int x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
                    {
                        BlockState* state   = const_cast<Chunk*>(chunk)->GetBlock(x, y, z);
                        uint32_t    stateID = m_stateMapping.GetStateID(state);
                        blockData.push_back(stateID);
                    }
                }
            }

            // Convert to binary format (simple memcpy)
            outData.resize(blockData.size() * sizeof(uint32_t));
            std::memcpy(outData.data(), blockData.data(), outData.size());

            return true;
        }
        catch (const std::exception& e)
        {
            core::LogError("esf_serializer", "Failed to serialize chunk: %s", e.what());
            return false;
        }
    }

    bool ESFChunkSerializer::DeserializeChunk(Chunk* chunk, const std::vector<uint8_t>& data)
    {
        // Interface implementation
        if (!chunk || data.empty())
            return false;

        try
        {
            // Validate data size
            if (data.size() != Chunk::BLOCKS_PER_CHUNK * sizeof(uint32_t))
            {
                core::LogError("esf_serializer", "Invalid serialized chunk data size: %zu (expected %zu)",
                               data.size(), Chunk::BLOCKS_PER_CHUNK * sizeof(uint32_t));
                return false;
            }

            // Convert binary data to block IDs
            std::vector<uint32_t> blockData(Chunk::BLOCKS_PER_CHUNK);
            std::memcpy(blockData.data(), data.data(), data.size());

            // Restore chunk blocks from state IDs
            size_t index = 0;
            for (int z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
            {
                for (int y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
                {
                    for (int x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
                    {
                        uint32_t    stateID = blockData[index++];
                        BlockState* state   = m_stateMapping.GetState(stateID);
                        chunk->SetBlock(x, y, z, state);
                    }
                }
            }

            return true;
        }
        catch (const std::exception& e)
        {
            core::LogError("esf_serializer", "Failed to deserialize chunk: %s", e.what());
            return false;
        }
    }

    size_t ESFChunkSerializer::GetSerializedSize(Chunk* chunk) const
    {
        UNUSED(chunk);
        return Chunk::BLOCKS_PER_CHUNK * sizeof(uint32_t);
    }

    bool ESFChunkSerializer::ValidateSerializedData(const std::vector<uint8_t>& data) const
    {
        return data.size() == Chunk::BLOCKS_PER_CHUNK * sizeof(uint32_t);
    }

    // ESFWorldManager implementation
    ESFWorldManager::ESFWorldManager(const std::string& worldPath)
        : m_worldPath(worldPath)
    {
        EnsureWorldDirectoryExists();
    }

    bool ESFWorldManager::SaveWorldInfo(const WorldInfo& info)
    {
        try
        {
            std::string   infoPath = GetWorldInfoPath();
            std::ofstream file(infoPath);

            if (!file.is_open())
            {
                core::LogError("world_manager", "Failed to open world info file for writing: %s", infoPath.c_str());
                return false;
            }

            // Write simple XML format
            file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
            file << "<world>\n";
            file << "  <name>" << info.worldName << "</name>\n";
            file << "  <seed>" << info.worldSeed << "</seed>\n";
            file << "  <version>" << info.worldVersion << "</version>\n";
            file << "  <lastPlayed>" << info.lastPlayed << "</lastPlayed>\n";
            file << "  <spawn x=\"" << info.spawnX << "\" y=\"" << info.spawnY << "\" z=\"" << info.spawnZ << "\"/>\n";
            file << "</world>\n";

            file.close();
            core::LogInfo("world_manager", "World info saved: %s", info.worldName.c_str());
            return true;
        }
        catch (const std::exception& e)
        {
            core::LogError("world_manager", "Exception saving world info: %s", e.what());
            return false;
        }
    }

    bool ESFWorldManager::LoadWorldInfo(WorldInfo& info)
    {
        try
        {
            std::string infoPath = GetWorldInfoPath();
            if (!std::filesystem::exists(infoPath))
            {
                core::LogError("world_manager", "World info file not found: %s", infoPath.c_str());
                return false;
            }

            // Simple parsing - in a full implementation, use proper XML parser
            std::ifstream file(infoPath);
            if (!file.is_open())
            {
                core::LogError("world_manager", "Failed to open world info file: %s", infoPath.c_str());
                return false;
            }

            std::string line;
            while (std::getline(file, line))
            {
                // Very basic XML parsing - in production, use a proper XML library
                if (line.find("<name>") != std::string::npos)
                {
                    size_t start = line.find("<name>") + 6;
                    size_t end   = line.find("</name>");
                    if (end != std::string::npos)
                    {
                        info.worldName = line.substr(start, end - start);
                    }
                }
                // Add parsing for other fields...
            }

            core::LogInfo("world_manager", "World info loaded: %s", info.worldName.c_str());
            return true;
        }
        catch (const std::exception& e)
        {
            core::LogError("world_manager", "Exception loading world info: %s", e.what());
            return false;
        }
    }

    bool ESFWorldManager::WorldExists() const
    {
        return std::filesystem::exists(GetWorldInfoPath());
    }

    bool ESFWorldManager::CreateWorld(const WorldInfo& info)
    {
        EnsureWorldDirectoryExists();
        return SaveWorldInfo(info);
    }

    bool ESFWorldManager::DeleteWorld()
    {
        try
        {
            std::filesystem::remove_all(m_worldPath);
            core::LogInfo("world_manager", "World deleted: %s", m_worldPath.c_str());
            return true;
        }
        catch (const std::exception& e)
        {
            core::LogError("world_manager", "Failed to delete world: %s", e.what());
            return false;
        }
    }

    std::vector<std::string> ESFWorldManager::ListWorlds(const std::string& savesPath)
    {
        std::vector<std::string> worlds;

        try
        {
            if (!std::filesystem::exists(savesPath))
                return worlds;

            for (const auto& entry : std::filesystem::directory_iterator(savesPath))
            {
                if (entry.is_directory())
                {
                    std::string worldInfoPath = entry.path().string() + "/world.xml";
                    if (std::filesystem::exists(worldInfoPath))
                    {
                        worlds.push_back(entry.path().filename().string());
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            core::LogError("world_manager", "Exception listing worlds: %s", e.what());
        }

        return worlds;
    }

    std::string ESFWorldManager::GetWorldInfoPath() const
    {
        return m_worldPath + "/world.xml";
    }

    void ESFWorldManager::EnsureWorldDirectoryExists()
    {
        std::filesystem::create_directories(m_worldPath);
    }

    std::string ESFWorldManager::WorldInfo::ToString() const
    {
        return Stringf("World: %s, Seed: %llu, Version: %d", worldName.c_str(), worldSeed, worldVersion);
    }

    // Cache management methods implementation
    ESFChunkStorage::RegionFileCache* ESFChunkStorage::GetOrCreateRegionFile(const std::string& regionPath) const
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);

        auto it = m_regionCache.find(regionPath);
        if (it != m_regionCache.end())
        {
            // Update access time and return existing cache entry
            it->second->lastAccessTime = static_cast<uint64_t>(std::time(nullptr));
            return it->second.get();
        }

        // Check if we need to evict an entry before adding new one
        if (m_regionCache.size() >= MAX_CACHED_REGIONS)
        {
            EvictLeastRecentlyUsedRegion();
        }

        // Create new cache entry
        auto cache            = std::make_unique<RegionFileCache>();
        cache->regionPath     = regionPath;
        cache->lastAccessTime = static_cast<uint64_t>(std::time(nullptr));

        // Open the file
        cache->fileStream.open(regionPath, std::ios::binary | std::ios::in | std::ios::out);
        if (!cache->fileStream.is_open())
        {
            // Try to create the file if it doesn't exist
            cache->fileStream.open(regionPath, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        }

        RegionFileCache* result   = cache.get();
        m_regionCache[regionPath] = std::move(cache);
        return result;
    }

    void ESFChunkStorage::EvictLeastRecentlyUsedRegion() const
    {
        if (m_regionCache.empty()) return;

        auto     oldestEntry = m_regionCache.begin();
        uint64_t oldestTime  = oldestEntry->second->lastAccessTime;

        for (auto it = m_regionCache.begin(); it != m_regionCache.end(); ++it)
        {
            if (it->second->lastAccessTime < oldestTime)
            {
                oldestTime  = it->second->lastAccessTime;
                oldestEntry = it;
            }
        }

        // Flush any pending writes before closing
        if (oldestEntry->second->isDirty && oldestEntry->second->fileStream.is_open())
        {
            oldestEntry->second->fileStream.flush();
        }

        core::LogDebug(LogChunk, "Evicted region file from cache: %s", oldestEntry->first.c_str());
        m_regionCache.erase(oldestEntry);
    }

    std::string ESFChunkStorage::GetRegionFilePath(int32_t regionX, int32_t regionY) const
    {
        return GetWorldSavePath() + "/region/r." + std::to_string(regionX) + "." + std::to_string(regionY) + ".esf";
    }
}
