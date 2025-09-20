#pragma once
#include <cstdint>

// ========================================================================
// Configuration Presets Guide
// ========================================================================

/*
Configuration Guide:

To modify the area size, directly modify the ESF_REGION_SIZE value:

1. Small World: ESF_REGION_SIZE = 8
   - Advantages: Small memory usage, fast loading
   - Disadvantages: Large number of files, suitable for <1GB world

2. Standard configuration: ESF_REGION_SIZE = 16 [original default]
   -Balanced performance and memory usage
   - Suitable for most application scenarios

3. Large World: ESF_REGION_SIZE = 32 [Current Settings]
   - Advantages: Small number of files and high I/O efficiency
   - Disadvantages: Large memory usage, suitable for >4GB RAM

4. Extreme performance: ESF_REGION_SIZE = 64
   - Suitable for professional servers and high-end hardware
   - Requires a lot of RAM and high-speed storage

Important reminder:
- The entire project must be recompiled after modification
- All existing archives will not be read
- Need to delete old archived data
*/

namespace enigma::voxel::chunk
{
    /**
     * @file ESFConfig.hpp
     * @brief ESF (Enigma Save File) Format Configuration
     *
     * Centrally manage all ESF format-related configuration parameters.
     * Modify these parameters and recompile to apply new settings.
     *
     * Important: Modifying these parameters will cause incompatible with the old archive format!
     */

    // ========================================================================
    // Core Layout Configuration
    // ========================================================================

    /**
     * @brief The number of blocks contained in each zone file (single sided)
     *
     * Default value: 16 (generate 16x16=256 block per area file)
     * Recommended values: 8, 16, 32, 64
     *
     * Influence:
     * - Region file size = REGION_SIZE² × Average block size
     * - Memory usage: Larger areas require more RAM
     * - I/O performance: Larger area reduces file count but increases single I/O size
     */
    static constexpr size_t ESF_REGION_SIZE = 32; // After modification: 32x32 = 1024 chunks per region

    /**
     * @brief The displacement value of the area size (for fast division/multiple)
     *
     * Automatic calculation: log2(ESF_REGION_SIZE)
     * For optimization: chunkX >> ESF_REGION_SHIFT is equivalent to chunkX / ESF_REGION_SIZE
     */
    static constexpr int ESF_REGION_SHIFT = []() constexpr
    {
        int    shift = 0;
        size_t temp  = ESF_REGION_SIZE;
        while (temp > 1)
        {
            temp >>= 1;
            shift++;
        }
        return shift;
    }();

    /**
     * @brief The maximum number of blocks per region file
     *
     * Automatic calculation: REGION_SIZE × REGION_SIZE
     */
    static constexpr size_t ESF_MAX_CHUNKS = ESF_REGION_SIZE * ESF_REGION_SIZE;

    // ========================================================================
    // File Format Configuration
    // ========================================================================

    /**
     * @brief ESF file format version number
     *
     * Version History:
     * - v1: Basic format, only supports BlockData
     * - v2: Add StateMapping support
     */
    static constexpr uint32_t ESF_FORMAT_VERSION = 1;

    /**
     * @brief ESF file magic number identification
     *
     * v1 format: 0x45534631 ("ESF1")
     * v2 format: 0x45534632 ("ESF2")
     */
    static constexpr uint32_t ESF_MAGIC_NUMBER = 0x45534631; // "ESF1"
    static constexpr uint32_t ESF_V2_MAGIC     = 0x45534632; // "ESF2"

    // ========================================================================
    // File Structure Configuration
    // ========================================================================

    /**
     * @brief ESF file header size (bytes)
     *
     * Includes: magic number, version, coordinates, timestamps, CRC32 and other metadata
     */
    static constexpr size_t ESF_HEADER_SIZE = 64;

    /**
     * @brief Block index entry size (bytes)
     *
     * Each entry contains: offset(4) + size(4) = 8 bytes
     * Note: Chunk coordinates (chunkX, chunkY) are stored in ESFChunkDataHeader
     */
    static constexpr size_t ESF_INDEX_ENTRY_SIZE = 8;

    /**
     * @brief Total block index area size (bytes)
     *
     * Automatic calculation: MAX_CHUNKS × INDEX_ENTRY_SIZE
     */
    static constexpr size_t ESF_INDEX_SIZE = ESF_MAX_CHUNKS * ESF_INDEX_ENTRY_SIZE;

    // ========================================================================
    // Compression Configuration
    // ========================================================================

    /**
     * @brief Supported compression algorithm types
     */
    enum class ESFCompressionType : uint8_t
    {
        None = 0, // No compression
        RLE = 255 // Run Length Encoding
    };

    /**
     * @brief default compression algorithm
     *
     * RLE is suitable for terrain with a large number of repeated blocks (such as underground and ocean)
     * None is suitable for complex buildings and areas with rich terrain changes
     */
    static constexpr ESFCompressionType ESF_DEFAULT_COMPRESSION = ESFCompressionType::RLE;

    // ========================================================================
    // StateMapping Configuration
    // ========================================================================

    /**
     * @brief StateMapping Maximum size (bytes) of serialized data
     *
     * For verification and buffer allocation
     * 64KB should be enough to store hundreds of different block states
     */
    static constexpr size_t ESF_MAX_STATE_MAPPING_SIZE = 64 * 1024; // 64KB

    /**
     * @brief Format overhead in ESF v2 format (bytes)
     *
     * Included: Magic(4) + StateMappingSize(4) = 8 bytes
     */
    static constexpr size_t ESF_FORMAT_OVERHEAD = 2 * sizeof(uint32_t);

    // ========================================================================
    // Performance Tuning Configuration
    // ========================================================================

    /**
     * @brief The minimum size of chunk data (bytes)
     *
     * Based on chunk size calculation: BLOCKS_PER_CHUNK × sizeof(uint32_t)
     * For verification and memory allocation
     */
    static constexpr size_t ESF_MIN_BLOCK_DATA_SIZE = 16 * 16 * 16 * sizeof(uint32_t); // 4096 blocks × 4 bytes

    /**
     * @brief The maximum total size of ESF data (bytes)
     *
     * Includes: Minimum block of data + StateMapping + Format overhead
     */
    static constexpr size_t ESF_MAX_TOTAL_SIZE = ESF_MIN_BLOCK_DATA_SIZE + ESF_MAX_STATE_MAPPING_SIZE + ESF_FORMAT_OVERHEAD;

    /**
     * @brief Conservative maximum chunk data size for validation (bytes)
     *
     * This should be large enough to handle:
     * 1. Normal chunk data with StateMapping
     * 2. Worst-case RLE compression expansion (2x original size)
     * 3. Different configuration compatibility
     * 4. Future format extensions
     *
     * Calculation: (BaseChunkData + StateMapping + Overhead) × CompressionWorstCase × ConfigCompatibilityBuffer
     */
    static constexpr size_t ESF_MAX_REASONABLE_CHUNK_SIZE =
        (ESF_MIN_BLOCK_DATA_SIZE + ESF_MAX_STATE_MAPPING_SIZE + ESF_FORMAT_OVERHEAD) * 2 * 2;

    // ========================================================================
    // File Naming Configuration
    // ========================================================================

    /**
     * @brief Region file extension
     */
    static constexpr const char* ESF_FILE_EXTENSION = ".esf";
    /**
     * @brief Region file naming prefix
     *
     * Full format: "r.{regionX}.{regionY}.esf"
     * Example: "r.0.0.esf", "r.-1.2.esf"
     */
    static constexpr const char* ESF_FILE_PREFIX = "r.";

    // ========================================================================
    // Debug and Logging Configuration
    // ========================================================================

    /**
     * @brief Whether to enable detailed serialization logs
     *
     * After enabled, detailed information such as StateMapping, block coordinates, etc. will be output.
     * It is recommended to set to false to improve performance in the production environment
     */
    static constexpr bool ESF_ENABLE_VERBOSE_LOGGING = true;

    /**
     * @brief Whether to enable CRC32 verification
     *
     * When enabled, the block data will be checked integrity.
     * Can detect file corruption but will increase a small amount of CPU overhead
     */
    static constexpr bool ESF_ENABLE_CRC32_VALIDATION = true;

    // ========================================================================
    // Helper Macros and Inline Functions
    // ========================================================================

    /**
     * @brief calculates the minimum file size for the specified number of chunk
     */
    constexpr size_t CalculateMinFileSize(uint32_t chunkCount) noexcept
    {
        return ESF_HEADER_SIZE + ESF_INDEX_SIZE + (chunkCount * sizeof(uint32_t));
    }

    /**
     * @brief Verify that the region coordinates are within the valid range
     */
    constexpr bool IsValidRegionCoordinate(int32_t coord) noexcept
    {
        // Supports the full int32_t range, but avoids extreme values to prevent overflow
        return coord > INT32_MIN + 1000 && coord < INT32_MAX - 1000;
    }

    /**
     * @brief Check whether the area size configuration is reasonable
     */
    constexpr bool ValidateRegionSizeConfig() noexcept
    {
        return ESF_REGION_SIZE >= 4 && // At least 4x4 chunks
            ESF_REGION_SIZE <= 128 && // Up to 128x128 chunks
            (ESF_REGION_SIZE & (ESF_REGION_SIZE - 1)) == 0; // Must be a power of 2
    }

    // Verify configuration rationality at compile time
    static_assert(ValidateRegionSizeConfig(), "ESF_REGION_SIZE must be a power of 2 between 4 and 128");
    static_assert(ESF_MAX_CHUNKS > 0, "ESF_MAX_CHUNKS must be positive");
    static_assert(ESF_HEADER_SIZE >= 32, "ESF_HEADER_SIZE must be at least 32 bytes");
    static_assert(ESF_MAX_STATE_MAPPING_SIZE >= 1024, "ESF_MAX_STATE_MAPPING_SIZE must be at least 1KB");
}
