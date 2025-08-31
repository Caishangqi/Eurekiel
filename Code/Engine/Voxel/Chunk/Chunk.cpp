#include "Chunk.hpp"

#include "Engine/Core/ErrorWarningAssert.hpp"
using namespace enigma::voxel::chunk;

Chunk::Chunk(IntVec2 chunkCoords) : m_chunkCoords(chunkCoords)
{
    m_blocks.reserve(BLOCKS_PER_CHUNK);
}

BlockState Chunk::GetBlock(int32_t x, int32_t y, int32_t z)
{
    return m_blocks[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_HEIGHT];
}

void Chunk::SetBlock(int32_t x, int32_t y, int32_t z, BlockState state)
{
    m_blocks[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_HEIGHT] = state;
}

void Chunk::MarkDirty()
{
    m_isDirty = true;
}

void Chunk::RebuildMesh()
{
    ERROR_RECOVERABLE("Chunk::RebuildMesh is not implemented")
}

ChunkMesh* Chunk::GetMesh() const
{
    return m_mesh.get();
}

bool Chunk::NeedsMeshRebuild() const
{
    return m_isDirty;
}

/**
 * Retrieves the block state at the specified world position within the chunk.
 *
 * This method converts the given world position to local chunk coordinates.
 * If the coordinates are within the chunk's range, it fetches the block state
 * at those local coordinates. If the coordinates are outside of the chunk's
 * range, it currently returns a default-constructed block state.
 *
 * @param worldPos The position in world coordinates where the block state needs to be retrieved.
 * @return The block state at the given world position. Returns a default block state
 *         if the position is outside the chunk's range.
 */
BlockState Chunk::GetBlockWorld(const BlockPos& worldPos)
{
    int32_t localX, localY, localZ;
    if (!WorldToLocal(worldPos, localX, localY, localZ)) // Convert world coordinates to local coordinates in chunk
    {
        // The coordinates are not within this chunk range, and return the default air block state
        // TODO: Here, an AIR BlockState should be returned, and the default constructed state should be returned.
        // return BlockState{};
    }

    return GetBlock(localX, localY, localZ); // If within range, call GetBlock to return BlockState
}

void Chunk::SetBlockWorld(const BlockPos& worldPos, BlockState state)
{
    int32_t localX, localY, localZ;
    if (!WorldToLocal(worldPos, localX, localY, localZ))
    {
        // The coordinates are not within this chunk range and cannot be set. Return directly
        // In actual implementation, warnings or errors may need to be recorded
        return;
    }

    SetBlock(localX, localY, localZ, state);
    MarkDirty(); // Marking chunk requires regenerating mesh
}

/**
 * Converts local chunk coordinates to world coordinates.
 *
 * This method calculates the world position of a block by adding the chunk's
 * world position offset to the given local coordinates. The X and Z world
 * coordinates are computed by multiplying the chunk's grid position by the chunk
 * size and adding the respective local coordinates. The Y coordinate remains unchanged,
 * as chunks span the full vertical height of the world.
 *
 * @param x The local X coordinate within the chunk.
 * @param y The local Y coordinate within the chunk.
 * @param z The local Z coordinate within the chunk.
 * @return A BlockPos object representing the block's position in world coordinates.
 */
BlockPos Chunk::LocalToWorld(int32_t x, int32_t y, int32_t z)
{
    int32_t worldX = m_chunkCoords.x * CHUNK_SIZE + x; // chunk world X * 16 + local X
    int32_t worldY = y; // The Y coordinate remains unchanged
    int32_t worldZ = m_chunkCoords.y * CHUNK_SIZE + z; // chunk world Z * 16 + local Z

    return BlockPos(worldX, worldY, worldZ);
}

/**
 * Converts world coordinates to local chunk coordinates and verifies if the position
 * is within the boundaries of the current chunk.
 *
 * This method calculates local coordinates relative to the chunk's origin based
 * on the given world position. If the world position does not belong to the current
 * chunk or the resulting local coordinates fall outside valid bounds, the method
 * returns false. Otherwise, it returns true along with the converted local coordinates.
 *
 * @param worldPos The world position to be converted to local coordinates.
 * @param x Reference to store the computed local x-coordinate.
 * @param y Reference to store the computed local y-coordinate.
 * @param z Reference to store the computed local z-coordinate.
 * @return True if the world position is within the current chunk and valid local
 *         coordinates were calculated. False otherwise.
 */
bool Chunk::WorldToLocal(const BlockPos& worldPos, int32_t& x, int32_t& y, int32_t& z)
{
    // Calculate the chunk coordinates corresponding to the world coordinates (integer division will automatically round down)
    int32_t chunkX = worldPos.x / CHUNK_SIZE;
    int32_t chunkZ = worldPos.z / CHUNK_SIZE;

    // Handle the special case of negative number coordinates
    if (worldPos.x < 0 && worldPos.x % CHUNK_SIZE != 0) chunkX--;
    if (worldPos.z < 0 && worldPos.z % CHUNK_SIZE != 0) chunkZ--;

    // Check whether it belongs to the current chunk
    if (chunkX != m_chunkCoords.x || chunkZ != m_chunkCoords.y)
    {
        return false; // Not part of the current chunk
    }

    // Calculate local coordinates (world coordinates - chunk starting world coordinates)
    x = worldPos.x - (m_chunkCoords.x * CHUNK_SIZE); // Local X = World X - chunk Start X
    y = worldPos.y; // The Y coordinate remains unchanged
    z = worldPos.z - (m_chunkCoords.y * CHUNK_SIZE); // Local Z = World Z - chunk Start Z

    // Verify that local coordinates are in the valid range [0, CHUNK_SIZE) and [0, CHUNK_HEIGHT)
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE)
    {
        return false; // The coordinates are outside the chunk range
    }

    return true;
}

/**
 * Determines if the given world position is contained within the bounds of this chunk.
 *
 * This method checks if the specified world position can be successfully converted
 * to local chunk coordinates, indicating that the position lies within the range
 * of this chunk.
 *
 * @param worldPos The position in world coordinates to check for containment within the chunk.
 * @return True if the world position is within the chunk's range; otherwise, false.
 */
bool Chunk::ContainsWorldPos(const BlockPos& worldPos)
{
    // Check whether the world coordinates are within this chunk range
    int32_t localX, localY, localZ;
    return WorldToLocal(worldPos, localX, localY, localZ); // Try to convert to local coordinates, if successful, include
}

/**
 * Clears all data within the chunk.
 *
 * This method resets the chunk's internal state, releasing any stored data
 * or references to other objects. It ensures that the chunk is in an empty
 * and ready-to-use state following execution.
 */
void Chunk::Clear()
{
    ERROR_RECOVERABLE("Chunk::Clear is not implemented")
}

/**
 * Retrieves the world position corresponding to the starting corner of the chunk.
 *
 * This method calculates the position in world coordinates based on the chunk's
 * coordinates and the chunk size. The Y-coordinate defaults to 0, representing
 * the lowest vertical position.
 *
 * @return The block position in world coordinates representing the chunk's starting corner.
 */
BlockPos Chunk::GetWorldPos() const
{
    int32_t worldX = m_chunkCoords.x * CHUNK_SIZE; // The world start X coordinate of chunk
    int32_t worldY = 0; // Start at the bottom of the world
    int32_t worldZ = m_chunkCoords.y * CHUNK_SIZE; // The world start Z coordinate of chunk

    return BlockPos(worldX, worldY, worldZ);
}
