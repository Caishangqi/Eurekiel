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
    // Same boundary checks and index calculation as SetBlock
    size_t index    = CoordsToIndex(x, y, z);
    m_blocks[index] = state;

    // Mark chunk as both modified for saving AND dirty for mesh rebuild (player action)
    m_isModified     = true;
    m_playerModified = true; // Mark as player-modified for PlayerModifiedOnly save strategy
    m_isDirty        = true;
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
            m_world->MarkChunkDirty(neighbor);
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
