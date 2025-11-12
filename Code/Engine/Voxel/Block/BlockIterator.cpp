#include "BlockIterator.hpp"
#include "../Chunk/Chunk.hpp"

namespace enigma::voxel
{
    BlockState* BlockIterator::GetBlock() const
    {
        if (!IsValid()) return nullptr;

        int x = GetLocalX();
        int y = GetLocalY();
        int z = GetLocalZ();
        return m_chunk->GetBlock(x, y, z);
    }

    BlockIterator BlockIterator::GetNeighbor(Direction dir) const
    {
        if (!IsValid()) return BlockIterator();

        switch (dir)
        {
        case Direction::NORTH:
            {
                int y = GetLocalY();
                if (y == Chunk::CHUNK_MAX_Y)
                {
                    Chunk* northChunk = m_chunk->GetNorthNeighbor();
                    if (!northChunk || !northChunk->IsActive()) return BlockIterator();
                    return BlockIterator(northChunk, m_blockIndex & ~Chunk::CHUNK_MASK_Y);
                }
                int newIndex = (m_blockIndex & ~Chunk::CHUNK_MASK_Y) | ((y + 1) << Chunk::CHUNK_BITS_X);
                return BlockIterator(m_chunk, newIndex);
            }
        case Direction::SOUTH:
            {
                int y = GetLocalY();
                if (y == 0)
                {
                    Chunk* southChunk = m_chunk->GetSouthNeighbor();
                    if (!southChunk || !southChunk->IsActive()) return BlockIterator();
                    return BlockIterator(southChunk, m_blockIndex | Chunk::CHUNK_MASK_Y);
                }
                int newIndex = (m_blockIndex & ~Chunk::CHUNK_MASK_Y) | ((y - 1) << Chunk::CHUNK_BITS_X);
                return BlockIterator(m_chunk, newIndex);
            }
        case Direction::EAST:
            {
                int x = GetLocalX();
                if (x == Chunk::CHUNK_MAX_X)
                {
                    Chunk* eastChunk = m_chunk->GetEastNeighbor();
                    if (!eastChunk || !eastChunk->IsActive()) return BlockIterator();
                    return BlockIterator(eastChunk, m_blockIndex & ~Chunk::CHUNK_MASK_X);
                }
                return BlockIterator(m_chunk, m_blockIndex + 1);
            }
        case Direction::WEST:
            {
                int x = GetLocalX();
                if (x == 0)
                {
                    Chunk* westChunk = m_chunk->GetWestNeighbor();
                    if (!westChunk || !westChunk->IsActive()) return BlockIterator();
                    return BlockIterator(westChunk, m_blockIndex | Chunk::CHUNK_MASK_X);
                }
                return BlockIterator(m_chunk, m_blockIndex - 1);
            }
        case Direction::UP:
            {
                int z = GetLocalZ();
                if (z == Chunk::CHUNK_MAX_Z) return BlockIterator();
                int newIndex = m_blockIndex + (1 << (Chunk::CHUNK_BITS_X + Chunk::CHUNK_BITS_Y));
                return BlockIterator(m_chunk, newIndex);
            }
        case Direction::DOWN:
            {
                int z = GetLocalZ();
                if (z == 0) return BlockIterator();
                int newIndex = m_blockIndex - (1 << (Chunk::CHUNK_BITS_X + Chunk::CHUNK_BITS_Y));
                return BlockIterator(m_chunk, newIndex);
            }
        default:
            return BlockIterator();
        }
    }

    std::array<BlockIterator, 6> BlockIterator::GetNeighbors() const
    {
        return {
            GetNeighbor(Direction::NORTH),
            GetNeighbor(Direction::SOUTH),
            GetNeighbor(Direction::EAST),
            GetNeighbor(Direction::WEST),
            GetNeighbor(Direction::UP),
            GetNeighbor(Direction::DOWN)
        };
    }
}
