#include "ESFSFile.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Core/FileUtils.hpp"
#include <fstream>
#include <filesystem>
#include <cstring>

namespace enigma::voxel
{
    using namespace enigma::core;

    //-------------------------------------------------------------------------------------------
    // File Path Utilities
    //-------------------------------------------------------------------------------------------
    std::string ESFSFile::GetChunkFilePath(const std::string& worldPath, int32_t chunkX, int32_t chunkY)
    {
        // Format: {worldPath}/region/chunk_{X}_{Y}.esfs (matching ESF region directory)
        std::string regionDir = worldPath + "/region";
        std::string filename  = "chunk_" + std::to_string(chunkX) + "_" + std::to_string(chunkY) + ".esfs";
        return regionDir + "/" + filename;
    }

    bool ESFSFile::EnsureRegionDirectory(const std::string& worldPath)
    {
        std::string regionDir = worldPath + "/region";

        try
        {
            if (!std::filesystem::exists(regionDir))
            {
                std::filesystem::create_directories(regionDir);
                LogDebug("esfs", "Created region directory: %s", regionDir.c_str());
            }
            return true;
        }
        catch (const std::exception& e)
        {
            LogError("esfs", "Failed to create region directory '%s': %s", regionDir.c_str(), e.what());
            return false;
        }
    }

    //-------------------------------------------------------------------------------------------
    // Header Validation
    //-------------------------------------------------------------------------------------------
    bool ESFSFile::ValidateHeader(const Header& header)
    {
        // Check magic number "ESFS"
        if (std::memcmp(header.magic, "ESFS", 4) != 0)
        {
            LogError("esfs", "Invalid magic number in header (expected 'ESFS')");
            return false;
        }

        // Check version
        if (header.version != 1)
        {
            LogError("esfs", "Unsupported ESFS version: %u (expected 1)", header.version);
            return false;
        }

        // Check chunk bits (Assignment 02: 4, 4, 7)
        if (header.chunkBitsX != 4 || header.chunkBitsY != 4 || header.chunkBitsZ != 7)
        {
            LogError("esfs", "Invalid chunk bits: (%u, %u, %u) (expected 4, 4, 7)",
                     header.chunkBitsX, header.chunkBitsY, header.chunkBitsZ);
            return false;
        }

        return true;
    }

    //-------------------------------------------------------------------------------------------
    // RLE Compression (Assignment 02 Format)
    //-------------------------------------------------------------------------------------------
    bool ESFSFile::CompressRLE(const std::vector<int32_t>& blockIDs, std::vector<uint8_t>& outRLE)
    {
        if (blockIDs.size() != 32768)
        {
            LogError("esfs", "Invalid block data size: %zu (expected 32768)", blockIDs.size());
            return false;
        }

        // Validate all block IDs are in range [0, 255]
        for (size_t i = 0; i < blockIDs.size(); ++i)
        {
            if (blockIDs[i] < 0 || blockIDs[i] > 255)
            {
                LogError("esfs", "Block ID out of range at index %zu: %d (expected 0-255)", i, blockIDs[i]);
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

        LogDebug("esfs", "RLE compressed %zu blocks -> %zu bytes (%.1f%% of raw)",
                 blockIDs.size(), outRLE.size(),
                 100.0f * static_cast<float>(outRLE.size()) / static_cast<float>(blockIDs.size() * sizeof(int32_t)));

        return true;
    }

    //-------------------------------------------------------------------------------------------
    // RLE Decompression (Assignment 02 Format)
    //-------------------------------------------------------------------------------------------
    bool ESFSFile::DecompressRLE(const std::vector<uint8_t>& rleData, std::vector<int32_t>& outBlockIDs)
    {
        // Validate RLE data size (must be even: pairs of [BlockType][RunLength])
        if (rleData.size() % 2 != 0)
        {
            LogError("esfs", "Invalid RLE data size: %zu (must be even)", rleData.size());
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
                LogError("esfs", "Invalid RLE run length: 0 at byte offset %zu", i);
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
                LogError("esfs", "RLE decompression exceeded block count: %zu (max 32768)", totalBlocks);
                outBlockIDs.clear();
                return false;
            }
        }

        // Validate total block count
        if (outBlockIDs.size() != 32768)
        {
            LogError("esfs", "RLE decompression resulted in %zu blocks (expected 32768)", outBlockIDs.size());
            outBlockIDs.clear();
            return false;
        }

        LogDebug("esfs", "RLE decompressed %zu bytes -> %zu blocks", rleData.size(), outBlockIDs.size());
        return true;
    }

    //-------------------------------------------------------------------------------------------
    // Save Chunk (RLE Compression)
    //-------------------------------------------------------------------------------------------
    bool ESFSFile::SaveChunk(
        const std::string&          worldPath,
        int32_t                     chunkX, int32_t chunkY,
        const std::vector<int32_t>& blockIDs)
    {
        // Validate input
        if (blockIDs.size() != 32768)
        {
            LogError("esfs", "Invalid block data size: %zu (expected 32768)", blockIDs.size());
            return false;
        }

        // Ensure directory exists
        if (!EnsureRegionDirectory(worldPath))
        {
            return false;
        }

        // Prepare header (8 bytes)
        Header header;
        // magic, version, chunkBits already initialized in struct

        // RLE compress block data
        std::vector<uint8_t> rleData;
        if (!CompressRLE(blockIDs, rleData))
        {
            return false;
        }

        // Open file for writing
        std::string   filePath = GetChunkFilePath(worldPath, chunkX, chunkY);
        std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
        {
            LogError("esfs", "Failed to open file for writing: %s", filePath.c_str());
            return false;
        }

        // Write header (8 bytes)
        file.write(reinterpret_cast<const char*>(&header), sizeof(Header));

        // Write RLE-compressed block data
        file.write(reinterpret_cast<const char*>(rleData.data()), rleData.size());

        file.close();

        LogDebug("esfs", "Saved chunk (%d, %d) to %s (%zu bytes total)",
                 chunkX, chunkY, filePath.c_str(),
                 sizeof(Header) + rleData.size());

        return true;
    }

    //-------------------------------------------------------------------------------------------
    // Load Chunk (RLE Decompression)
    //-------------------------------------------------------------------------------------------
    bool ESFSFile::LoadChunk(
        const std::string&    worldPath,
        int32_t               chunkX, int32_t chunkY,
        std::vector<int32_t>& outBlockIDs)
    {
        // Get file path
        std::string filePath = GetChunkFilePath(worldPath, chunkX, chunkY);

        // Check if file exists
        if (!std::filesystem::exists(filePath))
        {
            LogDebug("esfs", "Chunk file does not exist: %s", filePath.c_str());
            return false;
        }

        // Open file for reading
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open())
        {
            LogError("esfs", "Failed to open file for reading: %s", filePath.c_str());
            return false;
        }

        // Read header (8 bytes)
        Header header;
        file.read(reinterpret_cast<char*>(&header), sizeof(Header));
        if (!file.good())
        {
            LogError("esfs", "Failed to read header from: %s", filePath.c_str());
            return false;
        }

        // Validate header
        if (!ValidateHeader(header))
        {
            return false;
        }

        // Read RLE-compressed block data (rest of file)
        size_t fileSize    = std::filesystem::file_size(filePath);
        size_t rleDataSize = fileSize - sizeof(Header);

        std::vector<uint8_t> rleData(rleDataSize);
        file.read(reinterpret_cast<char*>(rleData.data()), rleDataSize);
        if (!file.good())
        {
            LogError("esfs", "Failed to read RLE data from: %s", filePath.c_str());
            return false;
        }

        file.close();

        // RLE decompress block data
        if (!DecompressRLE(rleData, outBlockIDs))
        {
            return false;
        }

        LogDebug("esfs", "Loaded chunk (%d, %d) from %s", chunkX, chunkY, filePath.c_str());
        return true;
    }

    //-------------------------------------------------------------------------------------------
    // Chunk Existence Check
    //-------------------------------------------------------------------------------------------
    bool ESFSFile::ChunkExists(const std::string& worldPath, int32_t chunkX, int32_t chunkY)
    {
        std::string filePath = GetChunkFilePath(worldPath, chunkX, chunkY);
        return std::filesystem::exists(filePath);
    }

    //-------------------------------------------------------------------------------------------
    // Delete Chunk
    //-------------------------------------------------------------------------------------------
    bool ESFSFile::DeleteChunk(const std::string& worldPath, int32_t chunkX, int32_t chunkY)
    {
        std::string filePath = GetChunkFilePath(worldPath, chunkX, chunkY);

        if (!std::filesystem::exists(filePath))
        {
            LogDebug("esfs", "Chunk file does not exist, nothing to delete: %s", filePath.c_str());
            return true;
        }

        try
        {
            std::filesystem::remove(filePath);
            LogDebug("esfs", "Deleted chunk file: %s", filePath.c_str());
            return true;
        }
        catch (const std::exception& e)
        {
            LogError("esfs", "Failed to delete chunk file '%s': %s", filePath.c_str(), e.what());
            return false;
        }
    }
} // namespace enigma::voxel
