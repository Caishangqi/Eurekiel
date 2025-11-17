#include "ChunkMeshHelper.hpp"
#include "Chunk.hpp"
#include "../../Registry/Block/Block.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Renderer/Model/BlockRenderMesh.hpp"
#include "Engine/Math/Mat44.hpp"
#include "../../Voxel/Property/PropertyTypes.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"
#include "../Block/BlockIterator.hpp"

using namespace enigma::voxel;

//--------------------------------------------------------------------------------------------------
// Anonymous namespace for helper functions
//--------------------------------------------------------------------------------------------------
namespace
{
    /**
     * @brief Get directional shade value based on block face direction
     * @details Implements Assignment 05 directional lighting specification:
     *          - EAST:  0.7f (Eastern faces, moderate brightness)
     *          - WEST:  0.6f (Western faces, slightly darker)
     *          - SOUTH: 0.8f (Southern faces, brighter)
     *          - NORTH: 0.75f (Northern faces, moderate-bright)
     *          - UP:    1.0f (Top faces, full brightness)
     *          - DOWN:  0.5f (Bottom faces, darkest)
     * @param direction The direction of the block face
     * @return Shade multiplier in range [0.5f, 1.0f]
     */
    constexpr float GetDirectionalShade(Direction direction)
    {
        switch (direction)
        {
        case Direction::EAST: return 0.7f;
        case Direction::WEST: return 0.6f;
        case Direction::SOUTH: return 0.8f;
        case Direction::NORTH: return 0.75f;
        case Direction::UP: return 1.0f;
        case Direction::DOWN: return 0.5f;
        default: return 1.0f; // Fallback: full brightness
        }
    }

    /**
     * @brief Light data structure containing normalized outdoor and indoor light values
     * @details Stores dual-channel lighting information from a block:
     *          - outdoorLight: Sky light from above (0.0 = dark, 1.0 = full sunlight)
     *          - indoorLight: Block light from emissive blocks (0.0 = no emission, 1.0 = max emission)
     */
    struct LightingData
    {
        float outdoorLight; // Normalized outdoor light [0.0, 1.0]
        float indoorLight; // Normalized indoor light [0.0, 1.0]
    };

    /**
     * @brief Get normalized lighting values from a neighbor block
     * 
     * Retrieves dual-channel light values from the specified neighbor block and normalizes
     * them to [0.0, 1.0] range. Provides boundary-safe access with defensive checks.
     * 
     * @param neighborIter Neighbor block iterator (obtained via BlockIterator::GetNeighbor())
     * @return LightingData Normalized lighting data
     *         - outdoorLight: Outdoor light intensity 0.0-1.0 (0=completely dark, 1.0=brightest)
     *         - indoorLight: Indoor light intensity 0.0-1.0 (from emissive blocks like glowstone)
     * 
     * BOUNDARY SAFETY:
     * - Returns {0.0f, 0.0f} if neighbor iterator is invalid (out of world bounds)
     * - Returns {0.0f, 0.0f} if neighbor chunk is not loaded
     * - Ensures safe access across chunk boundaries
     * 
     * IMPLEMENTATION DETAILS:
     * - Light values retrieved from Chunk independent storage (m_lightData array)
     * - Storage format: high 4 bits = outdoor light, low 4 bits = indoor light
     * - Normalization formula: lightLevel / 15.0f (original range 0-15)
     * 
     * @note Uses Chunk::GetOutdoorLight() and Chunk::GetIndoorLight() APIs
     * @note Follows DRY principle to avoid repeated boundary checks in mesh building code
     */
    LightingData GetNeighborLighting(const BlockIterator& neighborIter)
    {
        // [FIX] Minecraft-style minimum ambient light (light level 1 = 1/15 ≈ 6.67%)
        // This prevents completely black faces in unlit areas while maintaining lighting contrast
        // Reference: Assignment 05 - Lighting System Bug Fix (2025-11-16)
        LightingData result = {1.0f / 15.0f, 0.0f};

        // Boundary check 1: Iterator validity (not out of world bounds)
        if (!neighborIter.IsValid())
        {
            return result;
        }

        // Boundary check 2: Neighbor chunk is loaded
        Chunk* neighborChunk = neighborIter.GetChunk();
        if (neighborChunk == nullptr)
        {
            return result;
        }

        // Extract neighbor block's local coordinates (chunk-local 0-15)
        int32_t x, y, z;
        neighborIter.GetLocalCoords(x, y, z);

        // Query dual-channel light values (raw range 0-15)
        uint8_t outdoorRaw = neighborChunk->GetOutdoorLight(x, y, z);
        uint8_t indoorRaw  = neighborChunk->GetIndoorLight(x, y, z);

        // Normalize to [0.0, 1.0] (divide by max value 15)
        result.outdoorLight = static_cast<float>(outdoorRaw) / 15.0f;
        result.indoorLight  = static_cast<float>(indoorRaw) / 15.0f;
#undef max
        // [FIX] Ensure minimum brightness (light level 1) for visibility
        // This guarantees all block faces are at least slightly visible
        float totalLight = std::max(result.outdoorLight, result.indoorLight);
        if (totalLight < 1.0f / 15.0f)
        {
            result.outdoorLight = 1.0f / 15.0f;
        }

        return result;
    }
} // anonymous namespace

std::unique_ptr<ChunkMesh> ChunkMeshHelper::BuildMesh(Chunk* chunk)
{
    // Static local variable to cache air block reference (C++11 thread-safe initialization)
    static auto air = registry::block::BlockRegistry::GetBlock("simpleminer", "air");

    // ===== Phase 4: 入口状态检查（防护点1） =====
    if (!chunk)
    {
        core::LogWarn("ChunkMeshHelper", "Attempted to build mesh for null chunk");
        return nullptr;
    }

    ChunkState state = chunk->GetState();
    if (state != ChunkState::Active)
    {
        core::LogDebug("ChunkMeshHelper", "BuildMesh: chunk not in valid state (state=%s), aborting", chunk->GetStateName());
        return nullptr;
    }

    //--------------------------------------------------------------------------------------------------
    // Phase 2: 跨边界隐藏面剔除 - 4邻居激活检查
    //
    // 问题：如果邻居Chunk未激活，边界Block的ShouldRenderFace()会返回true（保守策略），
    //       导致不必要的面被渲染。
    //
    // 解决：延迟Mesh构建直到4个水平邻居全部激活。当邻居激活时，Chunk::SetState()会
    //       通知本Chunk重建Mesh（参见Task 2.2）。
    //
    // 性能影响：微小（仅4次指针检查 + 4次bool检查）
    // 收益：边界Block顶点数减少30-40%
    //--------------------------------------------------------------------------------------------------
    Chunk* eastNeighbor  = chunk->GetEastNeighbor();
    Chunk* westNeighbor  = chunk->GetWestNeighbor();
    Chunk* northNeighbor = chunk->GetNorthNeighbor();
    Chunk* southNeighbor = chunk->GetSouthNeighbor();

    // [FIX] Assignment requirement: "Only construct meshes for chunks with all 4 neighbors active"
    // Must check IsActive(), not just pointer existence
    bool hasAllActiveNeighbors =
        eastNeighbor != nullptr && eastNeighbor->IsActive() &&
        westNeighbor != nullptr && westNeighbor->IsActive() &&
        northNeighbor != nullptr && northNeighbor->IsActive() &&
        southNeighbor != nullptr && southNeighbor->IsActive();

    if (!hasAllActiveNeighbors)
    {
        // 邻居未激活，延迟Mesh构建（稍后由邻居激活时触发重建）
        core::LogDebug("ChunkMeshHelper",
                       "BuildMesh: skipping chunk (%d, %d) - not all 4 neighbors are active (E=%s W=%s N=%s S=%s)",
                       chunk->GetChunkX(), chunk->GetChunkY(),
                       (eastNeighbor && eastNeighbor->IsActive()) ? "OK" : "NO",
                       (westNeighbor && westNeighbor->IsActive()) ? "OK" : "NO",
                       (northNeighbor && northNeighbor->IsActive()) ? "OK" : "NO",
                       (southNeighbor && southNeighbor->IsActive()) ? "OK" : "NO"
        );
        return nullptr;
    }

    auto chunkMesh  = std::make_unique<ChunkMesh>();
    int  blockCount = 0;

    core::LogInfo("ChunkMeshHelper", "Building mesh for chunk...");

    // Assignment 2: Two-pass optimization
    // First pass: Count visible faces to pre-allocate memory
    size_t opaqueQuadCount      = 0;
    size_t transparentQuadCount = 0;

    // First pass: Count quads needed
    static const std::vector<enigma::voxel::Direction> allDirections = {
        enigma::voxel::Direction::NORTH,
        enigma::voxel::Direction::SOUTH,
        enigma::voxel::Direction::EAST,
        enigma::voxel::Direction::WEST,
        enigma::voxel::Direction::UP,
        enigma::voxel::Direction::DOWN
    };

    for (int x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
    {
        for (int y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
        {
            for (int z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
            {
                int           blockIndex = x + (y << Chunk::CHUNK_BITS_X) + (z << (Chunk::CHUNK_BITS_X + Chunk::CHUNK_BITS_Y));
                BlockIterator iterator(chunk, blockIndex);
                BlockState*   blockState = iterator.GetBlock();

                if (ShouldRenderBlock(blockState, air))
                {
                    for (const auto& direction : allDirections)
                    {
                        if (ShouldRenderFace(iterator, direction))
                        {
                            opaqueQuadCount++;
                        }
                    }
                }
            }
        }
    }

    // Pre-allocate memory based on count
    chunkMesh->Reserve(opaqueQuadCount, transparentQuadCount);

    // Second pass: Actually build the mesh
    for (int x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
    {
        for (int y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
        {
            for (int z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
            {
                if (chunk->GetState() != ChunkState::Active) return nullptr;

                int           blockIndex = x + (y << Chunk::CHUNK_BITS_X) + (z << (Chunk::CHUNK_BITS_X + Chunk::CHUNK_BITS_Y));
                BlockIterator iterator(chunk, blockIndex);
                BlockState*   blockState = iterator.GetBlock();

                if (ShouldRenderBlock(blockState, air))
                {
                    BlockPos blockPos = GetBlockPosition(x, y, z);
                    AddBlockToMesh(chunkMesh.get(), blockState, blockPos, iterator);
                    blockCount++;
                }
            }
        }
    }

    core::LogInfo("ChunkMeshHelper", "Chunk mesh built successfully. Blocks: %d, Vertices: %zu, Triangles: %zu",
                  blockCount, chunkMesh->GetOpaqueVertexCount(), chunkMesh->GetOpaqueTriangleCount());

    return chunkMesh;
}

void ChunkMeshHelper::RebuildMesh(Chunk* chunk)
{
    if (!chunk)
    {
        return;
    }

    // Build new mesh
    auto newMesh = BuildMesh(chunk);

    // Set the new mesh on the chunk
    if (newMesh)
    {
        newMesh->CompileToGPU(); // Compile before setting
        chunk->SetMesh(std::move(newMesh));
        core::LogInfo("ChunkMeshHelper", "Successfully rebuilt and set mesh for chunk");
    }
    else
    {
        core::LogError("ChunkMeshHelper", "Failed to rebuild mesh for chunk");
    }
}

void ChunkMeshHelper::AddBlockToMesh(ChunkMesh* chunkMesh, BlockState* blockState, const BlockPos& blockPos, const BlockIterator& iterator)
{
    if (!chunkMesh || !blockState)
    {
        return;
    }

    Chunk* chunk = iterator.GetChunk();
    if (!chunk || chunk->GetState() != ChunkState::Active)
    {
        core::LogDebug("ChunkMeshHelper", "AddBlockToMesh: chunk invalid or not Active, aborting");
        return;
    }

    auto blockRenderMesh = blockState->GetRenderMesh();
    if (!blockRenderMesh || blockRenderMesh->IsEmpty())
    {
        return;
    }

    static const std::vector<enigma::voxel::Direction> allDirections = {
        enigma::voxel::Direction::NORTH,
        enigma::voxel::Direction::SOUTH,
        enigma::voxel::Direction::EAST,
        enigma::voxel::Direction::WEST,
        enigma::voxel::Direction::UP,
        enigma::voxel::Direction::DOWN
    };

    Vec3  blockPosVec3(static_cast<float>(blockPos.x), static_cast<float>(blockPos.y), static_cast<float>(blockPos.z));
    Mat44 blockToChunkTransform = Mat44::MakeTranslation3D(blockPosVec3);

    for (const auto& direction : allDirections)
    {
        if (chunk->GetState() != ChunkState::Active)
        {
            core::LogDebug("ChunkMeshHelper", "AddBlockToMesh: chunk state changed during face iteration, aborting");
            return;
        }

        if (!ShouldRenderFace(iterator, direction))
        {
            continue;
        }

        const auto* renderFace = blockRenderMesh->GetFace(direction);
        if (!renderFace || renderFace->vertices.empty())
        {
            continue; // Skip faces without geometry
        }

        // Transform vertices from block space to chunk space
        std::vector<Vertex_PCU> transformedVertices((int)renderFace->vertices.size(), Vertex_PCU());

        // [Phase 8] Get neighbor lighting values for this face
        BlockIterator neighborIter     = iterator.GetNeighbor(direction);
        LightingData  lighting         = GetNeighborLighting(neighborIter);
        float         directionalShade = GetDirectionalShade(direction);

        // Calculate vertex color from lighting data (manual normalization to uint8_t)
        uint8_t r = static_cast<uint8_t>(lighting.outdoorLight * 255.0f);
        uint8_t g = static_cast<uint8_t>(lighting.indoorLight * 255.0f);
        uint8_t b = static_cast<uint8_t>(directionalShade * 255.0f);
        Rgba8   vertexColor(r, g, b, 255);

        for (int i = 0; i < (int)renderFace->vertices.size(); ++i)
        {
            Vertex_PCU transformedVertex = renderFace->vertices[i];
            // Transform position from block space (0,0,0)-(1,1,1) to chunk space
            transformedVertex.m_position = blockToChunkTransform.TransformPosition3D(renderFace->vertices[i].m_position);
            // [Phase 8] Set vertex color encoding lighting (R=outdoor, G=indoor, B=directional shade)
            transformedVertex.m_color = vertexColor;
            transformedVertices[i]    = transformedVertex;
        }

        // Convert vertices to quads and add to chunk mesh
        // Assuming each face has 4 vertices arranged as a quad
        if (transformedVertices.size() >= 4)
        {
            std::array<Vertex_PCU, 4> quad = {
                transformedVertices[0],
                transformedVertices[1],
                transformedVertices[2],
                transformedVertices[3]
            };

            // Add the quad to chunk mesh
            // For Assignment01, we treat all geometry as opaque
            chunkMesh->AddOpaqueQuad(quad);
        }
        else if (transformedVertices.size() > 0)
        {
            core::LogWarn("ChunkMeshHelper", "Face has %zu vertices, expected 4 for quad conversion", transformedVertices.size());
        }
    }
}

bool ChunkMeshHelper::ShouldRenderBlock(BlockState* blockState, const std::shared_ptr<registry::block::Block>& air)
{
    if (!blockState)
    {
        return false;
    }

    // Assignment 1 simplified: render all non-air blocks
    // Air blocks should have a specific registry name or be null

    if (!blockState->GetBlock())
    {
        return false; // No block type (air)
    }

    // Don't render air blocks
    if (blockState->GetBlock() == air.get())
    {
        return false;
    }

    return true;
}

bool ChunkMeshHelper::ShouldRenderFace(const BlockIterator& iterator, Direction direction)
{
    // Get neighbor block in the specified direction
    BlockIterator neighborIterator = iterator.GetNeighbor(direction);

    // Render face if neighbor is invalid (chunk boundary or unloaded chunk)
    if (!neighborIterator.IsValid())
    {
        return true;
    }

    // Get neighbor block state
    BlockState* neighborBlock = neighborIterator.GetBlock();

    // Render face if neighbor is air (null pointer)
    if (!neighborBlock)
    {
        return true;
    }

    // [Phase 3] Use IsFullOpaque flag for precise face culling
    // If neighbor is fully opaque, this face is hidden and should not be rendered
    if (neighborBlock->IsFullOpaque())
    {
        return false;
    }

    // Render face for translucent or non-full blocks (water, glass, etc.)
    return true;
}

BlockPos ChunkMeshHelper::GetBlockPosition(int x, int y, int z)
{
    return BlockPos(x, y, z);
}
