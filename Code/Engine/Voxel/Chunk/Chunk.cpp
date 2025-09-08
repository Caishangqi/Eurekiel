#include "Chunk.hpp"
#include "ChunkMeshBuilder.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Renderer/IRenderer.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Renderer/Model/RenderMesh.hpp"
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Engine/Resource/Atlas/TextureAtlas.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Game/GameCommon.hpp"

using namespace enigma::voxel::chunk;

Chunk::Chunk(IntVec2 chunkCoords) : m_chunkCoords(chunkCoords)
{
    core::LogInfo("chunk", "Chunk created: %d, %d", m_chunkCoords.x, m_chunkCoords.y);

    // Get required block types from registry
    auto airBlock     = enigma::registry::block::BlockRegistry::GetBlock("simpleminer", "air");
    auto grassBlock   = enigma::registry::block::BlockRegistry::GetBlock("simpleminer", "grass");
    auto dirtBlock    = enigma::registry::block::BlockRegistry::GetBlock("simpleminer", "dirt");
    auto stoneBlock   = enigma::registry::block::BlockRegistry::GetBlock("simpleminer", "stone");
    auto waterBlock   = enigma::registry::block::BlockRegistry::GetBlock("simpleminer", "water");
    auto coalBlock    = enigma::registry::block::BlockRegistry::GetBlock("simpleminer", "coal_ore");
    auto ironBlock    = enigma::registry::block::BlockRegistry::GetBlock("simpleminer", "iron_ore");
    auto goldBlock    = enigma::registry::block::BlockRegistry::GetBlock("simpleminer", "gold_ore");
    auto diamondBlock = enigma::registry::block::BlockRegistry::GetBlock("simpleminer", "diamond_ore");

    if (!airBlock || !grassBlock || !dirtBlock || !stoneBlock || !waterBlock ||
        !coalBlock || !ironBlock || !goldBlock || !diamondBlock)
    {
        core::LogError("chunk", "Failed to get required blocks from registry - chunk generation failed");
        return;
    }

    auto* airState     = airBlock->GetDefaultState();
    auto* grassState   = grassBlock->GetDefaultState();
    auto* dirtState    = dirtBlock->GetDefaultState();
    auto* stoneState   = stoneBlock->GetDefaultState();
    auto* waterState   = waterBlock->GetDefaultState();
    auto* coalState    = coalBlock->GetDefaultState();
    auto* ironState    = ironBlock->GetDefaultState();
    auto* goldState    = goldBlock->GetDefaultState();
    auto* diamondState = diamondBlock->GetDefaultState();

    if (!airState || !grassState || !dirtState || !stoneState || !waterState ||
        !coalState || !ironState || !goldState || !diamondState)
    {
        core::LogError("chunk", "Failed to get block states - chunk generation failed");
        return;
    }

    // Initialize blocks storage with air blocks
    m_blocks.reserve(BLOCKS_PER_CHUNK);
    for (size_t i = 0; i < BLOCKS_PER_CHUNK; ++i)
    {
        m_blocks.push_back(*airState);
    }

    // Generate terrain using Assignment01 formula
    for (int32_t localX = 0; localX < CHUNK_SIZE; ++localX)
    {
        for (int32_t localZ = 0; localZ < CHUNK_SIZE; ++localZ)
        {
            // Convert local coordinates to global coordinates
            int32_t globalX = m_chunkCoords.x * CHUNK_SIZE + localX;
            int32_t globalY = m_chunkCoords.y * CHUNK_SIZE + localZ;

            // Calculate terrain height using the formula:
            // terrainHeightZ ( globalX, globalY ) = 50 + (globalX/3) + (globalY/5) + random(0 or 1)
            int32_t terrainHeightZ = 50 + (globalX / 3) + (globalY / 5) + (g_rng->RollRandomIntInRange(0, 2));

            // Clamp terrain height to valid range
            if (terrainHeightZ >= CHUNK_HEIGHT) terrainHeightZ = CHUNK_HEIGHT - 1;
            if (terrainHeightZ < 0) terrainHeightZ = 0;

            // Generate column from bottom to top
            for (int32_t localY = 0; localY < CHUNK_HEIGHT; ++localY)
            {
                size_t blockIndex = localX + localZ * CHUNK_SIZE + localY * CHUNK_SIZE * CHUNK_SIZE;

                if (localY == terrainHeightZ)
                {
                    // Surface block - grass
                    m_blocks[blockIndex] = *grassState;
                }
                else if (localY > terrainHeightZ)
                {
                    // Above surface - air or water
                    if (localY <= CHUNK_HEIGHT / 2)
                    {
                        // Water level - any air below chunk height / 2 becomes water
                        m_blocks[blockIndex] = *waterState;
                    }
                    else
                    {
                        // Above water level - air
                        m_blocks[blockIndex] = *airState;
                    }
                }
                else
                {
                    // Below surface - determine dirt or stone layers
                    int32_t depthBelowSurface  = terrainHeightZ - localY;
                    int32_t dirtLayerThickness = g_rng->RollRandomIntInRange(3, 5);

                    if (depthBelowSurface <= dirtLayerThickness)
                    {
                        // Dirt layer
                        m_blocks[blockIndex] = *dirtState;
                    }
                    else
                    {
                        // Stone layer with ore chances
                        // 5% chance to become "coal"; if not, 2% chance to become "iron"; 
                        // if not, 0.5% chance to become "gold"; if not, 0.1% chance to become "diamond"
                        int32_t oreRoll = g_rng->RollRandomIntInRange(1, 10000); // Use 10000 for precise percentages

                        if (oreRoll <= 500) // 5% = 500/10000
                        {
                            m_blocks[blockIndex] = *coalState;
                        }
                        else if (oreRoll <= 700) // 2% = 200/10000, cumulative 700
                        {
                            m_blocks[blockIndex] = *ironState;
                        }
                        else if (oreRoll <= 750) // 0.5% = 50/10000, cumulative 750
                        {
                            m_blocks[blockIndex] = *goldState;
                        }
                        else if (oreRoll <= 760) // 0.1% = 10/10000, cumulative 760
                        {
                            m_blocks[blockIndex] = *diamondState;
                        }
                        else
                        {
                            // Regular stone
                            m_blocks[blockIndex] = *stoneState;
                        }
                    }
                }
            }
        }
    }

    core::LogInfo("chunk", "Generated terrain for chunk (%d, %d) using Assignment01 formula", m_chunkCoords.x, m_chunkCoords.y);

    /// Calculate chunk bound aabb3
    BlockPos chunkBottomPos = GetWorldPos();
    m_chunkBounding.m_mins  = Vec3((float)chunkBottomPos.x, (float)chunkBottomPos.y, (float)chunkBottomPos.z);
    m_chunkBounding.m_maxs  = m_chunkBounding.m_mins + Vec3(CHUNK_SIZE, CHUNK_SIZE, CHUNK_HEIGHT);

    // Always rebuild mesh, even if chunk is empty
    RebuildMesh();
}

Chunk::~Chunk()
{
}

BlockState Chunk::GetBlock(int32_t x, int32_t y, int32_t z)
{
    // Boundary check
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_HEIGHT)
    {
        core::LogError("chunk", "GetBlock coordinates out of range: (%d,%d,%d)", x, y, z);
        // Return first block as fallback (should be replaced with Air block later)
        return m_blocks.empty() ? BlockState(nullptr, {}, 0) : m_blocks[0];
    }

    // Correct 3D to 1D index calculation: [x][y][z] -> x + y*CHUNK_SIZE + z*CHUNK_SIZE*CHUNK_SIZE
    size_t index = x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;

    if (index >= m_blocks.size())
    {
        core::LogError("chunk", "GetBlock calculated index %zu out of range (size: %zu)", index, m_blocks.size());
        // Return first block as fallback
        return m_blocks.empty() ? BlockState(nullptr, {}, 0) : m_blocks[0];
    }

    return m_blocks[index];
}

void Chunk::SetBlock(int32_t x, int32_t y, int32_t z, BlockState state)
{
    // Boundary check
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_HEIGHT)
    {
        core::LogError("chunk", "SetBlock coordinates out of range: (%d,%d,%d)", x, y, z);
        return;
    }

    // Correct 3D to 1D index calculation: [x][y][z] -> x + y*CHUNK_SIZE + z*CHUNK_SIZE*CHUNK_SIZE
    size_t index = x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;

    if (index >= m_blocks.size())
    {
        core::LogError("chunk", "SetBlock calculated index %zu out of range (size: %zu)", index, m_blocks.size());
        return;
    }

    m_blocks[index] = state;
}

void Chunk::MarkDirty()
{
    m_isDirty = true;
}

void Chunk::RebuildMesh()
{
    // Use ChunkMeshBuilder to create optimized mesh (Assignment01 approach)
    ChunkMeshBuilder builder;
    auto             newMesh = builder.BuildMesh(this);

    if (newMesh)
    {
        // Compile the new mesh to GPU
        newMesh->CompileToGPU();

        // Replace the old mesh with new one
        m_mesh    = std::move(newMesh);
        m_isDirty = false;

        core::LogInfo("chunk", "Chunk mesh rebuilt using ChunkMeshBuilder");
    }
    else
    {
        core::LogError("chunk", "ChunkMeshBuilder failed to create mesh");

        // Fallback: create an empty mesh
        if (!m_mesh)
        {
            m_mesh = std::make_unique<ChunkMesh>();
        }
        m_mesh->Clear();
        m_mesh->CompileToGPU();
        m_isDirty = false;
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

void Chunk::Update(float deltaTime)
{
    UNUSED(deltaTime)
}

void Chunk::Render(IRenderer* renderer) const
{
    if (!m_mesh || m_mesh->IsEmpty())
    {
        ERROR_RECOVERABLE("Chunk::Render No mesh to render")
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
    renderer->SetBlendMode(BlendMode::OPAQUE);

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
    int32_t worldX = m_chunkCoords.x * CHUNK_SIZE; // The world start X coordinate of chunk
    int32_t worldY = m_chunkCoords.y * CHUNK_SIZE; // The world start Y coordinate of chunk
    int32_t worldZ = 0; // Start at the bottom of the world

    return BlockPos(worldX, worldY, worldZ);
}
