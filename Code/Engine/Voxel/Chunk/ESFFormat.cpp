#include "ESFFormat.hpp"
#include "Chunk.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include <chrono>
#include <cstring>
// TODO: Add CRC32 implementation when available
// #include <crc32.h>

namespace enigma::voxel::chunk
{
    // ESFHeader implementation
    bool ESFHeader::IsValid() const
    {
        return magicNumber == ESF_MAGIC_NUMBER &&
            formatVersion == ESF_FORMAT_VERSION &&
            chunkCount <= ESF_MAX_CHUNKS;
    }

    void ESFHeader::UpdateTimestamp()
    {
        auto now  = std::chrono::system_clock::now();
        timestamp = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count());
    }

    void ESFHeader::CalculateCRC32(const uint8_t* chunkData, size_t dataSize)
    {
        // TODO: Implement CRC32 calculation when engine CRC32 utility is available
        // For now, use a simple checksum as placeholder
        crc32 = 0;
        if (chunkData && dataSize > 0)
        {
            for (size_t i = 0; i < dataSize; ++i)
            {
                crc32 = (crc32 * 31) + chunkData[i]; // Simple polynomial checksum
            }
        }
    }

    // ESFChunkDataHeader implementation
    bool ESFChunkDataHeader::IsValid() const
    {
        return uncompressedSize > 0 &&
            uncompressedSize <= (Chunk::BLOCKS_PER_CHUNK * sizeof(uint32_t)) && // Reasonable size limit
            compressedSize > 0 &&
            (compressionType == 0 || compressionType == 255); // RLE or no compression
    }

    // ESFLayout implementation
    void ESFLayout::WorldChunkToRegion(int32_t chunkX, int32_t chunkY, int32_t& regionX, int32_t& regionY)
    {
        // Convert world chunk coordinates to region coordinates
        // Each region contains 16x16 chunks

        // For positive coordinates: chunkX / 16
        // For negative coordinates: (chunkX + 1) / 16 - 1 to ensure proper floor division
        if (chunkX >= 0)
        {
            regionX = chunkX >> 4; // chunkX / 16
        }
        else
        {
            regionX = (chunkX + 1) / 16 - 1;
        }

        if (chunkY >= 0)
        {
            regionY = chunkY >> 4; // chunkY / 16
        }
        else
        {
            regionY = (chunkY + 1) / 16 - 1;
        }
    }

    void ESFLayout::RegionToWorldChunk(int32_t regionX, int32_t regionY, int32_t& startChunkX, int32_t& startChunkY)
    {
        // Convert region coordinates to the starting chunk coordinates of that region
        startChunkX = regionX << 4; // regionX * 16
        startChunkY = regionY << 4; // regionY * 16
    }

    size_t ESFLayout::ChunkToIndex(int32_t localChunkX, int32_t localChunkY)
    {
        // Convert local chunk coordinates (0-15) to array index (0-255)
        ASSERT_OR_DIE(localChunkX >= 0 && localChunkX < ESF_REGION_SIZE, "localChunkX out of range");
        ASSERT_OR_DIE(localChunkY >= 0 && localChunkY < ESF_REGION_SIZE, "localChunkY out of range");

        return static_cast<size_t>(localChunkY * ESF_REGION_SIZE + localChunkX);
    }

    void ESFLayout::IndexToChunk(size_t index, int32_t& localChunkX, int32_t& localChunkY)
    {
        // Convert array index (0-255) to local chunk coordinates (0-15)
        ASSERT_OR_DIE(index < ESF_MAX_CHUNKS, "Index out of range");

        localChunkX = static_cast<int32_t>(index % ESF_REGION_SIZE);
        localChunkY = static_cast<int32_t>(index / ESF_REGION_SIZE);
    }

    void ESFLayout::WorldChunkToLocal(int32_t  chunkX, int32_t  chunkY, int32_t regionX, int32_t regionY,
                                      int32_t& localX, int32_t& localY)
    {
        // Convert world chunk coordinates to local coordinates within region
        int32_t regionStartX, regionStartY;
        RegionToWorldChunk(regionX, regionY, regionStartX, regionStartY);

        localX = chunkX - regionStartX;
        localY = chunkY - regionStartY;

        // Debug output for coordinate conversion issues
        if (localX < 0 || localX >= ESF_REGION_SIZE || localY < 0 || localY >= ESF_REGION_SIZE)
        {
            core::LogError("chunk", Stringf("Coordinate conversion error: chunkX=%d, chunkY=%d, regionX=%d, regionY=%d, regionStartX=%d, regionStartY=%d, localX=%d, localY=%d",
                                            chunkX, chunkY, regionX, regionY, regionStartX, regionStartY, localX, localY));
        }

        // Validate local coordinates are within region bounds
        ASSERT_OR_DIE(localX >= 0 && localX < ESF_REGION_SIZE, "localX out of region bounds");
        ASSERT_OR_DIE(localY >= 0 && localY < ESF_REGION_SIZE, "localY out of region bounds");
    }

    std::string ESFLayout::GenerateRegionFileName(int32_t regionX, int32_t regionY)
    {
        // Generate filename in format: "r.X.Y.esf"
        // Examples: "r.0.0.esf", "r.-1.2.esf"
        return Stringf("r.%d.%d.esf", regionX, regionY);
    }

    bool ESFLayout::ParseRegionFileName(const std::string& fileName, int32_t& regionX, int32_t& regionY)
    {
        // Parse filename format: "r.X.Y.esf"
        if (fileName.size() < 7) return false; // Minimum: "r.0.0.esf"

        if (fileName.substr(0, 2) != "r." || fileName.substr(fileName.size() - 4) != ".esf")
            return false;

        // Extract the middle part between "r." and ".esf"
        std::string coords = fileName.substr(2, fileName.size() - 6);

        // Find the separator dot
        size_t dotPos = coords.find('.');
        if (dotPos == std::string::npos) return false;

        try
        {
            std::string xStr = coords.substr(0, dotPos);
            std::string yStr = coords.substr(dotPos + 1);

            regionX = std::stoi(xStr);
            regionY = std::stoi(yStr);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool ESFLayout::ValidateFileSize(size_t fileSize, uint32_t chunkCount)
    {
        // Minimum file size: header + index + at least one chunk data header per chunk
        size_t minSize = ESF_HEADER_SIZE + ESF_INDEX_SIZE + (chunkCount * sizeof(ESFChunkDataHeader));
        return fileSize >= minSize;
    }

    size_t ESFLayout::CalculateMinFileSize(uint32_t chunkCount)
    {
        // Calculate minimum required file size for given chunk count
        return ESF_HEADER_SIZE + ESF_INDEX_SIZE + (chunkCount * sizeof(ESFChunkDataHeader));
    }

    // Error code to string conversion
    const char* ESFErrorToString(ESFError error)
    {
        switch (error)
        {
        case ESFError::None: return "No error";
        case ESFError::InvalidMagicNumber: return "Invalid magic number";
        case ESFError::UnsupportedVersion: return "Unsupported file format version";
        case ESFError::CorruptedHeader: return "Corrupted file header";
        case ESFError::InvalidChunkIndex: return "Invalid chunk index";
        case ESFError::CompressionError: return "Compression/decompression error";
        case ESFError::FileIOError: return "File I/O error";
        case ESFError::InvalidCoordinates: return "Invalid chunk coordinates";
        case ESFError::ChunkNotFound: return "Chunk not found in region";
        case ESFError::CRCMismatch: return "CRC checksum mismatch";
        default: return "Unknown error";
        }
    }
}
