#include "Chunk.hpp"
#include "ChunkMeshHelper.hpp"
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

Chunk::~Chunk()
{
    m_mesh.release();
}

BlockState* Chunk::GetBlock(int32_t x, int32_t y, int32_t z)
{
    // Optimized bit-shift index calculation: index = x + (y << CHUNK_BITS_X) + (z << (CHUNK_BITS_X + CHUNK_BITS_Y))
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
    BlockState* oldState  = GetBlock(x, y, z);
    bool        wasOpaque = oldState->IsFullOpaque();
    bool        wasSky    = GetIsSky(x, y, z);

    // 2. Set new block
    size_t index    = CoordsToIndex(x, y, z);
    m_blocks[index] = state;

    // 3. Mark chunk as modified and dirty
    m_isModified     = true;
    m_playerModified = true;
    m_isDirty        = true;

    // 4. Get new block properties
    bool isOpaque = state->IsFullOpaque();

    if (m_world)
    {
        BlockIterator iter(this, index);

        // ===== [NEW] SKY Flag Propagation (Assignment 05 requirements) =====

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
                        if (currentBlock->IsFullOpaque())
                        {
                            break; // Stop at first opaque block
                        }

                        // Flag as SKY and mark dirty
                        SetIsSky(x, y, descendZ, true);
                        SetOutdoorLight(x, y, descendZ, 15);

                        BlockIterator descendIter(this, CoordsToIndex(x, y, descendZ));
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
            SetOutdoorLight(x, y, z, 0);

            // Descend downward, clearing SKY flags
            for (int descendZ = z - 1; descendZ >= 0; --descendZ)
            {
                BlockState* currentBlock = GetBlock(x, y, descendZ);
                if (currentBlock->IsFullOpaque())
                {
                    break; // Stop at first opaque block
                }

                // Clear SKY flag and mark dirty
                SetIsSky(x, y, descendZ, false);
                SetOutdoorLight(x, y, descendZ, 0);

                BlockIterator descendIter(this, CoordsToIndex(x, y, descendZ));
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

void Chunk::RebuildMesh()
{
    // Use ChunkMeshHelper to create optimized mesh (Assignment01 approach)
    auto newMesh = ChunkMeshHelper::BuildMesh(this);

    if (newMesh)
    {
        // Compile the new mesh to GPU
        newMesh->CompileToGPU();

        // Replace the old mesh with new one
        m_mesh    = std::move(newMesh);
        m_isDirty = false;

        core::LogInfo("chunk", "Chunk mesh rebuilt using ChunkMeshHelper");
    }
    else
    {
        //--------------------------------------------------------------------------------------------------
        // Phase 2: 跨边界隐藏面剔除 - 修复空mesh fallback问题
        //
        // 问题：之前的逻辑将BuildMesh()返回nullptr视为错误，创建空mesh作为fallback。
        //       但nullptr是合法的延迟（等待邻居激活），不应创建空mesh。
        //
        // 解决：允许Active的Chunk暂时无mesh，保持m_isDirty = true允许重试。
        //       当邻居激活时，Chunk::SetState()会通知本Chunk重建（Task 2.2）。
        //
        // Assignment 05要求："You may need to change your code to allow for active chunks 
        //                     that don't have meshes"
        //--------------------------------------------------------------------------------------------------

        // BuildMesh返回nullptr是合法的延迟（等待邻居加载），不是错误
        core::LogDebug("chunk",
                       "RebuildMesh: delayed for chunk (%d, %d) - waiting for neighbors to load",
                       m_chunkCoords.x, m_chunkCoords.y);

        // 保持m_isDirty = true，下次UpdateChunkMeshes()会重试
        // 不创建空mesh，允许Active的Chunk暂时无mesh（符合Assignment 05要求）
        return;
    }
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

void Chunk::Render(IRenderer* renderer) const
{
    if (!m_mesh || m_mesh->IsEmpty())
    {
        //ERROR_RECOVERABLE("Chunk::Render No mesh to render")
        // No mesh to render
        return;
    }

    // Get chunk world position (bottom corner)
    BlockPos chunkWorldPos = GetWorldPos();

    // Create model-to-world transform matrix 
    // Chunk coordinates are in block space, so we need to translate to world position
    Mat44 modelToWorldTransform = Mat44::MakeTranslation3D(
        Vec3((float)chunkWorldPos.x, (float)chunkWorldPos.y, (float)chunkWorldPos.z)
    );

    // Set model constants (similar to Prop.cpp:23)
    // Using white color for now - could be made configurable
    renderer->SetModelConstants(modelToWorldTransform, Rgba8::WHITE);

    // Set rendering state
    //renderer->SetBlendMode(BlendMode::OPAQUE);

    // Note: Texture binding is now handled by ChunkManager to avoid per-chunk queries
    // ChunkManager binds the blocks atlas texture once for all chunks (NeoForge pattern)

    // Render the chunk mesh
    m_mesh->RenderAll(renderer);
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
 * When chunk transitions from non-Active to Active state, notifies 4 horizontal
 * neighbors to rebuild their mesh. This ensures boundary blocks correctly update
 * their face visibility for cross-chunk hidden face culling.
 *
 * Architecture:
 * - Task 2.1: BuildMesh() returns nullptr if neighbor not active
 * - Task 2.2: SetState() detects activation → notifies neighbors
 * - World::UpdateChunkMeshes() rebuilds neighbors from dirty queue
 * - Task 2.1: BuildMesh() now sees active neighbor → correct culling
 *
 * Thread Safety:
 * - AtomicChunkState::Load/Store are thread-safe
 * - NotifyNeighborsDirty() must run on main thread
 *   (World::MarkChunkDirty accesses non-thread-safe queue)
 *
 * @param newState New chunk state to set
 */
void Chunk::SetState(ChunkState newState)
{
    ChunkState oldState = m_state.Load();
    m_state.Store(newState);

    // [Phase 2] Cross-Chunk Hidden Face Culling
    // When chunk activates, neighbors need to rebuild mesh
    // (their BuildMesh() previously returned nullptr because this chunk was not active)
    if (oldState != ChunkState::Active && newState == ChunkState::Active)
    {
        core::LogDebug("chunk", "Chunk (%d, %d) activated (state %s -> %s), notifying neighbors to rebuild mesh",
                       m_chunkCoords.x, m_chunkCoords.y,
                       ChunkStateToString(oldState), ChunkStateToString(newState));
        NotifyNeighborsDirty();
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
    // Step 1: Default all blocks to lighting=0, not dirty
    // [NEW] Use Chunk's independent storage instead of shared BlockState
    for (int i = 0; i < BLOCKS_PER_CHUNK; ++i)
    {
        int32_t x, y, z;
        IndexToCoords(i, x, y, z);
        SetOutdoorLight(x, y, z, 0);
        SetIndoorLight(x, y, z, 0);
        SetIsLightDirty(x, y, z, false);
    }

    // Step 2: Mark boundary blocks as dirty
    MarkBoundaryBlocksDirty(world);

    // Step 3: Set SKY flags and outdoor light (CORRECTED - no neighbor marking)
    for (int x = 0; x < CHUNK_SIZE_X; ++x)
    {
        for (int y = 0; y < CHUNK_SIZE_Y; ++y)
        {
            for (int z = CHUNK_MAX_Z; z >= 0; --z)
            {
                BlockState* state = GetBlock(x, y, z);
                if (state->IsFullOpaque())
                {
                    break; // Stop at first opaque block
                }
                SetIsSky(x, y, z, true);
                SetOutdoorLight(x, y, z, 15);
            }
        }
    }

    // Step 4: Mark light-emitting blocks dirty (CORRECTED - mark self only)
    for (int i = 0; i < BLOCKS_PER_CHUNK; ++i)
    {
        BlockState* state         = m_blocks[i];
        uint8_t     emissionLevel = state->GetBlock()->GetIndoorLightEmission();
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
 * @brief Notify 4 horizontal neighbors to rebuild mesh
 *
 * Called when this chunk activates. Neighbors that are already active need to
 * rebuild their mesh because boundary block faces may change visibility.
 *
 * Why not notify in constructor?
 * - Chunk creation happens during terrain generation (neighbors may not exist yet)
 * - Only activation matters (neighbors might already be loaded by then)
 *
 * Why not notify Up/Down neighbors?
 * - Chunk height is 256 blocks (Z=0 to Z=255), covers entire world height
 * - No vertical chunk neighbors exist in SimpleMiner
 *
 * Performance:
 * - Low frequency event (only when chunk activates)
 * - 4 pointer lookups + up to 4 MarkChunkDirty() calls (~100ns)
 * - World::MarkChunkDirty() automatically de-duplicates (std::find check)
 *
 * @see World::MarkChunkDirty() - Adds chunk to m_pendingMeshRebuildQueue
 * @see World::UpdateChunkMeshes() - Processes dirty queue on main thread
 */
void Chunk::NotifyNeighborsDirty()
{
    if (!m_world)
    {
        core::LogWarn("chunk", "NotifyNeighborsDirty: m_world is null, cannot notify neighbors");
        return;
    }

    // Get 4 horizontal neighbors (Up/Down don't exist - Z in same chunk)
    Chunk* neighbors[] = {
        GetEastNeighbor(), // X + 1
        GetWestNeighbor(), // X - 1
        GetNorthNeighbor(), // Y + 1
        GetSouthNeighbor() // Y - 1
    };

    const char* neighborNames[] = {"East", "West", "North", "South"};

    int notifiedCount = 0;
    for (int i = 0; i < 4; ++i)
    {
        Chunk* neighbor = neighbors[i];
        if (neighbor && neighbor->IsActive())
        {
            // Add neighbor to World's dirty queue for mesh rebuild
            // World::MarkChunkDirty() already checks for duplicates (std::find)
            m_world->ScheduleChunkMeshRebuild(neighbor);
            notifiedCount++;

            core::LogDebug("chunk", "  -> Marked %s neighbor (%d, %d) dirty",
                           neighborNames[i],
                           neighbor->GetChunkX(),
                           neighbor->GetChunkY());
        }
    }

    core::LogDebug("chunk", "NotifyNeighborsDirty: notified %d active neighbors (E=%s W=%s N=%s S=%s)",
                   notifiedCount,
                   (neighbors[0] && neighbors[0]->IsActive()) ? "OK" : "NO",
                   (neighbors[1] && neighbors[1]->IsActive()) ? "OK" : "NO",
                   (neighbors[2] && neighbors[2]->IsActive()) ? "OK" : "NO",
                   (neighbors[3] && neighbors[3]->IsActive()) ? "OK" : "NO");
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
                if (!state->IsFullOpaque())
                {
                    BlockIterator iter(this, CoordsToIndex(CHUNK_MAX_X, y, z));
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
                if (!state->IsFullOpaque())
                {
                    BlockIterator iter(eastNeighbor, eastNeighbor->CoordsToIndex(0, y, z));
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
                if (!state->IsFullOpaque())
                {
                    BlockIterator iter(this, CoordsToIndex(0, y, z));
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
                if (!state->IsFullOpaque())
                {
                    BlockIterator iter(westNeighbor, westNeighbor->CoordsToIndex(CHUNK_MAX_X, y, z));
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
                if (!state->IsFullOpaque())
                {
                    BlockIterator iter(this, CoordsToIndex(x, CHUNK_MAX_Y, z));
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
                if (!state->IsFullOpaque())
                {
                    BlockIterator iter(northNeighbor, northNeighbor->CoordsToIndex(x, 0, z));
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
                if (!state->IsFullOpaque())
                {
                    BlockIterator iter(this, CoordsToIndex(x, 0, z));
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
                if (!state->IsFullOpaque())
                {
                    BlockIterator iter(southNeighbor, southNeighbor->CoordsToIndex(x, CHUNK_MAX_Y, z));
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
uint8_t Chunk::GetOutdoorLight(int32_t x, int32_t y, int32_t z) const
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
void Chunk::SetOutdoorLight(int32_t x, int32_t y, int32_t z, uint8_t light)
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
uint8_t Chunk::GetIndoorLight(int32_t x, int32_t y, int32_t z) const
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
void Chunk::SetIndoorLight(int32_t x, int32_t y, int32_t z, uint8_t light)
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

/**
 * @brief Get IsLightDirty flag from independent storage
 *
 * IsLightDirty flag is stored in bit 1 of m_flags[index].
 * This flag indicates whether the block's lighting needs to be recalculated.
 *
 * @param x Local X coordinate (0-15)
 * @param y Local Y coordinate (0-15)
 * @param z Local Z coordinate (0-255)
 * @return true if block lighting is dirty, false otherwise
 */
bool Chunk::GetIsLightDirty(int32_t x, int32_t y, int32_t z) const
{
    size_t index = CoordsToIndex(x, y, z);
    return (m_flags[index] & 0x02) != 0; // Bit 1
}

/**
 * @brief Set IsLightDirty flag in independent storage
 *
 * IsLightDirty flag is stored in bit 1 of m_flags[index].
 * This method uses bit manipulation to set or clear bit 1 while preserving other bits.
 *
 * @param x Local X coordinate (0-15)
 * @param y Local Y coordinate (0-15)
 * @param z Local Z coordinate (0-255)
 * @param value true to mark lighting as dirty, false to clear dirty flag
 */
void Chunk::SetIsLightDirty(int32_t x, int32_t y, int32_t z, bool value)
{
    size_t index = CoordsToIndex(x, y, z);
    if (value)
        m_flags[index] |= 0x02;
    else
        m_flags[index] &= ~0x02;
}
