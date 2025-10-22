#pragma once
#include "Engine/Core/EngineCommon.hpp"
#include "ESFConfig.hpp"// Unified ESF configuration management
#include <array>
#include <string>

#include "Engine/Core/LogCategory/LogCategory.hpp"

DECLARE_LOG_CATEGORY_EXTERN(LogESF)

namespace enigma::voxel
{
    /**
     * @brief ESF (Enigma Save File) Format Specification
     *
     * Regional file format, each .esf file stores NxN blocks (configured).
     * Designed for main thread synchronization I/O, compatible with Assignment 2 specification.
     *
     * Configuration parameters are centrally managed in ESFConfig.hpp.
     *
     * File structure:
     * [Header] [ChunkIndex] [ChunkData1] [ChunkData2] ... [ChunkDataN]
     */

    /**
     * @brief ESF File Header (64 bytes)
     *
     * Contains file metadata and validation information.
     */
    struct ESFHeader
    {
        uint32_t magicNumber   = ESF_MAGIC_NUMBER; // 0x45534631 ("ESF1")
        uint32_t formatVersion = ESF_FORMAT_VERSION; // Current format version
        int32_t  regionX       = 0; // Region X coordinate
        int32_t  regionY       = 0; // Region Y coordinate
        uint32_t chunkCount    = 0; // Number of chunks stored in this file
        uint32_t fileSize      = 0; // Total file size in bytes
        uint64_t timestamp     = 0; // Last modification timestamp
        uint32_t crc32         = 0; // CRC32 checksum of chunk data
        uint8_t  reserved[28]  = {0}; // Reserved for future use

        // Validation
        bool IsValid() const;
        void UpdateTimestamp();
        void CalculateCRC32(const uint8_t* chunkData, size_t dataSize);
    };

    static_assert(sizeof(ESFHeader) == ESF_HEADER_SIZE, "ESFHeader must be exactly 64 bytes");

    /**
     * @brief Block index entry (8 bytes)
     *
     * Point to the location and size of the block data in the file.
     */
    struct ESFChunkIndexEntry
    {
        uint32_t offset = 0; // Byte offset from the file (0 = block does not exist)
        uint32_t size   = 0; // Compressed block data size (bytes)

        bool IsEmpty() const { return offset == 0 && size == 0; }

        void Clear()
        {
            offset = 0;
            size   = 0;
        }
    };

    static_assert(sizeof(ESFChunkIndexEntry) == ESF_INDEX_ENTRY_SIZE, "ESFChunkIndexEntry size must match config");

    /**
     * @brief block index table (size configuration)
     *
     * Array of ESF_MAX_CHUNKS index entries (ESF_REGION_SIZE × ESF_REGION_SIZE).
     * Blocks of the entry [i] corresponding to the locations in the area (i %ESF_REGION_SIZE, i / ESF_REGION_SIZE).
     */
    using ESFChunkIndex = std::array<ESFChunkIndexEntry, ESF_MAX_CHUNKS>;
    static_assert(sizeof(ESFChunkIndex) == ESF_INDEX_SIZE, "ESFChunkIndex size must match config");

    /**
     * @brief Chunk Data Header (20 bytes)
     *
     * Precedes each chunk's compressed data block.
     */
    struct ESFChunkDataHeader
    {
        int32_t  chunkX           = 0; // World chunk X coordinate
        int32_t  chunkY           = 0; // World chunk Y coordinate
        uint32_t uncompressedSize = 0; // Original block data size
        uint32_t compressedSize   = 0; // Compressed data size
        uint32_t compressionType  = 0; // 0 = RLE, 255 = No compression

        bool IsValid() const;
    };

    static_assert(sizeof(ESFChunkDataHeader) == 20, "ESFChunkDataHeader must be exactly 20 bytes");

    /**
     * @brief ESF File Layout Helper
     *
     * Provides methods for calculating file offsets and validating file structure.
     */
    class ESFLayout
    {
    public:
        // File offset calculations
        static constexpr size_t GetHeaderOffset() { return 0; }
        static constexpr size_t GetIndexOffset() { return ESF_HEADER_SIZE; }
        static constexpr size_t GetDataStartOffset() { return ESF_HEADER_SIZE + ESF_INDEX_SIZE; }

        // Region coordinate conversion
        static void WorldChunkToRegion(int32_t chunkX, int32_t chunkY, int32_t& regionX, int32_t& regionY);
        static void RegionToWorldChunk(int32_t regionX, int32_t regionY, int32_t& startChunkX, int32_t& startChunkY);

        // Chunk index calculation within region
        static size_t ChunkToIndex(int32_t localChunkX, int32_t localChunkY);
        static void   IndexToChunk(size_t index, int32_t& localChunkX, int32_t& localChunkY);

        // Local coordinates within region (0-15)
        static void WorldChunkToLocal(int32_t  chunkX, int32_t  chunkY, int32_t regionX, int32_t regionY,
                                      int32_t& localX, int32_t& localY);

        // File path generation
        static std::string GenerateRegionFileName(int32_t regionX, int32_t regionY);
        static bool        ParseRegionFileName(const std::string& fileName, int32_t& regionX, int32_t& regionY);

        // Validation
        static bool   ValidateFileSize(size_t fileSize, uint32_t chunkCount);
        static size_t CalculateMinFileSize(uint32_t chunkCount);
    };

    // Error codes for ESF operations
    enum class ESFError : uint32_t
    {
        None = 0,
        InvalidMagicNumber,
        UnsupportedVersion,
        CorruptedHeader,
        InvalidChunkIndex,
        CompressionError,
        FileIOError,
        InvalidCoordinates,
        ChunkNotFound,
        CRCMismatch
    };

    // Convert error code to human-readable string
    const char* ESFErrorToString(ESFError error);
}
