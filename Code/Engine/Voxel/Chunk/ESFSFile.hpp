#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace enigma::voxel
{
    /**
     * @brief ESFS (Enigma Single-file Format for Chunks) - High-performance chunk storage
     *
     * File Format Specification (Based on Assignment 02):
     * ----------------------------------------------------
     * File: .enigma/saves/{world_name}/region/chunk_{X}_{Y}.esfs
     *
     * HEADER (8 bytes):
     *   - Magic: "GCHK" (4 bytes) - Guildhall Chunk 4CC code
     *   - Version: uint8 (1 byte) [Current: 1]
     *   - CHUNK_BITS_X: uint8 (1 byte) [Current: 4]
     *   - CHUNK_BITS_Y: uint8 (1 byte) [Current: 4]
     *   - CHUNK_BITS_Z: uint8 (1 byte) [Current: 7]
     *
     * BLOCK DATA (RLE Compressed):
     *   Run-Length Encoded (RLE) format:
     *   - Each run is 2 bytes: [BlockType (1 byte)][RunLength (1 byte)]
     *   - BlockType: Block numeric ID [0-255] (0 = AIR)
     *   - RunLength: Number of consecutive blocks [1-255]
     *   - Blocks are encoded in blockIndex order (x + y*16 + z*256)
     *   - Total run lengths must equal BLOCKS_PER_CHUNK (32768)
     *
     * Example RLE:
     *   01 FF  01 FF  01 07  00 FF  00 08
     *   = 517 stone blocks (type 1) followed by 263 air blocks (type 0)
     *
     * Performance Benefits:
     * ---------------------
     * - File Size: ~2-10KB typical (vs 128KB raw) - 10-50x smaller
     * - Load Speed: <1ms (single binary read + RLE decode)
     * - Save Speed: <1ms (RLE encode + single binary write)
     * - No external compression library needed (pure C++ RLE)
     */
    class ESFSFile
    {
    public:
        //-------------------------------------------------------------------------------------------
        // File Header Structure (8 bytes) - Assignment 02 Format
        //-------------------------------------------------------------------------------------------
        struct Header
        {
            char    magic[4]   = {'E', 'S', 'F', 'S'}; // "ESFS" = Enigma Save File Super
            uint8_t version    = 1; // Format version
            uint8_t chunkBitsX = 4; // CHUNK_BITS_X (16 blocks)
            uint8_t chunkBitsY = 4; // CHUNK_BITS_Y (16 blocks)
            uint8_t chunkBitsZ = 7; // CHUNK_BITS_Z (128 blocks)

            // Calculate expected block count from bits
            uint32_t GetBlockCount() const
            {
                return (1 << chunkBitsX) * (1 << chunkBitsY) * (1 << chunkBitsZ);
            }
        };

        static_assert(sizeof(Header) == 8, "ESFSFile::Header must be exactly 8 bytes");

        //-------------------------------------------------------------------------------------------
        // Save & Load Operations
        //-------------------------------------------------------------------------------------------

        /**
         * @brief Save chunk data to ESFS file with RLE compression
         *
         * @param worldPath Base world save path (e.g., ".enigma/saves/MyWorld")
         * @param chunkX Chunk X coordinate
         * @param chunkY Chunk Y coordinate
         * @param blockIDs Block ID array (size must be 32768, values 0-255)
         * @return True if saved successfully
         */
        static bool SaveChunk(
            const std::string&          worldPath,
            int32_t                     chunkX, int32_t chunkY,
            const std::vector<int32_t>& blockIDs
        );

        /**
         * @brief Load chunk data from ESFS file
         *
         * @param worldPath Base world save path
         * @param chunkX Chunk X coordinate
         * @param chunkY Chunk Y coordinate
         * @param outBlockIDs Output block ID array (will be resized to 32768)
         * @return True if loaded successfully
         */
        static bool LoadChunk(
            const std::string&    worldPath,
            int32_t               chunkX, int32_t chunkY,
            std::vector<int32_t>& outBlockIDs
        );

        /**
         * @brief Check if chunk file exists
         *
         * @param worldPath Base world save path
         * @param chunkX Chunk X coordinate
         * @param chunkY Chunk Y coordinate
         * @return True if chunk file exists on disk
         */
        static bool ChunkExists(
            const std::string& worldPath,
            int32_t            chunkX, int32_t chunkY
        );

        /**
         * @brief Delete chunk file from disk
         *
         * @param worldPath Base world save path
         * @param chunkX Chunk X coordinate
         * @param chunkY Chunk Y coordinate
         * @return True if deleted successfully (or file didn't exist)
         */
        static bool DeleteChunk(
            const std::string& worldPath,
            int32_t            chunkX, int32_t chunkY
        );

        //-------------------------------------------------------------------------------------------
        // Utility Methods
        //-------------------------------------------------------------------------------------------

        /**
         * @brief Get chunk file path for given coordinates
         *
         * @param worldPath Base world save path
         * @param chunkX Chunk X coordinate
         * @param chunkY Chunk Y coordinate
         * @return Full path to chunk file (e.g., ".enigma/saves/MyWorld/region/chunk_0_0.esfs")
         */
        static std::string GetChunkFilePath(
            const std::string& worldPath,
            int32_t            chunkX, int32_t chunkY
        );

        /**
         * @brief Ensure region directory exists
         *
         * Creates "region/" subdirectory in world path if it doesn't exist.
         *
         * @param worldPath Base world save path
         * @return True if directory exists or was created successfully
         */
        static bool EnsureRegionDirectory(const std::string& worldPath);

        /**
         * @brief Validate header integrity
         *
         * Checks magic number "GCHK", version, and chunk bits.
         *
         * @param header Header to validate
         * @return True if header is valid
         */
        static bool ValidateHeader(const Header& header);

    private:
        //-------------------------------------------------------------------------------------------
        // Internal RLE Compression/Decompression (No external library)
        //-------------------------------------------------------------------------------------------

        /**
         * @brief RLE compression for block IDs
         *
         * Run-Length Encoding: consecutive identical blocks are stored as [BlockType][Count]
         * Each run is 2 bytes: BlockType (uint8) + RunLength (uint8)
         *
         * @param blockIDs Input block ID array (32768 blocks, values 0-255)
         * @param outRLE Output RLE-compressed data
         * @return True if compression succeeded
         */
        static bool CompressRLE(
            const std::vector<int32_t>& blockIDs,
            std::vector<uint8_t>&       outRLE
        );

        /**
         * @brief RLE decompression for block IDs
         *
         * Decodes RLE format: [BlockType][Count] pairs
         *
         * @param rleData Input RLE-compressed data
         * @param outBlockIDs Output block ID array (will be resized to 32768)
         * @return True if decompression succeeded
         */
        static bool DecompressRLE(
            const std::vector<uint8_t>& rleData,
            std::vector<int32_t>&       outBlockIDs
        );
    };
} // namespace enigma::voxel
