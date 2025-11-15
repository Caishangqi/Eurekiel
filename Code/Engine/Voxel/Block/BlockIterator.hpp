#pragma once
#include "BlockState.hpp"
#include "../Property/PropertyTypes.hpp"
#include <array>

namespace enigma::voxel
{
    class Chunk;

    /**
     * @brief Block iterator using bit-masked block_index for efficient neighbor access across chunk boundaries
     *
     * BLOCK INDEX ENCODING FORMAT:
     *   block_index = x + (y << CHUNK_BITS_X) + (z << (CHUNK_BITS_X + CHUNK_BITS_Y))
     *   - CHUNK_BITS_X = 4 (supports 2^4 = 16 blocks in X direction)
     *   - CHUNK_BITS_Y = 4 (supports 2^4 = 16 blocks in Y direction)
     *   - CHUNK_BITS_Z = 8 (supports 2^8 = 256 blocks in Z direction)
     *
     *   Example encoding for position (x=10, y=8, z=15):
     *     block_index = 10 + (8 << 4) + (15 << 8)
     *                 = 0x000A + 0x0080 + 0x0F00
     *                 = 0x0F8A (decimal: 3978)
     *
     *   Binary representation:
     *     0x0F8A = 0000 1111 1000 1010
     *              ^^^^ ^^^^      ^^^^
     *              z=15 y=8       x=10
     *
     * BIT MASKS FOR COORDINATE EXTRACTION:
     *   - CHUNK_MASK_X = 0x000F (bits 0-3)   - extracts x coordinate
     *   - CHUNK_MASK_Y = 0x00F0 (bits 4-7)   - extracts y coordinate
     *   - CHUNK_MASK_Z = 0xFF00 (bits 8-15)  - extracts z coordinate
     *
     *   Example extraction from block_index = 0x0F8A:
     *     x = blockIndex & CHUNK_MASK_X          = 0x0F8A & 0x000F = 0x000A = 10
     *     y = (blockIndex >> 4) & 0x0F           = 0x00F8 & 0x000F = 0x0008 = 8
     *     z = blockIndex >> 8                    = 0x000F           = 15
     *
     * CROSS-CHUNK BOUNDARY ALGORITHM:
     *   When a block is at the edge of a chunk (e.g., y=15 going NORTH):
     *
     *   Step 1: Detect boundary condition (y == CHUNK_MAX_Y)
     *   Step 2: Get neighbor chunk via Chunk::GetNorthNeighbor()
     *   Step 3: Clear coordinate bits to reset to opposite edge
     *           - For NORTH: blockIndex & ~CHUNK_MASK_Y clears y-bits to 0
     *           - For SOUTH: blockIndex | CHUNK_MASK_Y sets y-bits to 15
     *   Step 4: Return new BlockIterator pointing to neighbor chunk
     *
     *   Example: Crossing NORTH boundary
     *     Current: (x=10, y=15, z=64) in Chunk(0,0)
     *              blockIndex = 10 + (15 << 4) + (64 << 8) = 0x400A + 0x00F0 = 0x40FA
     *
     *     After crossing to NORTH neighbor Chunk(0,1):
     *              blockIndex = 0x40FA & ~CHUNK_MASK_Y
     *                         = 0x40FA & 0xFF0F
     *                         = 0x400A
     *              Decoded: (x=10, y=0, z=64) in Chunk(0,1) ✓
     *
     * WHY USE BIT-MASKING?
     *   - Fast coordinate manipulation without division/modulo operations
     *   - Efficient neighbor queries for lighting propagation (Assignment 05)
     *   - Cache-friendly: single integer stores 3D coordinates
     *   - Enables cross-chunk access with minimal overhead
     *
     * @see Assignment 05 - Block Iterators section
     * @see Chunk.hpp for bit mask constant definitions
     */
    class BlockIterator
    {
    public:
        BlockIterator() : m_chunk(nullptr), m_blockIndex(-1)
        {
        }

        BlockIterator(Chunk* chunk, int blockIndex) : m_chunk(chunk), m_blockIndex(blockIndex)
        {
        }

        BlockState* GetBlock() const;
        bool        IsValid() const { return m_chunk != nullptr && m_blockIndex >= 0; }

        // [REQUIRED] Direction-based neighbor query
        BlockIterator GetNeighbor(Direction dir) const;

        // [BATCH] Get all 6 neighbors at once
        std::array<BlockIterator, 6> GetNeighbors() const;

        // [CONVENIENCE] Inline methods for common directions
        BlockIterator GetNorth() const { return GetNeighbor(Direction::NORTH); }
        BlockIterator GetSouth() const { return GetNeighbor(Direction::SOUTH); }
        BlockIterator GetEast() const { return GetNeighbor(Direction::EAST); }
        BlockIterator GetWest() const { return GetNeighbor(Direction::WEST); }
        BlockIterator GetUp() const { return GetNeighbor(Direction::UP); }
        BlockIterator GetDown() const { return GetNeighbor(Direction::DOWN); }

        Chunk* GetChunk() const { return m_chunk; }
        int    GetBlockIndex() const { return m_blockIndex; }

        /**
         * @brief Get world position of the block
         * @return BlockPos World coordinates of the block
         */
        BlockPos GetBlockPos() const;

        /**
         * @brief Get local coordinates within the chunk
         * @param x Output: local X coordinate (0-15)
         * @param y Output: local Y coordinate (0-15)
         * @param z Output: local Z coordinate (0-255)
         */
        void GetLocalCoords(int32_t& x, int32_t& y, int32_t& z) const;

    private:
        Chunk* m_chunk;
        int    m_blockIndex;

        /**
         * @brief Extract local X coordinate from block_index using bit masking
         *
         * Extracts bits 0-3 which encode the X coordinate (0-15).
         *
         * Example: block_index = 0x0F8A (x=10, y=8, z=15)
         *   GetLocalX() = 0x0F8A & 0x000F = 0x000A = 10
         *
         * @return X coordinate in range [0, 15]
         */
        int GetLocalX() const { return m_blockIndex & 0x0F; }

        /**
         * @brief Extract local Y coordinate from block_index using bit shift and masking
         *
         * Extracts bits 4-7 which encode the Y coordinate (0-15).
         * Step 1: Right-shift by 4 bits to move Y bits to position 0-3
         * Step 2: Apply mask 0x0F to isolate lower 4 bits
         *
         * Example: block_index = 0x0F8A (x=10, y=8, z=15)
         *   Step 1: 0x0F8A >> 4 = 0x00F8
         *   Step 2: 0x00F8 & 0x0F = 0x0008 = 8
         *
         * @return Y coordinate in range [0, 15]
         */
        int GetLocalY() const { return (m_blockIndex >> 4) & 0x0F; }

        /**
         * @brief Extract local Z coordinate from block_index using bit shift
         *
         * Extracts bits 8-15 which encode the Z coordinate (0-255).
         * Right-shift by 8 bits to move Z bits to position 0-7.
         * No masking needed as Z occupies the highest bits.
         *
         * Example: block_index = 0x0F8A (x=10, y=8, z=15)
         *   GetLocalZ() = 0x0F8A >> 8 = 0x000F = 15
         *
         * @return Z coordinate in range [0, 255]
         */
        int GetLocalZ() const { return m_blockIndex >> 8; }
    };
}
