#include "Chunk.hpp"
#include "ChunkMeshBuilder.hpp"
#include "../World/World.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Renderer/IRenderer.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Renderer/Model/RenderMesh.hpp"
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Engine/Resource/Atlas/TextureAtlas.hpp"
#include "Engine/Voxel/Builtin/DefaultBlock.hpp"

using namespace enigma::voxel;

namespace
{
    std::atomic<uint64_t> g_nextChunkInstanceId{ 1ULL };
}

// Optimized bit-shift coordinate to index conversion
// Formula: index = x + (y << CHUNK_BITS_X) + (z << (CHUNK_BITS_X + CHUNK_BITS_Y))
inline size_t Chunk::CoordsToIndex(int32_t x, int32_t y, int32_t z)
{
    return static_cast<size_t>(x + (y << CHUNK_BITS_X) + (z << (CHUNK_BITS_X + CHUNK_BITS_Y)));
}

// Optimized bit-shift index to coordinates conversion
// Inverse of the above formula using bit masking
inline void Chunk::IndexToCoords(size_t index, int32_t& x, int32_t& y, int32_t& z)
{
    x = static_cast<int32_t>(index & CHUNK_MAX_X);
    y = static_cast<int32_t>((index >> CHUNK_BITS_X) & CHUNK_MAX_Y);
    z = static_cast<int32_t>(index >> (CHUNK_BITS_X + CHUNK_BITS_Y));
}

Chunk::Chunk(IntVec2 chunkCoords) : m_chunkCoords(chunkCoords)
{
    m_instanceId = g_nextChunkInstanceId.fetch_add(1ULL, std::memory_order_relaxed);
    core::LogInfo("chunk", "Chunk created: %d, %d", m_chunkCoords.x, m_chunkCoords.y);
    m_blocks.reserve(BLOCKS_PER_CHUNK * sizeof(BlockState*));

    auto  airBlock = enigma::registry::block::BlockRegistry::GetBlock("simpleminer", "air");
    auto* airState = airBlock->GetDefaultState();
    for (size_t i = 0; i < BLOCKS_PER_CHUNK; ++i)
    {
        m_blocks.push_back(airState);
    }

    /// Calculate chunk bound aabb3
    BlockPos chunkBottomPos = GetWorldPos();
    m_chunkBounding.m_mins  = Vec3((float)chunkBottomPos.x, (float)chunkBottomPos.y, (float)chunkBottomPos.z);
    m_chunkBounding.m_maxs  = m_chunkBounding.m_mins + Vec3(CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z);

    // [A05] Initialize independent light data arrays (Task 1)
    // Each block gets its own light data and flags to avoid BlockState sharing pollution
    m_lightData.resize(BLOCKS_PER_CHUNK, 0); // Initialize all light values to 0 (outdoor=0, indoor=0)
    m_flags.resize(BLOCKS_PER_CHUNK, 0); // Initialize all flags to 0 (no flags set)
}

Chunk::~Chunk() = default;

BlockState* Chunk::GetBlock(int32_t x, int32_t y, int32_t z)
{
    // Optimized bit-shift index calculation: index = x + (y << CHUNK_BITS_X) + (z << (CHUNK_BITS_X + CHUNK_BITS_Y))
    size_t index = CoordsToIndex(x, y, z);
    return m_blocks[index];
}

BlockState* Chunk::GetBlock(int32_t x, int32_t y, int32_t z) const
{
    size_t index = CoordsToIndex(x, y, z);
    return m_blocks[index];
}

void Chunk::SetBlock(int32_t x, int32_t y, int32_t z, BlockState* state)
{
    // Boundary check
    /*
    if (x < 0 || x >= CHUNK_SIZE_X || y < 0 || y >= CHUNK_SIZE_Y || z < 0 || z >= CHUNK_SIZE_Z)
    {
        core::LogError("chunk", "SetBlock coordinates out of range: (%d,%d,%d)", x, y, z);
        return;
    }
    */

    // Optimized bit-shift index calculation: index = x + (y << CHUNK_BITS_X) + (z << (CHUNK_BITS_X + CHUNK_BITS_Y))
    size_t index = CoordsToIndex(x, y, z);

    /*
    if (index >= m_blocks.size())
    {
        core::LogError("chunk", "SetBlock calculated index %zu out of range (size: %zu)", index, m_blocks.size());
        return;
    }*/
    m_blocks[index] = state;

    // Mark chunk as dirty for mesh rebuild only (world generation, no save needed)
    m_isDirty = true;
}

void Chunk::SetBlockByPlayer(int32_t x, int32_t y, int32_t z, BlockState* state)
{
    // 1. Get old block state BEFORE changing
    BlockState* oldState = GetBlock(x, y, z);
    // [UPDATED] Use per-state opacity check for non-full blocks (slabs/stairs)
    bool wasOpaque = oldState->GetBlock()->IsOpaque(oldState);
    bool wasSky    = GetIsSky(x, y, z);

    // 2. Set new block
    size_t index    = CoordsToIndex(x, y, z);
    m_blocks[index] = state;

    // 3. Mark chunk as modified and dirty
    m_isModified     = true;
    m_playerModified = true;
    m_isDirty        = true;

    // 4. Get new block properties
    // [UPDATED] Use per-state opacity check for non-full blocks (slabs/stairs)
    bool isOpaque = state->GetBlock()->IsOpaque(state);

    if (m_world)
    {
        BlockIterator iter(this, (int)index);

        // ===== SKY Flag Propagation (Assignment 05 requirements) =====

        // Case 1: Digging a block (old=opaque, new=non-opaque)
        if (wasOpaque && !isOpaque)
        {
            // Check if block above is SKY
            if (z < CHUNK_MAX_Z)
            {
                bool aboveIsSky = GetIsSky(x, y, z + 1);
                if (aboveIsSky)
                {
                    // Descend downward, flagging non-opaque blocks as SKY
                    for (int descendZ = z; descendZ >= 0; --descendZ)
                    {
                        BlockState* currentBlock = GetBlock(x, y, descendZ);
                        // [UPDATED] Use per-state opacity check for non-full blocks (slabs/stairs)
                        if (currentBlock->GetBlock()->IsOpaque(currentBlock))
                        {
                            break; // Stop at first opaque block
                        }

                        // Flag as SKY and mark dirty
                        SetIsSky(x, y, descendZ, true);
                        SetSkyLight(x, y, descendZ, 15);

                        BlockIterator descendIter(this, (int)CoordsToIndex(x, y, descendZ));
                        m_world->MarkLightingDirty(descendIter);
                    }
                }
            }
        }

        // Case 2: Placing an opaque block (old=SKY, new=opaque)
        else if (wasSky && isOpaque)
        {
            // Clear SKY flag and descend downward
            SetIsSky(x, y, z, false);
            SetSkyLight(x, y, z, 0);

            // Descend downward, clearing SKY flags
            for (int descendZ = z - 1; descendZ >= 0; --descendZ)
            {
                BlockState* currentBlock = GetBlock(x, y, descendZ);
                // [UPDATED] Use per-state opacity check for non-full blocks (slabs/stairs)
                if (currentBlock->GetBlock()->IsOpaque(currentBlock))
                {
                    break; // Stop at first opaque block
                }

                // Clear SKY flag and mark dirty
                SetIsSky(x, y, descendZ, false);
                SetSkyLight(x, y, descendZ, 0);

                BlockIterator descendIter(this, (int)CoordsToIndex(x, y, descendZ));
                m_world->MarkLightingDirty(descendIter);
            }
        }

        // Always mark the changed block itself as dirty
        m_world->MarkLightingDirty(iter);
    }
}

void Chunk::MarkDirty()
{
    m_isDirty = true;
}

bool Chunk::RebuildMesh()
{
    ChunkMeshBuilder builder;
    auto             newMesh = builder.BuildMesh(this);

    if (newMesh)
    {
        SetMesh(std::move(newMesh));
        core::LogInfo("chunk", "Chunk mesh rebuilt using ChunkMeshBuilder");
        return true;
    }

    // Snapshot prerequisites may still be missing during sync fallback.
    core::LogDebug("chunk",
                   "RebuildMesh: deferred for chunk (%d, %d) - snapshot prerequisites are not ready",
                   m_chunkCoords.x, m_chunkCoords.y);

    return false;
}

void Chunk::SetMesh(std::unique_ptr<ChunkMesh> mesh)
{
    m_mesh    = std::move(mesh);
    m_isDirty = false;
}

ChunkMesh* Chunk::GetMesh() const
{
    return m_mesh.get();
}

bool Chunk::NeedsMeshRebuild() const
{
    return m_isDirty;
}

bool Chunk::CanPublishMesh() const
{
    return GetState() == ChunkState::Active;
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
BlockState* Chunk::GetBlock(const BlockPos& worldPos)
{
    int32_t localX, localY, localZ;
    if (!WorldToLocal(worldPos, localX, localY, localZ)) // Convert world coordinates to local coordinates in chunk
    {
        // The coordinates are not within this chunk range, and return the default air block state
        // TODO: Here, an AIR BlockState should be returned, and the default constructed state should be returned.
        // return BlockState{};
        return nullptr; // Return nullptr for now
    }

    return GetBlock(localX, localY, localZ); // If within range, call GetBlock to return BlockState
}

BlockState* Chunk::GetTopBlock(const BlockPos& worldPos)
{
    int32_t localX, localY, localZ;
    if (!WorldToLocal(worldPos, localX, localY, localZ)) // Convert world coordinates to local coordinates in chunk
    {
        // The coordinates are not within this chunk range, and return the default air block state
        // TODO: Here, an AIR BlockState should be returned, and the default constructed state should be returned.
        // return BlockState{};
        return nullptr; // Return nullptr for now
    }
    for (int z = worldPos.z; z < 0; --z)
    {
        BlockState* block = GetBlock(localX, localY, z);
        if (block || block != AIR->GetDefaultState())
        {
            return block;
        }
    }
    return nullptr;
}

void Chunk::SetBlockWorld(const BlockPos& worldPos, BlockState* state)
{
    int32_t localX, localY, localZ;
    if (!WorldToLocal(worldPos, localX, localY, localZ))
    {
        // The coordinates are not within this chunk range and cannot be set. Return directly
        // In actual implementation, warnings or errors may need to be recorded
        return;
    }

    SetBlock(localX, localY, localZ, state);
    // Note: SetBlock already marks chunk as dirty for mesh rebuild (world generation, no save needed)
}

void Chunk::SetBlockWorldByPlayer(const BlockPos& worldPos, BlockState* state)
{
    int32_t localX, localY, localZ;
    if (!WorldToLocal(worldPos, localX, localY, localZ))
    {
        // The coordinates are not within this chunk range and cannot be set. Return directly
        return;
    }

    SetBlockByPlayer(localX, localY, localZ, state);
    // Note: SetBlockByPlayer already marks chunk as both modified and dirty (player action)
}

int Chunk::GetTopBlockZ(const BlockPos& worldPos)
{
    int32_t localX, localY, localZ;
    if (!WorldToLocal(worldPos, localX, localY, localZ)) // Convert world coordinates to local coordinates in chunk
    {
        // The coordinates are not within this chunk range, and return the default air block state
        // TODO: Here, an AIR BlockState should be returned, and the default constructed state should be returned.
        // return BlockState{};
        return -1; // Return nullptr for now
    }

    for (int z = localZ; z >= 0; --z)
    {
        BlockState* block = GetBlock(localX, localY, z);
        if (block && block != AIR->GetDefaultState())
        {
            return z;
        }
    }
    return -1;
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
    int32_t worldX = ChunkCoordsToWorld(m_chunkCoords.x) + x; // chunk world X << 4 + local X (forward)
    int32_t worldY = ChunkCoordsToWorld(m_chunkCoords.y) + y; // chunk world Y << 4 + local Y (left)
    int32_t worldZ = z; // Z coordinate is height, no chunk offset needed

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
    int32_t chunkX = worldPos.x / CHUNK_SIZE_X; // X is forward direction
    int32_t chunkY = worldPos.y / CHUNK_SIZE_Y; // Y is left direction

    // Handle the special case of negative number coordinates
    if (worldPos.x < 0 && worldPos.x % CHUNK_SIZE_X != 0) chunkX--;
    if (worldPos.y < 0 && worldPos.y % CHUNK_SIZE_Y != 0) chunkY--;

    // Check whether it belongs to the current chunk
    if (chunkX != m_chunkCoords.x || chunkY != m_chunkCoords.y)
    {
        return false; // Not part of the current chunk
    }

    // Calculate local coordinates (world coordinates - chunk starting world coordinates)
    x = worldPos.x - ChunkCoordsToWorld(m_chunkCoords.x); // Local X = World X - chunk Start X (forward)
    y = worldPos.y - ChunkCoordsToWorld(m_chunkCoords.y); // Local Y = World Y - chunk Start Y (left)
    z = worldPos.z; // Z coordinate is height, remains unchanged

    // Verify that local coordinates are in the valid range [0, CHUNK_SIZE_X) and [0, CHUNK_SIZE_Y) and [0, CHUNK_SIZE_Z)
    if (x < 0 || x >= CHUNK_SIZE_X || y < 0 || y >= CHUNK_SIZE_Y || z < 0 || z >= CHUNK_SIZE_Z)
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

void Chunk::Update(float deltaTime)
{
    UNUSED(deltaTime)
}

void Chunk::DebugDraw(IRenderer* renderer)
{
    std::vector<Vertex_PCU> outVertices;
    AddVertsForCube3DWireFrame(outVertices, m_chunkBounding, Rgba8::WHITE, 0.06f);
    renderer->SetModelConstants(Mat44());
    renderer->DrawVertexArray(outVertices);
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
    int32_t worldX = ChunkCoordsToWorld(m_chunkCoords.x); // The world start X coordinate of chunk (forward)
    int32_t worldY = ChunkCoordsToWorld(m_chunkCoords.y); // The world start Y coordinate of chunk (left)
    int32_t worldZ = 0; // Start at the bottom of the world (Z is height, starts at 0)

    return BlockPos(worldX, worldY, worldZ);
}

//-------------------------------------------------------------------------------------------
// [Phase 2] State Management - State Transition Detection
//-------------------------------------------------------------------------------------------

/**
 * @brief Set chunk state with activation event detection
 *
 * When chunk transitions from non-Active to Active state, notifies World that
 * this chunk became readable for meshing. World then handles waiting-target
 * wakeups before any generic neighbor mesh refresh propagation.
 *
 * Architecture:
 * - Task 2.1: BuildMesh() returns nullptr if neighbor not active
 * - Task 2.2: SetState() detects activation → notifies world coordination
 * - World wakes dependency waits before generic neighbor refresh propagation
 * - Task 2.1: BuildMesh() now sees active neighbor → correct culling
 *
 * Thread Safety:
 * - AtomicChunkState::Load/Store are thread-safe
 * - NotifyChunkBecameMeshReadable() must run on main thread
 *   (World mesh queue and wait registry are main-thread owned)
 *
 * @param newState New chunk state to set
 */
void Chunk::SetState(ChunkState newState)
{
    ChunkState oldState = m_state.Load();
    m_state.Store(newState);

    // Activation makes this chunk readable for meshing again.
    // World coordinates dependency wakes first, then generic neighbor refresh.
    if (oldState != ChunkState::Active && newState == ChunkState::Active)
    {
        core::LogDebug("chunk", "Chunk (%d, %d) activated (state %s -> %s), notifying world mesh readiness coordination",
                       m_chunkCoords.x, m_chunkCoords.y,
                       ChunkStateToString(oldState), ChunkStateToString(newState));
        NotifyChunkBecameMeshReadable();
    }
}

//-------------------------------------------------------------------------------------------
// [A05] Neighbor Chunk Access for BlockIterator
//-------------------------------------------------------------------------------------------

Chunk* Chunk::GetNorthNeighbor() const
{
    if (!m_world) return nullptr;
    return m_world->GetChunk(m_chunkCoords.x, m_chunkCoords.y + 1);
}

Chunk* Chunk::GetSouthNeighbor() const
{
    if (!m_world) return nullptr;
    return m_world->GetChunk(m_chunkCoords.x, m_chunkCoords.y - 1);
}

Chunk* Chunk::GetEastNeighbor() const
{
    if (!m_world) return nullptr;
    return m_world->GetChunk(m_chunkCoords.x + 1, m_chunkCoords.y);
}

Chunk* Chunk::GetWestNeighbor() const
{
    if (!m_world) return nullptr;
    return m_world->GetChunk(m_chunkCoords.x - 1, m_chunkCoords.y);
}

//-------------------------------------------------------------------------------------------
// [A05] Lighting System - Initialization
//-------------------------------------------------------------------------------------------

/**
 * @brief Initialize lighting values for all blocks in the chunk
 *
 * This is the first step (Task 6.1) of the lighting initialization pipeline.
 * Sets all blocks to default lighting state (outdoor=0, indoor=0, not dirty).
 *
 * Full Pipeline (Assignment 05):
 * - Task 6.1: Default all lighting to 0 (this method)
 * - Task 6.2: Mark boundary blocks as dirty
 * - Task 6.3: Set SKY flag for top-exposed blocks
 * - Task 6.4: Initialize sky light (15 for SKY blocks)
 * - Task 6.5: Scan for emissive blocks and set indoor light
 *
 * @param world World pointer (reserved for future tasks, currently unused)
 */
void Chunk::InitializeLighting(World* world)
{
    // Step 1: Default all blocks to lighting=0, clear both per-engine dirty flags
    for (int i = 0; i < BLOCKS_PER_CHUNK; ++i)
    {
        int32_t x, y, z;
        IndexToCoords(i, x, y, z);
        SetSkyLight(x, y, z, 0);
        SetBlockLight(x, y, z, 0);
        SetIsBlockLightDirty(x, y, z, false);
        SetIsSkyLightDirty(x, y, z, false);
    }

    // Step 2: Mark boundary blocks as dirty
    MarkBoundaryBlocksDirty(world);

    // Step 3: Set SKY flags and skylight=15 directly (no BFS for sky blocks)
    // Sky blocks always have light level 15 - set directly without BFS queue
    for (int x = 0; x < CHUNK_SIZE_X; ++x)
    {
        for (int y = 0; y < CHUNK_SIZE_Y; ++y)
        {
            for (int z = CHUNK_MAX_Z; z >= 0; --z)
            {
                BlockState* state = GetBlock(x, y, z);
                if (state->GetBlock()->IsOpaque(state))
                {
                    break;
                }
                SetIsSky(x, y, z, true);
                SetSkyLight(x, y, z, 15);
            }
        }
    }

    // Step 3b: Seed BFS with sky-light frontier blocks only
    // Frontier = non-sky, non-opaque blocks directly adjacent to a sky block
    // These are cave/overhang entry points where skylight propagates inward
    // (~100-500 blocks per chunk vs ~49K if all sky blocks were queued)
    for (int x = 0; x < CHUNK_SIZE_X; ++x)
    {
        for (int y = 0; y < CHUNK_SIZE_Y; ++y)
        {
            for (int z = CHUNK_MAX_Z; z >= 0; --z)
            {
                if (GetIsSky(x, y, z))
                {
                    continue;
                }

                BlockState* state = GetBlock(x, y, z);
                if (state->GetBlock()->IsOpaque(state))
                {
                    continue;
                }

                // Check if adjacent to any sky block within this chunk
                bool adjacentToSky = false;
                if (z < CHUNK_MAX_Z && GetIsSky(x, y, z + 1)) adjacentToSky = true;
                if (!adjacentToSky && z > 0 && GetIsSky(x, y, z - 1)) adjacentToSky = true;
                if (!adjacentToSky && x > 0 && GetIsSky(x - 1, y, z)) adjacentToSky = true;
                if (!adjacentToSky && x < CHUNK_MAX_X && GetIsSky(x + 1, y, z)) adjacentToSky = true;
                if (!adjacentToSky && y > 0 && GetIsSky(x, y - 1, z)) adjacentToSky = true;
                if (!adjacentToSky && y < CHUNK_MAX_Y && GetIsSky(x, y + 1, z)) adjacentToSky = true;

                if (adjacentToSky)
                {
                    BlockIterator iter(this, (int)CoordsToIndex(x, y, z));
                    world->MarkLightingDirty(iter);
                }
            }
        }
    }

    // Step 4: Mark light-emitting blocks dirty (CORRECTED - mark self only)
    for (int i = 0; i < BLOCKS_PER_CHUNK; ++i)
    {
        BlockState* state         = m_blocks[i];
        uint8_t     emissionLevel = state->GetBlock()->GetBlockLightEmission();
        if (emissionLevel > 0)
        {
            BlockIterator iter(this, i);
            world->MarkLightingDirty(iter);
        }
    }
}

//-------------------------------------------------------------------------------------------
// [Phase 2] Neighbor Notification - Cross-Chunk Hidden Face Culling
//-------------------------------------------------------------------------------------------

/**
 * @brief Notify World that this chunk became readable for meshing
 *
 * World owns both waiting-registry wakeup and generic neighbor refresh
 * coordination. Chunk only reports the readability transition.
 */
void Chunk::NotifyChunkBecameMeshReadable()
{
    if (!m_world)
    {
        core::LogWarn("chunk", "NotifyChunkBecameMeshReadable: m_world is null, cannot notify world");
        return;
    }

    m_world->HandleChunkBecameMeshReadable(*this);
}

//-------------------------------------------------------------------------------------------
// [A05] Lighting System - Boundary Block Marking
//-------------------------------------------------------------------------------------------

/**
 * @brief Mark boundary blocks as dirty for lighting propagation
 *
 * This method scans all 4 horizontal chunk boundaries (East, West, North, South)
 * and marks non-opaque blocks as dirty if the neighboring chunk is active.
 * These blocks need lighting updates because they may receive light from neighbors.
 *
 * Architecture:
 * - Only processes boundaries where neighbor is active (light can propagate)
 * - Scans all blocks on each boundary face (16x256 blocks per face)
 * - Uses BlockIterator for cross-chunk boundary handling
 * - Marks blocks via World::MarkLightingDirty() (global dirty queue)
 *
 * Performance:
 * - Called once per chunk during InitializeLighting()
 * - Up to 4 boundaries × 16×256 blocks = 16,384 checks per chunk
 * - Only marks non-opaque blocks (typically <10% of boundary blocks)
 *
 * @param world World pointer for accessing MarkLightingDirty()
 */
void Chunk::MarkBoundaryBlocksDirty(World* world)
{
    // Get 4 horizontal neighbors
    Chunk* eastNeighbor  = GetEastNeighbor();
    Chunk* westNeighbor  = GetWestNeighbor();
    Chunk* northNeighbor = GetNorthNeighbor();
    Chunk* southNeighbor = GetSouthNeighbor();

    // East boundary (x = CHUNK_MAX_X = 15)
    if (eastNeighbor && eastNeighbor->IsActive())
    {
        // [Step 1] Mark this chunk's East boundary (x=15)
        for (int y = 0; y < CHUNK_SIZE_Y; ++y)
        {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z)
            {
                BlockState* state = GetBlock(CHUNK_MAX_X, y, z);
                // [UPDATED] Use per-state opacity check for non-full blocks (slabs/stairs)
                if (!state->GetBlock()->IsOpaque(state))
                {
                    BlockIterator iter(this, (int)CoordsToIndex(CHUNK_MAX_X, y, z));
                    world->MarkLightingDirty(iter);
                }
            }
        }

        // [Step 2] Mark neighbor chunk's West boundary (x=0)
        for (int y = 0; y < CHUNK_SIZE_Y; ++y)
        {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z)
            {
                BlockState* state = eastNeighbor->GetBlock(0, y, z);
                // [UPDATED] Use per-state opacity check for non-full blocks (slabs/stairs)
                if (!state->GetBlock()->IsOpaque(state))
                {
                    BlockIterator iter(eastNeighbor, (int)eastNeighbor->CoordsToIndex(0, y, z));
                    world->MarkLightingDirty(iter);
                }
            }
        }
    }

    // West boundary (x = 0)
    if (westNeighbor && westNeighbor->IsActive())
    {
        // [Step 1] Mark this chunk's West boundary (x=0)
        for (int y = 0; y < CHUNK_SIZE_Y; ++y)
        {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z)
            {
                BlockState* state = GetBlock(0, y, z);
                // [UPDATED] Use per-state opacity check for non-full blocks (slabs/stairs)
                if (!state->GetBlock()->IsOpaque(state))
                {
                    BlockIterator iter(this, (int)CoordsToIndex(0, y, z));
                    world->MarkLightingDirty(iter);
                }
            }
        }

        // [Step 2] Mark neighbor chunk's East boundary (x=15)
        for (int y = 0; y < CHUNK_SIZE_Y; ++y)
        {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z)
            {
                BlockState* state = westNeighbor->GetBlock(CHUNK_MAX_X, y, z);
                // [UPDATED] Use per-state opacity check for non-full blocks (slabs/stairs)
                if (!state->GetBlock()->IsOpaque(state))
                {
                    BlockIterator iter(westNeighbor, (int)westNeighbor->CoordsToIndex(CHUNK_MAX_X, y, z));
                    world->MarkLightingDirty(iter);
                }
            }
        }
    }

    // North boundary (y = CHUNK_MAX_Y = 15)
    if (northNeighbor && northNeighbor->IsActive())
    {
        // [Step 1] Mark this chunk's North boundary (y=15)
        for (int x = 0; x < CHUNK_SIZE_X; ++x)
        {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z)
            {
                BlockState* state = GetBlock(x, CHUNK_MAX_Y, z);
                // [UPDATED] Use per-state opacity check for non-full blocks (slabs/stairs)
                if (!state->GetBlock()->IsOpaque(state))
                {
                    BlockIterator iter(this, (int)CoordsToIndex(x, CHUNK_MAX_Y, z));
                    world->MarkLightingDirty(iter);
                }
            }
        }

        // [Step 2] Mark neighbor chunk's South boundary (y=0)
        for (int x = 0; x < CHUNK_SIZE_X; ++x)
        {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z)
            {
                BlockState* state = northNeighbor->GetBlock(x, 0, z);
                // [UPDATED] Use per-state opacity check for non-full blocks (slabs/stairs)
                if (!state->GetBlock()->IsOpaque(state))
                {
                    BlockIterator iter(northNeighbor, (int)northNeighbor->CoordsToIndex(x, 0, z));
                    world->MarkLightingDirty(iter);
                }
            }
        }
    }

    // South boundary (y = 0)
    if (southNeighbor && southNeighbor->IsActive())
    {
        // [Step 1] Mark this chunk's South boundary (y=0)
        for (int x = 0; x < CHUNK_SIZE_X; ++x)
        {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z)
            {
                BlockState* state = GetBlock(x, 0, z);
                // [UPDATED] Use per-state opacity check for non-full blocks (slabs/stairs)
                if (!state->GetBlock()->IsOpaque(state))
                {
                    BlockIterator iter(this, (int)CoordsToIndex(x, 0, z));
                    world->MarkLightingDirty(iter);
                }
            }
        }

        // [Step 2] Mark neighbor chunk's North boundary (y=15)
        for (int x = 0; x < CHUNK_SIZE_X; ++x)
        {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z)
            {
                BlockState* state = southNeighbor->GetBlock(x, CHUNK_MAX_Y, z);
                // [UPDATED] Use per-state opacity check for non-full blocks (slabs/stairs)
                if (!state->GetBlock()->IsOpaque(state))
                {
                    BlockIterator iter(southNeighbor, (int)southNeighbor->CoordsToIndex(x, CHUNK_MAX_Y, z));
                    world->MarkLightingDirty(iter);
                }
            }
        }
    }
}

//-------------------------------------------------------------------------------------------
// [A05] Lighting Data Access - Independent Storage Implementation
//-------------------------------------------------------------------------------------------

/**
 * @brief Get outdoor light level (0-15) from independent storage
 *
 * Outdoor light is stored in the high 4 bits of m_lightData[index].
 * This method extracts the high 4 bits using bit shift and mask operations.
 *
 * @param x Local X coordinate (0-15)
 * @param y Local Y coordinate (0-15)
 * @param z Local Z coordinate (0-255)
 * @return Outdoor light level (0-15)
 */
uint8_t Chunk::GetSkyLight(int32_t x, int32_t y, int32_t z) const
{
    size_t index = CoordsToIndex(x, y, z);
    return (m_lightData[index] >> 4) & 0x0F; // High 4 bits
}

/**
 * @brief Set outdoor light level (0-15) in independent storage
 *
 * Outdoor light is stored in the high 4 bits of m_lightData[index].
 * This method preserves the low 4 bits (indoor light) while updating high 4 bits.
 *
 * @param x Local X coordinate (0-15)
 * @param y Local Y coordinate (0-15)
 * @param z Local Z coordinate (0-255)
 * @param light Outdoor light level (0-15)
 */
void Chunk::SetSkyLight(int32_t x, int32_t y, int32_t z, uint8_t light)
{
    size_t index       = CoordsToIndex(x, y, z);
    m_lightData[index] = (m_lightData[index] & 0x0F) | ((light & 0x0F) << 4);
}

/**
 * @brief Get indoor light level (0-15) from independent storage
 *
 * Indoor light is stored in the low 4 bits of m_lightData[index].
 * This method extracts the low 4 bits using mask operation.
 *
 * @param x Local X coordinate (0-15)
 * @param y Local Y coordinate (0-15)
 * @param z Local Z coordinate (0-255)
 * @return Indoor light level (0-15)
 */
uint8_t Chunk::GetBlockLight(int32_t x, int32_t y, int32_t z) const
{
    size_t index = CoordsToIndex(x, y, z);
    return m_lightData[index] & 0x0F; // Low 4 bits
}

/**
 * @brief Set indoor light level (0-15) in independent storage
 *
 * Indoor light is stored in the low 4 bits of m_lightData[index].
 * This method preserves the high 4 bits (outdoor light) while updating low 4 bits.
 *
 * @param x Local X coordinate (0-15)
 * @param y Local Y coordinate (0-15)
 * @param z Local Z coordinate (0-255)
 * @param light Indoor light level (0-15)
 */
void Chunk::SetBlockLight(int32_t x, int32_t y, int32_t z, uint8_t light)
{
    size_t index       = CoordsToIndex(x, y, z);
    m_lightData[index] = (m_lightData[index] & 0xF0) | (light & 0x0F);
}

/**
 * @brief Get IsSky flag from independent storage
 *
 * IsSky flag is stored in bit 0 of m_flags[index].
 * This flag indicates whether the block is exposed to sky (for outdoor lighting).
 *
 * @param x Local X coordinate (0-15)
 * @param y Local Y coordinate (0-15)
 * @param z Local Z coordinate (0-255)
 * @return true if block is exposed to sky, false otherwise
 */
bool Chunk::GetIsSky(int32_t x, int32_t y, int32_t z) const
{
    size_t index = CoordsToIndex(x, y, z);
    return (m_flags[index] & 0x01) != 0; // Bit 0
}

/**
 * @brief Set IsSky flag in independent storage
 *
 * IsSky flag is stored in bit 0 of m_flags[index].
 * This method uses bit manipulation to set or clear bit 0 while preserving other bits.
 *
 * @param x Local X coordinate (0-15)
 * @param y Local Y coordinate (0-15)
 * @param z Local Z coordinate (0-255)
 * @param value true to set IsSky flag, false to clear it
 */
void Chunk::SetIsSky(int32_t x, int32_t y, int32_t z, bool value)
{
    size_t index = CoordsToIndex(x, y, z);
    if (value)
        m_flags[index] |= 0x01;
    else
        m_flags[index] &= ~0x01;
}

//-------------------------------------------------------------------------------------------
// Per-engine dirty flags (separate bits to avoid cross-engine interference)
// BlockLightEngine uses bit 1 (0x02), SkyLightEngine uses bit 2 (0x04)
//-------------------------------------------------------------------------------------------

bool Chunk::GetIsBlockLightDirty(int32_t x, int32_t y, int32_t z) const
{
    size_t index = CoordsToIndex(x, y, z);
    return (m_flags[index] & 0x02) != 0; // Bit 1
}

void Chunk::SetIsBlockLightDirty(int32_t x, int32_t y, int32_t z, bool value)
{
    size_t index = CoordsToIndex(x, y, z);
    if (value)
        m_flags[index] |= 0x02;
    else
        m_flags[index] &= ~0x02;
}

bool Chunk::GetIsSkyLightDirty(int32_t x, int32_t y, int32_t z) const
{
    size_t index = CoordsToIndex(x, y, z);
    return (m_flags[index] & 0x04) != 0; // Bit 2
}

void Chunk::SetIsSkyLightDirty(int32_t x, int32_t y, int32_t z, bool value)
{
    size_t index = CoordsToIndex(x, y, z);
    if (value)
        m_flags[index] |= 0x04;
    else
        m_flags[index] &= ~0x04;
}

Mat44 Chunk::GetModelToWorldTransform() const
{
    // Get chunk world position (bottom corner)
    BlockPos chunkWorldPos = GetWorldPos();
    // Create model-to-world transform matrix 
    // Chunk coordinates are in block space, so we need to translate to world position
    Mat44 modelToWorld = Mat44::MakeTranslation3D(Vec3((float)chunkWorldPos.x, (float)chunkWorldPos.y, (float)chunkWorldPos.z));
    return modelToWorld;
}
