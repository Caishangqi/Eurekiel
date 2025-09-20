#pragma once
#include "Engine/Core/EngineCommon.hpp"
#include <vector>
#include <cstdint>

namespace enigma::voxel::chunk
{
    /**
     * @brief RLE (Run Length Encoding) Compressor for voxel block data
     *
     * Optimized for voxel chunk data where consecutive identical blocks are common.
     * Implements Assignment 2 RLE compression specification.
     *
     * RLE Format:
     * - Each run is encoded as: [RunLength][BlockStateID]
     * - RunLength: uint16_t (1-65535 blocks)
     * - BlockStateID: uint32_t (block state pointer/ID)
     * - Special case: RunLength = 0 indicates uncompressed data follows
     *
     * Benefits for voxel data:
     * - Large air regions compress to very small size
     * - Solid material layers compress efficiently
     * - Mixed areas fall back to reasonable overhead
     */
    class RLECompressor
    {
    public:
        /**
         * @brief RLE compression header (8 bytes)
         *
         * Placed at the beginning of compressed data for validation and decompression info.
         */
        struct RLEHeader
        {
            uint16_t magic        = 0x524C; // "RL" magic number
            uint16_t version      = 1; // RLE format version
            uint32_t originalSize = 0; // Original data size in bytes

            bool IsValid() const { return magic == 0x524C && version == 1; }
        };

        static_assert(sizeof(RLEHeader) == 8, "RLEHeader must be exactly 8 bytes");

        /**
         * @brief RLE run entry (6 bytes, packed)
         *
         * Represents a run of identical values in compressed data.
         */
#pragma pack(push, 1)
        struct RLERunEntry
        {
            uint16_t runLength    = 0; // Number of consecutive identical blocks (1-65535)
            uint32_t blockStateID = 0; // Block state ID/pointer value

            bool IsValid() const { return runLength > 0; }
        };
#pragma pack(pop)
        static_assert(sizeof(RLERunEntry) == 6, "RLERunEntry must be exactly 6 bytes");

    public:
        /**
         * @brief Compress block state data using RLE
         *
         * @param inputData Pointer to array of BlockState* values
         * @param inputSize Number of BlockState* elements
         * @param outputData Output buffer for compressed data
         * @param outputSize Maximum size of output buffer
         * @return Actual compressed data size, or 0 if compression failed
         */
        static size_t Compress(const uint32_t* inputData, size_t  inputSize,
                               uint8_t*        outputData, size_t outputSize);

        /**
         * @brief Decompress RLE data back to block state array
         *
         * @param inputData Compressed data buffer
         * @param inputSize Size of compressed data
         * @param outputData Output buffer for decompressed BlockState* values
         * @param outputSize Maximum number of BlockState* elements to write
         * @return Number of BlockState* elements written, or 0 if decompression failed
         */
        static size_t Decompress(const uint8_t* inputData, size_t  inputSize,
                                 uint32_t*      outputData, size_t outputSize);

        /**
         * @brief Calculate maximum compressed size for given input
         *
         * Worst case: every block is different, so we get header + (runCount * entrySize)
         * where runCount equals inputSize.
         *
         * @param inputSize Number of BlockState* elements
         * @return Maximum possible compressed size in bytes
         */
        static size_t CalculateMaxCompressedSize(size_t inputSize);

        /**
         * @brief Calculate compression ratio estimate
         *
         * @param inputData Pointer to block state data
         * @param inputSize Number of elements
         * @return Estimated compression ratio (0.0 - 1.0, lower is better compression)
         */
        static float EstimateCompressionRatio(const uint32_t* inputData, size_t inputSize);

        /**
         * @brief Check if data is worth compressing
         *
         * @param inputData Pointer to block state data
         * @param inputSize Number of elements
         * @return True if RLE compression would be beneficial
         */
        static bool ShouldCompress(const uint32_t* inputData, size_t inputSize);

        /**
         * @brief Validate compressed data integrity
         *
         * @param compressedData Compressed data buffer
         * @param compressedSize Size of compressed data
         * @param expectedOriginalSize Expected original data size
         * @return True if compressed data appears valid
         */
        static bool ValidateCompressedData(const uint8_t* compressedData, size_t compressedSize,
                                           size_t         expectedOriginalSize);

    private:
        // Internal helper methods
        static size_t CompressRuns(const uint32_t* inputData, size_t inputSize,
                                   RLERunEntry*    runs, size_t      maxRuns);
        static size_t WriteCompressedData(const RLERunEntry* runs, size_t           runCount,
                                          size_t             originalSize, uint8_t* outputData, size_t outputSize);
        static bool   ReadHeader(const uint8_t* inputData, size_t inputSize, RLEHeader& header);
        static size_t ReadRuns(const uint8_t* inputData, size_t  inputSize, size_t dataOffset,
                               uint32_t*      outputData, size_t outputSize);
    };

    /**
     * @brief RLE compression statistics for analysis and optimization
     */
    struct RLEStats
    {
        size_t originalSize     = 0; // Original data size in bytes
        size_t compressedSize   = 0; // Compressed data size in bytes
        size_t runCount         = 0; // Number of RLE runs
        float  compressionRatio = 1.0f; // Compression ratio (compressed/original)
        bool   wasCompressed    = false; // Whether compression was applied

        float  GetCompressionPercent() const { return (1.0f - compressionRatio) * 100.0f; }
        size_t GetSpaceSaved() const { return originalSize > compressedSize ? originalSize - compressedSize : 0; }
    };
}
