#include "ESFFormat.hpp"
#include "Chunk.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include <chrono>
#include <cstring>
// TODO: Add CRC32 implementation when available
// #include <crc32.h>
DEFINE_LOG_CATEGORY(LogESF)

namespace enigma::voxel
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
        using namespace enigma::core;

        // Basic validation checks
        bool isValid = uncompressedSize > 0 &&
            compressedSize > 0 &&
            (compressionType == 0 || compressionType == 255); // RLE or no compression

        if (!isValid)
        {
            LogError("chunk_header", "ESFChunkDataHeader validation failed: uncompressedSize=%u, compressedSize=%u, compressionType=%u",
                     uncompressedSize, compressedSize, compressionType);
            ERROR_AND_DIE(Stringf("ESFChunkDataHeader validation failed: uncompressedSize=%u, compressedSize=%u, compressionType=%u",
                uncompressedSize, compressedSize, compressionType))
        }

        // Use configuration-based reasonable size limit for backward compatibility
        // This handles different ESF configurations and worst-case scenarios
        if (uncompressedSize > ESF_MAX_REASONABLE_CHUNK_SIZE)
        {
            LogError("chunk_header", "ESFChunkDataHeader uncompressedSize too large: %u > %zu (ESF_MAX_REASONABLE_CHUNK_SIZE)",
                     uncompressedSize, ESF_MAX_REASONABLE_CHUNK_SIZE);
            ERROR_AND_DIE(Stringf("ESFChunkDataHeader uncompressedSize too large: %u > %zu (ESF_MAX_REASONABLE_CHUNK_SIZE)", uncompressedSize, ESF_MAX_REASONABLE_CHUNK_SIZE))
        }

        return true;
    }

    // ESFLayout implementation
    void ESFLayout::WorldChunkToRegion(int32_t chunkX, int32_t chunkY, int32_t& regionX, int32_t& regionY)
    {
        // Convert world chunk coordinates to region coordinates
        // Each area contains ESF_REGION_SIZE × ESF_REGION_SIZE blocks

        // Positive coordinates: chunkX / ESF_REGION_SIZE
        // Negative coordinates: (chunkX + 1) / ESF_REGION_SIZE - 1 Ensure correct downward rounding
        if (chunkX >= 0)
        {
            regionX = chunkX >> ESF_REGION_SHIFT; // Equivalent to chunkX / ESF_REGION_SIZE
        }
        else
        {
            regionX = (chunkX + 1) / static_cast<int32_t>(ESF_REGION_SIZE) - 1;
        }

        if (chunkY >= 0)
        {
            regionY = chunkY >> ESF_REGION_SHIFT; // Equivalent to chunkY / ESF_REGION_SIZE
        }
        else
        {
            regionY = (chunkY + 1) / static_cast<int32_t>(ESF_REGION_SIZE) - 1;
        }
    }

    void ESFLayout::RegionToWorldChunk(int32_t regionX, int32_t regionY, int32_t& startChunkX, int32_t& startChunkY)
    {
        // Convert region coordinates to the region's starting chunk coordinates
        startChunkX = regionX << ESF_REGION_SHIFT; // Equivalent to regionX * ESF_REGION_SIZE
        startChunkY = regionY << ESF_REGION_SHIFT; // Equivalent to regionY * ESF_REGION_SIZE
    }

    size_t ESFLayout::ChunkToIndex(int32_t localChunkX, int32_t localChunkY)
    {
        // Convert the local chunk coordinates (0 to ESF_REGION_SIZE-1) to the array index (0 to ESF_MAX_CHUNKS-1)
        ASSERT_OR_DIE(localChunkX >= 0 && localChunkX < ESF_REGION_SIZE, "localChunkX out of range");
        ASSERT_OR_DIE(localChunkY >= 0 && localChunkY < ESF_REGION_SIZE, "localChunkY out of range");

        return static_cast<size_t>(localChunkY * ESF_REGION_SIZE + localChunkX);
    }

    void ESFLayout::IndexToChunk(size_t index, int32_t& localChunkX, int32_t& localChunkY)
    {
        // Convert array index (0 to ESF_MAX_CHUNKS-1) to local chunk coordinates (0 to ESF_REGION_SIZE-1)
        ASSERT_OR_DIE(index < ESF_MAX_CHUNKS, "Index out of range")

        localChunkX = static_cast<int32_t>(index % ESF_REGION_SIZE);
        localChunkY = static_cast<int32_t>(index / ESF_REGION_SIZE);
    }

    void ESFLayout::WorldChunkToLocal(int32_t  chunkX, int32_t  chunkY, int32_t regionX, int32_t regionY,
                                      int32_t& localX, int32_t& localY)
    {
        // Convert world chunk coordinates to local coordinates within the region
        int32_t regionStartX, regionStartY;
        RegionToWorldChunk(regionX, regionY, regionStartX, regionStartY);

        localX = chunkX - regionStartX;
        localY = chunkY - regionStartY;

        // Debugging output for coordinate conversion problems
        if (localX < 0 || localX >= ESF_REGION_SIZE || localY < 0 || localY >= ESF_REGION_SIZE)
        {
            core::LogError(LogESF, Stringf("Coordinate conversion error: chunkX=%d, chunkY=%d, regionX=%d, regionY=%d, regionStartX=%d, regionStartY=%d, localX=%d, localY=%d",
                                           chunkX, chunkY, regionX, regionY, regionStartX, regionStartY, localX, localY).c_str());
        }

        // Verify that the local coordinates are within the region boundary
        ASSERT_OR_DIE(localX >= 0 && localX < ESF_REGION_SIZE, "localX out of region bounds")
        ASSERT_OR_DIE(localY >= 0 && localY < ESF_REGION_SIZE, "localY out of region bounds")
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
