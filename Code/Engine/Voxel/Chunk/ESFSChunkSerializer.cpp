#include "ESFSChunkSerializer.hpp"
#include "Chunk.hpp"
#include "../../Registry/Block/BlockRegistry.hpp"
#include "../../Core/Logger/LoggerAPI.hpp"
#include <cstring>

namespace enigma::voxel
{
    using namespace enigma::core;
    using namespace enigma::registry::block;

    //-------------------------------------------------------------------------------------------
    // IChunkSerializer Interface Implementation
    //-------------------------------------------------------------------------------------------

    bool ESFSChunkSerializer::SerializeChunk(const Chunk* chunk, std::vector<uint8_t>& outData)
    {
        if (!chunk)
        {
            LogError("esfs_serializer", "SerializeChunk: chunk is null");
            return false;
        }

        // Step 1: Convert Chunk to block IDs
        std::vector<int32_t> blockIDs;
        if (!SerializeToBlockIDs(chunk, blockIDs))
        {
            LogError("esfs_serializer", "Failed to serialize chunk to block IDs");
            return false;
        }

        // Step 2: RLE compress block IDs
        std::vector<uint8_t> rleData;
        if (!CompressRLE(blockIDs, rleData))
        {
            LogError("esfs_serializer", "Failed to RLE compress block IDs");
            return false;
        }

        // Step 3: Prepend header
        Header header; // Already initialized with correct values
        outData.clear();
        outData.reserve(sizeof(Header) + rleData.size());

        // Write header (8 bytes)
        outData.insert(outData.end(),
                       reinterpret_cast<const uint8_t*>(&header),
                       reinterpret_cast<const uint8_t*>(&header) + sizeof(Header));

        // Write RLE data
        outData.insert(outData.end(), rleData.begin(), rleData.end());

        LogDebug("esfs_serializer", "Serialized chunk to %zu bytes (header 8 + RLE %zu)",
                 outData.size(), rleData.size());

        return true;
    }

    bool ESFSChunkSerializer::DeserializeChunk(Chunk* chunk, const std::vector<uint8_t>& data)
    {
        if (!chunk)
        {
            LogError("esfs_serializer", "DeserializeChunk: chunk is null");
            return false;
        }

        // Step 1: Validate data size (at least header)
        if (data.size() < sizeof(Header))
        {
            LogError("esfs_serializer", "Data too small: %zu bytes (expected at least %zu)",
                     data.size(), sizeof(Header));
            return false;
        }

        // Step 2: Extract and validate header
        Header header;
        std::memcpy(&header, data.data(), sizeof(Header));

        if (!ValidateHeader(header))
        {
            return false;
        }

        // Step 3: Extract RLE data (everything after header)
        std::vector<uint8_t> rleData(data.begin() + sizeof(Header), data.end());

        // Step 4: RLE decompress to block IDs
        std::vector<int32_t> blockIDs;
        if (!DecompressRLE(rleData, blockIDs))
        {
            LogError("esfs_serializer", "Failed to RLE decompress data");
            return false;
        }

        // Step 5: Convert block IDs back to Chunk
        if (!DeserializeFromBlockIDs(chunk, blockIDs))
        {
            LogError("esfs_serializer", "Failed to deserialize block IDs to chunk");
            return false;
        }

        LogDebug("esfs_serializer", "Deserialized chunk from %zu bytes", data.size());
        return true;
    }

    //-------------------------------------------------------------------------------------------
    // Helper Methods - Block ID Conversion
    //-------------------------------------------------------------------------------------------

    bool ESFSChunkSerializer::SerializeToBlockIDs(const Chunk* chunk, std::vector<int32_t>& outBlockIDs)
    {
        if (!chunk)
        {
            LogError("esfs_serializer", "SerializeToBlockIDs: chunk is null");
            return false;
        }

        // Allocate block ID array (16 * 16 * 128 = 32768)
        outBlockIDs.resize(Chunk::BLOCKS_PER_CHUNK);

        // Convert BlockState pointers to Block IDs
        for (int32_t z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
        {
            for (int32_t y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
            {
                for (int32_t x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
                {
                    // Get block state from chunk
                    BlockState* state = const_cast<Chunk*>(chunk)->GetBlock(x, y, z);

                    size_t index = Chunk::CoordsToIndex(x, y, z);

                    if (!state)
                    {
                        // Null state -> Air block (ID = 0 assumed)
                        outBlockIDs[index] = 0;
                        continue;
                    }

                    // Get Block from BlockState
                    auto* block = state->GetBlock();
                    if (!block)
                    {
                        // No block -> Air (ID = 0)
                        outBlockIDs[index] = 0;
                        continue;
                    }

                    // Get block ID directly from Block object (O(1) vs O(log n))
                    int32_t blockId = block->GetNumericId();

                    if (blockId < 0)
                    {
                        // Block not registered -> fallback to Air
                        LogWarn("esfs_serializer", "Block '%s' not registered (no numeric ID) at (%d, %d, %d), using Air",
                                block->GetRegistryKey().c_str(), x, y, z);
                        outBlockIDs[index] = 0;
                        continue;
                    }

                    outBlockIDs[index] = blockId;
                }
            }
        }

        return true;
    }

    bool ESFSChunkSerializer::DeserializeFromBlockIDs(Chunk* chunk, const std::vector<int32_t>& blockIDs)
    {
        if (!chunk)
        {
            LogError("esfs_serializer", "DeserializeFromBlockIDs: chunk is null");
            return false;
        }

        if (blockIDs.size() != Chunk::BLOCKS_PER_CHUNK)
        {
            LogError("esfs_serializer", "DeserializeFromBlockIDs: invalid block ID count %zu (expected %d)",
                     blockIDs.size(), Chunk::BLOCKS_PER_CHUNK);
            return false;
        }

        // Convert Block IDs back to BlockState pointers
        for (int32_t z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
        {
            for (int32_t y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
            {
                for (int32_t x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
                {
                    size_t  index   = Chunk::CoordsToIndex(x, y, z);
                    int32_t blockId = blockIDs[index];

                    // Get Block from BlockRegistry by ID
                    auto block = BlockRegistry::GetBlockById(blockId);

                    if (!block)
                    {
                        // Unknown block ID -> fallback to Air
                        LogWarn("esfs_serializer", "Unknown block ID %d at (%d, %d, %d), using Air", blockId, x, y, z);
                        block = BlockRegistry::GetBlock("air");

                        if (!block)
                        {
                            LogError("esfs_serializer", "Critical: Air block not registered!");
                            return false;
                        }
                    }

                    // Get default BlockState for this Block
                    BlockState* state = block->GetDefaultState();

                    if (!state)
                    {
                        LogError("esfs_serializer", "Block '%s' has no default state", block->GetRegistryKey().c_str());
                        return false;
                    }

                    // Set block in chunk (using world generation method - no modify flag)
                    chunk->SetBlock(x, y, z, state);
                }
            }
        }

        return true;
    }

    //-------------------------------------------------------------------------------------------
    // RLE Compression/Decompression
    //-------------------------------------------------------------------------------------------

    bool ESFSChunkSerializer::CompressRLE(const std::vector<int32_t>& blockIDs, std::vector<uint8_t>& outRLE)
    {
        if (blockIDs.size() != 32768)
        {
            LogError("esfs_serializer", "Invalid block data size: %zu (expected 32768)", blockIDs.size());
            return false;
        }

        // Validate all block IDs are in range [0, 255]
        for (size_t i = 0; i < blockIDs.size(); ++i)
        {
            if (blockIDs[i] < 0 || blockIDs[i] > 255)
            {
                LogError("esfs_serializer", "Block ID out of range at index %zu: %d (expected 0-255)", i, blockIDs[i]);
                return false;
            }
        }

        // Clear output
        outRLE.clear();
        outRLE.reserve(32768 * 2); // Worst case: every block is different

        // RLE encoding: [BlockType (1 byte)][RunLength (1 byte)]
        size_t i = 0;
        while (i < blockIDs.size())
        {
            uint8_t blockType = static_cast<uint8_t>(blockIDs[i]);
            uint8_t runLength = 1;

            // Count consecutive identical blocks (max 255 per run)
            while (i + runLength < blockIDs.size() &&
                blockIDs[i + runLength] == blockType &&
                runLength < 255)
            {
                ++runLength;
            }

            // Write run: [BlockType][RunLength]
            outRLE.push_back(blockType);
            outRLE.push_back(runLength);

            i += runLength;
        }

        LogDebug("esfs_serializer", "RLE compressed %zu blocks -> %zu bytes (%.1f%% of raw)",
                 blockIDs.size(), outRLE.size(),
                 100.0f * static_cast<float>(outRLE.size()) / static_cast<float>(blockIDs.size() * sizeof(int32_t)));

        return true;
    }

    bool ESFSChunkSerializer::DecompressRLE(const std::vector<uint8_t>& rleData, std::vector<int32_t>& outBlockIDs)
    {
        // Validate RLE data size (must be even: pairs of [BlockType][RunLength])
        if (rleData.size() % 2 != 0)
        {
            LogError("esfs_serializer", "Invalid RLE data size: %zu (must be even)", rleData.size());
            return false;
        }

        // Clear output and reserve space
        outBlockIDs.clear();
        outBlockIDs.reserve(32768);

        // Decode RLE runs
        size_t totalBlocks = 0;
        for (size_t i = 0; i < rleData.size(); i += 2)
        {
            uint8_t blockType = rleData[i];
            uint8_t runLength = rleData[i + 1];

            // Validate run length
            if (runLength == 0)
            {
                LogError("esfs_serializer", "Invalid RLE run length: 0 at byte offset %zu", i);
                outBlockIDs.clear();
                return false;
            }

            // Expand run
            for (uint8_t j = 0; j < runLength; ++j)
            {
                outBlockIDs.push_back(static_cast<int32_t>(blockType));
            }

            totalBlocks += runLength;

            // Safety check: prevent infinite decompression
            if (totalBlocks > 32768)
            {
                LogError("esfs_serializer", "RLE decompression exceeded block count: %zu (max 32768)", totalBlocks);
                outBlockIDs.clear();
                return false;
            }
        }

        // Validate total block count
        if (outBlockIDs.size() != 32768)
        {
            LogError("esfs_serializer", "RLE decompression resulted in %zu blocks (expected 32768)", outBlockIDs.size());
            outBlockIDs.clear();
            return false;
        }

        LogDebug("esfs_serializer", "RLE decompressed %zu bytes -> %zu blocks", rleData.size(), outBlockIDs.size());
        return true;
    }

    //-------------------------------------------------------------------------------------------
    // Header Validation
    //-------------------------------------------------------------------------------------------

    bool ESFSChunkSerializer::ValidateHeader(const Header& header)
    {
        // Check magic number "ESFS"
        if (std::memcmp(header.magic, "ESFS", 4) != 0)
        {
            LogError("esfs_serializer", "Invalid magic number in header (expected 'ESFS')");
            return false;
        }

        // Check version
        if (header.version != 1)
        {
            LogError("esfs_serializer", "Unsupported ESFS version: %u (expected 1)", header.version);
            return false;
        }

        // Check chunk bits (Assignment 02: 4, 4, 7)
        if (header.chunkBitsX != 4 || header.chunkBitsY != 4 || header.chunkBitsZ != 7)
        {
            LogError("esfs_serializer", "Invalid chunk bits: (%u, %u, %u) (expected 4, 4, 7)",
                     header.chunkBitsX, header.chunkBitsY, header.chunkBitsZ);
            return false;
        }

        return true;
    }
} // namespace enigma::voxel
