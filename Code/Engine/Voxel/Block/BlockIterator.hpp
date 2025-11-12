#pragma once
#include "BlockState.hpp"
#include "../Property/PropertyTypes.hpp"
#include <array>

namespace enigma::voxel
{
    class Chunk;

    /**
     * @brief Block iterator using bit-masked block_index for efficient neighbor access
     *
     * block_index encoding: x + (y << 4) + (z << 8)
     * Supports cross-chunk boundary access using bit masks
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

    private:
        Chunk* m_chunk;
        int    m_blockIndex;

        // [HELPER] Extract local coordinates from block_index
        int GetLocalX() const { return m_blockIndex & 0x0F; }
        int GetLocalY() const { return (m_blockIndex >> 4) & 0x0F; }
        int GetLocalZ() const { return m_blockIndex >> 8; }
    };
}
