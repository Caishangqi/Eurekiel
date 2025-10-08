#pragma once
#include "ChunkSerializationInterfaces.hpp"
#include <vector>
#include <cstdint>

namespace enigma::voxel
{
    /**
     * @brief ESFS Chunk Serializer - RLE-based serialization for ESFS format
     *
     * Serialization Strategy:
     * -----------------------
     * 1. Extract block IDs from Chunk (BlockState* -> Block numeric ID)
     * 2. Apply RLE compression to block ID array
     * 3. Prepend 8-byte header (ESFS magic, version, chunk bits)
     * 4. Return compressed binary data
     *
     * Performance:
     * ------------
     * - Serialization: ~0.5ms (32768 blocks -> ~2-10KB)
     * - Deserialization: ~0.3ms
     * - Compression Ratio: 10-50x (depending on block variety)
     *
     * Usage:
     * ------
     * auto serializer = std::make_unique<ESFSChunkSerializer>();
     * std::vector<uint8_t> data;
     * serializer->SerializeChunk(chunk, data);
     */
    class ESFSChunkSerializer : public IChunkSerializer
    {
    public:
        //-------------------------------------------------------------------------------------------
        // IChunkSerializer Interface
        //-------------------------------------------------------------------------------------------

        /**
         * @brief Serialize chunk to ESFS binary format (Header + RLE data)
         *
         * @param chunk Chunk to serialize
         * @param outData Output binary data (Header 8 bytes + RLE compressed block IDs)
         * @return True if serialization succeeded
         */
        bool SerializeChunk(const Chunk* chunk, std::vector<uint8_t>& outData) override;

        /**
         * @brief Deserialize ESFS binary format to chunk
         *
         * @param chunk Chunk to fill with data
         * @param data Input binary data (Header + RLE compressed block IDs)
         * @return True if deserialization succeeded
         */
        bool DeserializeChunk(Chunk* chunk, const std::vector<uint8_t>& data) override;

    private:
        //-------------------------------------------------------------------------------------------
        // ESFS File Header (8 bytes)
        //-------------------------------------------------------------------------------------------
        struct Header
        {
            char    magic[4]   = {'E', 'S', 'F', 'S'}; // "ESFS"
            uint8_t version    = 1; // Format version
            uint8_t chunkBitsX = 4; // 16 blocks
            uint8_t chunkBitsY = 4; // 16 blocks
            uint8_t chunkBitsZ = 7; // 128 blocks

            // Calculate expected block count
            uint32_t GetBlockCount() const
            {
                return (1 << chunkBitsX) * (1 << chunkBitsY) * (1 << chunkBitsZ);
            }
        };

        static_assert(sizeof(Header) == 8, "ESFSChunkSerializer::Header must be exactly 8 bytes");

        //-------------------------------------------------------------------------------------------
        // Helper Methods
        //-------------------------------------------------------------------------------------------

        /**
         * @brief Convert Chunk to block ID array
         *
         * Extracts numeric block IDs from BlockState pointers.
         * Air blocks (null or no block) are represented as ID 0.
         *
         * @param chunk Chunk to serialize
         * @param outBlockIDs Output block ID array (32768 entries, values 0-255)
         * @return True if conversion succeeded
         */
        bool SerializeToBlockIDs(const Chunk* chunk, std::vector<int32_t>& outBlockIDs);

        /**
         * @brief Convert block ID array back to Chunk
         *
         * Looks up Block by numeric ID and sets BlockState in chunk.
         *
         * @param chunk Chunk to fill
         * @param blockIDs Input block ID array (32768 entries)
         * @return True if conversion succeeded
         */
        bool DeserializeFromBlockIDs(Chunk* chunk, const std::vector<int32_t>& blockIDs);

        /**
         * @brief RLE compress block ID array
         *
         * Run-Length Encoding format: [BlockType (1 byte)][RunLength (1 byte)]
         * Each run represents consecutive identical blocks (max 255 per run).
         *
         * @param blockIDs Input block IDs (32768 entries, values 0-255)
         * @param outRLE Output RLE-compressed data
         * @return True if compression succeeded
         */
        bool CompressRLE(const std::vector<int32_t>& blockIDs, std::vector<uint8_t>& outRLE);

        /**
         * @brief RLE decompress to block ID array
         *
         * Decodes RLE format: [BlockType][RunLength] pairs.
         *
         * @param rleData Input RLE-compressed data
         * @param outBlockIDs Output block ID array (will be resized to 32768)
         * @return True if decompression succeeded
         */
        bool DecompressRLE(const std::vector<uint8_t>& rleData, std::vector<int32_t>& outBlockIDs);

        /**
         * @brief Validate ESFS header
         *
         * Checks magic number "ESFS", version, and chunk bits.
         *
         * @param header Header to validate
         * @return True if header is valid
         */
        bool ValidateHeader(const Header& header);
    };
} // namespace enigma::voxel
