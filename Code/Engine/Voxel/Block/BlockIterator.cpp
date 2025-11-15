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

    BlockPos BlockIterator::GetBlockPos() const
    {
        if (!IsValid()) return BlockPos(0, 0, 0);

        int localX = GetLocalX();
        int localY = GetLocalY();
        int localZ = GetLocalZ();
        return m_chunk->LocalToWorld(localX, localY, localZ);
    }

    /**
     * @brief Get neighbor block iterator in the specified direction with cross-chunk boundary support
     *
     * ALGORITHM OVERVIEW:
     *   For each direction, we follow this pattern:
     *   1. Extract current coordinate (x/y/z) from block_index using bit operations
     *   2. Check if at chunk boundary (e.g., y == 15 for NORTH)
     *   3a. If at boundary: Get neighbor chunk + reset coordinate to opposite edge
     *   3b. If not at boundary: Update block_index within current chunk
     *   4. Return new BlockIterator (or invalid iterator if no neighbor exists)
     *
     * COORDINATE EXTRACTION FROM BLOCK_INDEX:
     *   x = blockIndex & CHUNK_MASK_X          // Bits 0-3
     *   y = (blockIndex >> 4) & 0x0F           // Bits 4-7
     *   z = blockIndex >> 8                     // Bits 8-15
     *
     * BIT MANIPULATION FOR COORDINATE UPDATE:
     *   - Clear bits:  blockIndex & ~CHUNK_MASK_Y  (sets coordinate to 0)
     *   - Set bits:    blockIndex | CHUNK_MASK_Y   (sets coordinate to 15)
     *   - Replace bits: (blockIndex & ~MASK) | (newValue << shift)
     *
     * CROSS-CHUNK BOUNDARY HANDLING:
     *   When crossing boundaries, we must:
     *   - Retrieve the adjacent chunk (may be null if not loaded)
     *   - Verify the chunk is active (not in Loading/Generating state)
     *   - Adjust block_index to represent the correct position in the new chunk
     *
     * WHY THIS DESIGN?
     *   - Lighting propagation (Assignment 05 Phase 2-3) needs to traverse chunk boundaries
     *   - Single-pass mesh generation requires accessing neighbor blocks
     *   - Bit operations are faster than modulo arithmetic for boundary detection
     *
     * @param dir Direction to move (NORTH/SOUTH/EAST/WEST/UP/DOWN)
     * @return BlockIterator pointing to neighbor block, or invalid iterator if:
     *         - Current iterator is invalid
     *         - Neighbor chunk doesn't exist or isn't active
     *         - Reaching world boundary (UP/DOWN at z=0 or z=255)
     */
    BlockIterator BlockIterator::GetNeighbor(Direction dir) const
    {
        if (!IsValid()) return BlockIterator();

        switch (dir)
        {
        case Direction::NORTH:
            {
                // NORTH: Move y+1, may cross chunk boundary from y=15 to y=0 in north neighbor
                //
                // Example crossing boundary:
                //   Current: (x=10, y=15, z=64) in Chunk(0,0)
                //   Target:  (x=10, y=0,  z=64) in Chunk(0,1)
                //
                // Step 1: Extract current y coordinate from block_index
                int y = GetLocalY(); // y = (blockIndex >> 4) & 0x0F

                // Step 2: Check if at NORTH boundary (y == 15)
                if (y == Chunk::CHUNK_MAX_Y)
                {
                    // Step 3: Get neighbor chunk to the NORTH (y+1 direction)
                    Chunk* northChunk = m_chunk->GetNorthNeighbor();
                    if (!northChunk || !northChunk->IsActive())
                        return BlockIterator(); // Invalid if neighbor not loaded

                    // Step 4: Clear y-bits to reset y=0 in neighbor chunk
                    // blockIndex & ~CHUNK_MASK_Y
                    //   Example: 0x40FA & ~0x00F0 = 0x40FA & 0xFF0F = 0x400A
                    //   Original:  (x=10, y=15, z=64) → blockIndex = 0x40FA
                    //   Result:    (x=10, y=0,  z=64) → blockIndex = 0x400A ✓
                    return BlockIterator(northChunk, m_blockIndex & ~Chunk::CHUNK_MASK_Y);
                }

                // Step 5: Within chunk, increment y coordinate
                // Clear y-bits, then set new y value
                //   Example: y=8 → y=9
                //   (0x0F8A & ~0x00F0) | (9 << 4) = 0x0F0A | 0x0090 = 0x0F9A
                int newIndex = (m_blockIndex & ~Chunk::CHUNK_MASK_Y) | ((y + 1) << Chunk::CHUNK_BITS_X);
                return BlockIterator(m_chunk, newIndex);
            }
        case Direction::SOUTH:
            {
                // SOUTH: Move y-1, may cross chunk boundary from y=0 to y=15 in south neighbor
                //
                // Example crossing boundary:
                //   Current: (x=10, y=0, z=64) in Chunk(0,0)
                //   Target:  (x=10, y=15, z=64) in Chunk(0,-1)
                //
                // Step 1: Extract current y coordinate
                int y = GetLocalY();

                // Step 2: Check if at SOUTH boundary (y == 0)
                if (y == 0)
                {
                    // Step 3: Get neighbor chunk to the SOUTH (y-1 direction)
                    Chunk* southChunk = m_chunk->GetSouthNeighbor();
                    if (!southChunk || !southChunk->IsActive())
                        return BlockIterator();

                    // Step 4: Set y-bits to maximum (y=15) in neighbor chunk
                    // blockIndex | CHUNK_MASK_Y
                    //   Example: 0x400A | 0x00F0 = 0x40FA
                    //   Original:  (x=10, y=0,  z=64) → blockIndex = 0x400A
                    //   Result:    (x=10, y=15, z=64) → blockIndex = 0x40FA ✓
                    //
                    // WHY use OR instead of AND?
                    //   - NORTH uses AND to clear bits (set to 0)
                    //   - SOUTH uses OR to set bits (set to 15)
                    //   - This is because we cross to opposite edges
                    return BlockIterator(southChunk, m_blockIndex | Chunk::CHUNK_MASK_Y);
                }

                // Step 5: Within chunk, decrement y coordinate
                int newIndex = (m_blockIndex & ~Chunk::CHUNK_MASK_Y) | ((y - 1) << Chunk::CHUNK_BITS_X);
                return BlockIterator(m_chunk, newIndex);
            }
        case Direction::EAST:
            {
                // EAST: Move x+1, may cross chunk boundary from x=15 to x=0 in east neighbor
                //
                // Step 1: Extract current x coordinate
                int x = GetLocalX();

                // Step 2: Check if at EAST boundary (x == 15)
                if (x == Chunk::CHUNK_MAX_X)
                {
                    // Step 3: Get neighbor chunk to the EAST
                    Chunk* eastChunk = m_chunk->GetEastNeighbor();
                    if (!eastChunk || !eastChunk->IsActive())
                        return BlockIterator();

                    // Step 4: Clear x-bits to reset x=0 in neighbor chunk
                    // blockIndex & ~CHUNK_MASK_X clears bits 0-3
                    return BlockIterator(eastChunk, m_blockIndex & ~Chunk::CHUNK_MASK_X);
                }

                // Step 5: Within chunk, increment x coordinate
                // Optimization: Since x occupies bits 0-3, we can simply add 1
                // Example: x=10 → x=11
                //   0x0F8A + 1 = 0x0F8B
                return BlockIterator(m_chunk, m_blockIndex + 1);
            }
        case Direction::WEST:
            {
                // WEST: Move x-1, may cross chunk boundary from x=0 to x=15 in west neighbor
                //
                // Step 1: Extract current x coordinate
                int x = GetLocalX();

                // Step 2: Check if at WEST boundary (x == 0)
                if (x == 0)
                {
                    // Step 3: Get neighbor chunk to the WEST
                    Chunk* westChunk = m_chunk->GetWestNeighbor();
                    if (!westChunk || !westChunk->IsActive())
                        return BlockIterator();

                    // Step 4: Set x-bits to maximum (x=15) in neighbor chunk
                    // blockIndex | CHUNK_MASK_X sets bits 0-3 to 1111
                    return BlockIterator(westChunk, m_blockIndex | Chunk::CHUNK_MASK_X);
                }

                // Step 5: Within chunk, decrement x coordinate
                // Optimization: Since x occupies bits 0-3, we can simply subtract 1
                return BlockIterator(m_chunk, m_blockIndex - 1);
            }
        case Direction::UP:
            {
                // UP: Move z+1, world boundary at z=255 (no cross-chunk, vertical is within chunk)
                //
                // Step 1: Extract current z coordinate
                int z = GetLocalZ();

                // Step 2: Check if at world top boundary (z == 255)
                if (z == Chunk::CHUNK_MAX_Z)
                    return BlockIterator(); // Invalid: cannot go above world height

                // Step 3: Increment z coordinate using bit shift
                // Add (1 << 8) to increment the z bits
                // Example: z=64 → z=65
                //   0x400A + (1 << 8) = 0x400A + 0x0100 = 0x410A
                //   Decoded: z increases from 64 to 65 ✓
                //
                // WHY shift by 8?
                //   - Z occupies bits 8-15
                //   - Adding (1 << 8) = 0x0100 increments the Z value by 1
                int newIndex = m_blockIndex + (1 << (Chunk::CHUNK_BITS_X + Chunk::CHUNK_BITS_Y));
                return BlockIterator(m_chunk, newIndex);
            }
        case Direction::DOWN:
            {
                // DOWN: Move z-1, world boundary at z=0 (no cross-chunk, vertical is within chunk)
                //
                // Step 1: Extract current z coordinate
                int z = GetLocalZ();

                // Step 2: Check if at world bottom boundary (z == 0)
                if (z == 0)
                    return BlockIterator(); // Invalid: cannot go below bedrock level

                // Step 3: Decrement z coordinate using bit shift
                // Subtract (1 << 8) to decrement the z bits
                // Example: z=65 → z=64
                //   0x410A - (1 << 8) = 0x410A - 0x0100 = 0x400A
                //
                // NOTE: UP/DOWN do not cross chunk boundaries because:
                //   - Chunks are 256 blocks tall (full world height)
                //   - Z coordinate is always within [0, 255] in the same chunk
                //   - Only X/Y directions require chunk neighbor access
                int newIndex = m_blockIndex - (1 << (Chunk::CHUNK_BITS_X + Chunk::CHUNK_BITS_Y));
                return BlockIterator(m_chunk, newIndex);
            }
        default:
            return BlockIterator();
        }
    }

    /**
     * @brief Get all 6 neighbor block iterators in one call (batch query optimization)
     *
     * Returns a std::array of 6 BlockIterators representing neighbors in all cardinal directions.
     * This is more efficient than calling GetNeighbor() 6 times separately when you need
     * all neighbors (e.g., for lighting propagation in Assignment 05 Phase 2-3).
     *
     * ARRAY ORDER:
     *   [0] = NORTH  (y+1)
     *   [1] = SOUTH  (y-1)
     *   [2] = EAST   (x+1)
     *   [3] = WEST   (x-1)
     *   [4] = UP     (z+1)
     *   [5] = DOWN   (z-1)
     *
     * USAGE EXAMPLE (Lighting propagation):
     *   BlockIterator current = ...;
     *   auto neighbors = current.GetNeighbors();
     *   for (const auto& neighbor : neighbors) {
     *       if (neighbor.IsValid()) {
     *           PropagateLightTo(neighbor);
     *       }
     *   }
     *
     * PERFORMANCE NOTE:
     *   Each GetNeighbor() call may query a different chunk, so this method
     *   may access up to 4 different chunks (current + 3 neighbors at corners).
     *
     * @return Array of 6 BlockIterators, some may be invalid if at boundaries or chunks not loaded
     */
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
