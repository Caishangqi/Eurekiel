#include "ChunkMeshHelper.hpp"
#include "Chunk.hpp"
#include "../../Registry/Block/Block.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Renderer/Model/BlockRenderMesh.hpp"
#include "Engine/Math/Mat44.hpp"
#include "../../Voxel/Property/PropertyTypes.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"
#include "../Block/BlockIterator.hpp"
#include "../World/TerrainVertexLayout.hpp"
#include "Engine/Registry/Block/RenderType.hpp"
#include "Engine/Registry/Block/RenderShape.hpp"

using namespace enigma::voxel;
using namespace enigma::registry::block;

//--------------------------------------------------------------------------------------------------
// Anonymous namespace for helper functions
//--------------------------------------------------------------------------------------------------
namespace
{
    /**
     * @brief Face normal lookup table for deferred rendering G-Buffer output
     * @details Engine coordinate system: +X Forward, +Y Left, +Z Up
     *          Array index matches Direction enum order
     */
    static const Vec3 FACE_NORMALS[6] = {
        Vec3(0.0f, 1.0f, 0.0f), // NORTH (0): +Y (Left)
        Vec3(0.0f, -1.0f, 0.0f), // SOUTH (1): -Y (Right)
        Vec3(1.0f, 0.0f, 0.0f), // EAST  (2): +X (Forward)
        Vec3(-1.0f, 0.0f, 0.0f), // WEST  (3): -X (Backward)
        Vec3(0.0f, 0.0f, 1.0f), // UP    (4): +Z (Up)
        Vec3(0.0f, 0.0f, -1.0f), // DOWN  (5): -Z (Down)
    };

    /**
     * @brief Get outward-facing normal for a block face
     * @param direction Block face direction
     * @return Normal vector pointing outward from the face
     */
    static Vec3 GetFaceNormal(Direction direction)
    {
        return FACE_NORMALS[static_cast<int>(direction)];
    }

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
     *          - skyLight: Sky light from above (0.0 = dark, 1.0 = full sunlight)
     *          - blockLight: Block light from emissive blocks (0.0 = no emission, 1.0 = max emission)
     */
    struct LightingData
    {
        float skyLight; // Normalized outdoor light [0.0, 1.0]
        float blockLight; // Normalized indoor light [0.0, 1.0]
    };

    /**
     * @brief Get normalized lighting values from a neighbor block
     * 
     * Retrieves dual-channel light values from the specified neighbor block and normalizes
     * them to [0.0, 1.0] range. Provides boundary-safe access with defensive checks.
     * 
     * @param neighborIter Neighbor block iterator (obtained via BlockIterator::GetNeighbor())
     * @return LightingData Normalized lighting data
     *         - skyLight: Outdoor light intensity 0.0-1.0 (0=completely dark, 1.0=brightest)
     *         - blockLight: Indoor light intensity 0.0-1.0 (from emissive blocks like glowstone)
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
     * @note Uses Chunk::GetSkyLight() and Chunk::GetBlockLight() APIs
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
        uint8_t outdoorRaw = neighborChunk->GetSkyLight(x, y, z);
        uint8_t indoorRaw  = neighborChunk->GetBlockLight(x, y, z);

        // Normalize to [0.0, 1.0] (divide by max value 15)
        result.skyLight   = static_cast<float>(outdoorRaw) / 15.0f;
        result.blockLight = static_cast<float>(indoorRaw) / 15.0f;
#undef max
        // [FIX] Ensure minimum brightness (light level 1) for visibility
        // This guarantees all block faces are at least slightly visible
        float totalLight = std::max(result.skyLight, result.blockLight);
        if (totalLight < 1.0f / 15.0f)
        {
            result.skyLight = 1.0f / 15.0f;
        }

        return result;
    }
} // anonymous namespace

std::unique_ptr<ChunkMesh> ChunkMeshHelper::BuildMesh(Chunk* chunk)
{
    // Static local variable to cache air block reference (C++11 thread-safe initialization)
    static auto air = registry::block::BlockRegistry::GetBlock("simpleminer", "air");

    // ===== Phase 4: Entry state check (guard point 1) =====
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
    // Phase 2: Cross-boundary hidden face culling - 4 neighbor activation check
    //--------------------------------------------------------------------------------------------------
    Chunk* eastNeighbor  = chunk->GetEastNeighbor();
    Chunk* westNeighbor  = chunk->GetWestNeighbor();
    Chunk* northNeighbor = chunk->GetNorthNeighbor();
    Chunk* southNeighbor = chunk->GetSouthNeighbor();

    bool hasAllActiveNeighbors =
        eastNeighbor != nullptr && eastNeighbor->IsActive() &&
        westNeighbor != nullptr && westNeighbor->IsActive() &&
        northNeighbor != nullptr && northNeighbor->IsActive() &&
        southNeighbor != nullptr && southNeighbor->IsActive();

    if (!hasAllActiveNeighbors)
    {
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

    // [REFACTORED] Three-pass optimization: Count visible faces for each render type
    size_t opaqueQuadCount      = 0;
    size_t cutoutQuadCount      = 0;
    size_t translucentQuadCount = 0;

    static const std::vector<enigma::voxel::Direction> allDirections = {
        enigma::voxel::Direction::NORTH,
        enigma::voxel::Direction::SOUTH,
        enigma::voxel::Direction::EAST,
        enigma::voxel::Direction::WEST,
        enigma::voxel::Direction::UP,
        enigma::voxel::Direction::DOWN
    };

    // First pass: Count quads needed for each render type
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
                    // [REFACTORED] Get render type from block
                    RenderType renderType = GetBlockRenderType(blockState);

                    for (const auto& direction : allDirections)
                    {
                        if (ShouldRenderFace(iterator, direction))
                        {
                            switch (renderType)
                            {
                            case RenderType::SOLID:
                                opaqueQuadCount++;
                                break;
                            case RenderType::CUTOUT:
                                cutoutQuadCount++;
                                break;
                            case RenderType::TRANSLUCENT:
                                translucentQuadCount++;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // Pre-allocate memory based on count
    chunkMesh->Reserve(opaqueQuadCount, cutoutQuadCount, translucentQuadCount);

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

    core::LogInfo("ChunkMeshHelper", "Chunk mesh built: Blocks=%d, Opaque=%zu, Cutout=%zu, Translucent=%zu",
                  blockCount, chunkMesh->GetOpaqueVertexCount() / 4,
                  chunkMesh->GetCutoutVertexCount() / 4,
                  chunkMesh->GetTranslucentVertexCount() / 4);

    return chunkMesh;
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

    // [DEBUG] Track block type for slab/stairs debugging
    std::string blockName    = blockState->GetBlock() ? blockState->GetBlock()->GetRegistryName() : "null";
    bool        isDebugBlock = (blockName.find("slab") != std::string::npos || blockName.find("stairs") != std::string::npos);

    auto blockRenderMesh = blockState->GetRenderMesh();

    if (!blockRenderMesh || blockRenderMesh->IsEmpty())
    {
        if (isDebugBlock)
        {
            core::LogWarn("ChunkMeshHelper", "[DEBUG] Skipping %s: mesh is null or empty!", blockName.c_str());
        }
        return;
    }

    // [REFACTORED] Get render type for this block
    RenderType renderType = GetBlockRenderType(blockState);

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

    // [DEBUG] Track face processing for slab/stairs
    int facesAdded             = 0;
    int facesSkippedCull       = 0;
    int facesSkippedNoGeometry = 0;

    for (const auto& direction : allDirections)
    {
        if (chunk->GetState() != ChunkState::Active)
        {
            core::LogDebug("ChunkMeshHelper", "AddBlockToMesh: chunk state changed during face iteration, aborting");
            return;
        }

        if (!ShouldRenderFace(iterator, direction))
        {
            if (isDebugBlock) facesSkippedCull++;
            continue;
        }

        // [FIX] Use GetFaces() to handle multi-element models (stairs have 11 faces)
        auto renderFaces = blockRenderMesh->GetFaces(direction);

        if (renderFaces.empty())
        {
            if (isDebugBlock)
            {
                core::LogWarn("ChunkMeshHelper", "[DEBUG] Direction %d: GetFaces() returned 0 faces",
                              static_cast<int>(direction));
                facesSkippedNoGeometry++;
            }
            continue;
        }

        // Process ALL faces for this direction (not just the first one)
        for (const auto* renderFace : renderFaces)
        {
            if (!renderFace || renderFace->vertices.empty())
            {
                if (isDebugBlock) facesSkippedNoGeometry++;
                continue;
            }

            // [OPTIMIZED] Skip faces with insufficient vertices for quad
            if (renderFace->vertices.size() < 4)
            {
                core::LogWarn("ChunkMeshHelper", "Face has %zu vertices, expected 4 for quad conversion", renderFace->vertices.size());
                continue;
            }

            // [Phase 8] Get neighbor lighting values for this face
            BlockIterator neighborIter = iterator.GetNeighbor(direction);
            LightingData  lighting     = GetNeighborLighting(neighborIter);

            // [FIX] Directional shading via vertex color (Minecraft-style)
            float   directionalShade = GetDirectionalShade(direction);
            uint8_t shade            = static_cast<uint8_t>(directionalShade * 255.0f);
            Rgba8   vertexColor(shade, shade, shade, 255);

            // Get face normal and lightmap coordinates for G-Buffer output
            Vec3 faceNormal = GetFaceNormal(direction);
            Vec2 lightmapCoord(lighting.blockLight, lighting.skyLight);

            // [OPTIMIZED] Single-pass: directly build TerrainVertex quad
            std::array<graphic::TerrainVertex, 4> terrainQuad;
            for (int vi = 0; vi < 4; ++vi)
            {
                const Vertex_PCU& srcVertex = renderFace->vertices[vi];

                terrainQuad[vi].m_position      = blockToChunkTransform.TransformPosition3D(srcVertex.m_position);
                terrainQuad[vi].m_color         = vertexColor;
                terrainQuad[vi].m_uvTexCoords   = srcVertex.m_uvTextCoords;
                terrainQuad[vi].m_normal        = faceNormal;
                terrainQuad[vi].m_lightmapCoord = lightmapCoord;
            }

            // [REFACTORED] Route to appropriate mesh based on render type
            switch (renderType)
            {
            case RenderType::SOLID:
                chunkMesh->AddOpaqueTerrainQuad(terrainQuad);
                break;
            case RenderType::CUTOUT:
                chunkMesh->AddCutoutTerrainQuad(terrainQuad);
                break;
            case RenderType::TRANSLUCENT:
                chunkMesh->AddTranslucentTerrainQuad(terrainQuad);
                break;
            }
            if (isDebugBlock) facesAdded++;
        }
    }

    // [DEBUG] Log summary for slab/stairs blocks
    if (isDebugBlock)
    {
        core::LogInfo("ChunkMeshHelper", "[DEBUG] %s: faces_added=%d, culled=%d, no_geometry=%d",
                      blockName.c_str(), facesAdded, facesSkippedCull, facesSkippedNoGeometry);
    }
}

bool ChunkMeshHelper::ShouldRenderBlock(BlockState* blockState, const std::shared_ptr<registry::block::Block>& air)
{
    if (!blockState)
    {
        return false;
    }

    if (!blockState->GetBlock())
    {
        return false; // No block type (air)
    }

    // Don't render air blocks
    if (blockState->GetBlock() == air.get())
    {
        return false;
    }

    // [REFACTORED] Check RenderShape - INVISIBLE blocks are handled by dedicated renderers
    // [MINECRAFT REF] LiquidBlock.getRenderShape() returns INVISIBLE
    RenderShape renderShape = blockState->GetBlock()->GetRenderShape(blockState);
    if (renderShape == RenderShape::INVISIBLE)
    {
        return false;
    }

    return true;
}

bool ChunkMeshHelper::ShouldRenderFace(const BlockIterator& iterator, Direction direction)
{
    // Get current block state
    BlockState* currentBlock = iterator.GetBlock();
    if (!currentBlock || !currentBlock->GetBlock())
    {
        return false;
    }

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
    if (!neighborBlock || !neighborBlock->GetBlock())
    {
        return true;
    }

    // [REFACTORED] Check Block::SkipRendering for same-type culling
    // [MINECRAFT REF] HalfTransparentBlock.skipRendering - same block type culling
    // [MINECRAFT REF] LiquidBlock.skipRendering - same fluid type culling
    if (currentBlock->GetBlock()->SkipRendering(currentBlock, neighborBlock, direction))
    {
        return false;
    }

    // [FIX] Get current block's render type for proper face culling
    // Cutout/Translucent blocks should NOT be culled by opaque neighbors
    // because they have transparent parts that need to show the neighbor behind
    RenderType currentRenderType = currentBlock->GetBlock()->GetRenderType();

    // [REFACTORED] Use CanOcclude() for Minecraft-style face culling
    // [MINECRAFT REF] Block.shouldRenderFace() core logic:
    //   if (neighborState.canOcclude()) { /* compare VoxelShapes */ }
    //   else { return true; /* neighbor can't occlude, must render face */ }
    // 
    // Only cull faces when neighbor CanOcclude() == true (full solid blocks)
    // Cutout blocks (leaves) and Translucent blocks (glass) have canOcclude=false
    if (neighborBlock->CanOcclude())
    {
        // Only cull if current block is also SOLID (fully opaque rendering)
        if (currentRenderType == RenderType::SOLID)
        {
            return false;
        }
        // Cutout and Translucent blocks: always render face against opaque neighbors
        // This ensures leaves render properly against logs/dirt
        return true;
    }

    // Render face for translucent or non-full blocks (water, glass, etc.)
    return true;
}

// ============================================================
// [NEW] GetBlockRenderType - Determine render pass for a block
// [MINECRAFT REF] ItemBlockRenderTypes classification
// ============================================================
RenderType ChunkMeshHelper::GetBlockRenderType(BlockState* blockState)
{
    if (!blockState || !blockState->GetBlock())
    {
        return RenderType::SOLID;
    }

    // Get render type from block's GetRenderType() method
    // This is set by:
    // - Block subclass override (LeavesBlock -> CUTOUT, HalfTransparentBlock -> TRANSLUCENT)
    // - YAML configuration (render_type: cutout/translucent/opaque)
    return blockState->GetBlock()->GetRenderType();
}

BlockPos ChunkMeshHelper::GetBlockPosition(int x, int y, int z)
{
    return BlockPos(x, y, z);
}
