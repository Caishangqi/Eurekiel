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
#include "../Fluid/FluidState.hpp"

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

    // ========================================================================
    // Ambient Occlusion (AO) Calculation
    // ========================================================================
    // [MINECRAFT REF] Smooth lighting / AO algorithm
    // For each vertex of a face, check 3 adjacent blocks (corner + 2 edges)
    // AO value is determined by the number of occluding blocks
    //
    // Vertex layout for a face (looking at face from outside):
    //   v3 ---- v2
    //   |       |
    //   |       |
    //   v0 ---- v1
    //
    // For each vertex, we check:
    //   - side1: one edge neighbor
    //   - side2: other edge neighbor  
    //   - corner: diagonal neighbor
    //
    // AO formula (Minecraft-style):
    //   if (side1 && side2) ao = 0 (fully occluded corner)
    //   else ao = 3 - (side1 + side2 + corner)
    //   Normalized: ao / 3.0
    // ========================================================================

    /**
     * @brief AO lookup table for vertex brightness
     * @details Maps occlusion count (0-3) to brightness value (0.0-1.0)
     *          0 occluders = 1.0 (full bright)
     *          1 occluder  = 0.7
     *          2 occluders = 0.5
     *          3 occluders = 0.2 (darkest)
     */
    static const float AO_CURVE[4] = {1.0f, 0.7f, 0.5f, 0.2f};

    /**
     * @brief Check if a block at given iterator position can occlude light for AO
     * @param iter Block iterator to check
     * @return true if block is solid and can occlude, false otherwise
     */
    static bool IsOccluder(const BlockIterator& iter)
    {
        if (!iter.IsValid())
        {
            return false;
        }

        BlockState* block = iter.GetBlock();
        if (!block || !block->GetBlock())
        {
            return false; // Air or invalid block
        }

        return block->CanOcclude();
    }

    /**
     * @brief Calculate AO value for a single vertex based on 3 neighbor blocks
     * @param side1 First edge neighbor is occluder
     * @param side2 Second edge neighbor is occluder
     * @param corner Corner neighbor is occluder
     * @return AO value in range [0.2, 1.0]
     *
     * [MINECRAFT REF] vertexAO algorithm from smooth lighting
     * Special case: if both sides are occluders, corner is fully occluded
     */
    static float CalculateVertexAO(bool side1, bool side2, bool corner)
    {
        int occluderCount;
        if (side1 && side2)
        {
            // Both sides occlude = corner is fully dark (can't see corner block)
            occluderCount = 3;
        }
        else
        {
            occluderCount = (side1 ? 1 : 0) + (side2 ? 1 : 0) + (corner ? 1 : 0);
        }
        return AO_CURVE[occluderCount];
    }

    // ========================================================================
    // [FLIPPED QUADS] Adaptive Quad Triangulation for Smooth AO
    // ========================================================================
    // [SODIUM REF] ModelQuadOrientation.java - orientByBrightness() method
    // File: net/caffeinemc/mods/sodium/client/model/quad/properties/ModelQuadOrientation.java
    //
    // When a quad has non-uniform AO values at its 4 corners, the triangulation
    // direction affects how the GPU interpolates the AO across the face.
    // Fixed triangulation (always 0-2 diagonal) can cause visible "diagonal crease"
    // artifacts when AO values are anisotropic.
    //
    // Solution: Compare the sum of AO values along both diagonals:
    //   - Diagonal 0-2: ao[0] + ao[2]
    //   - Diagonal 1-3: ao[1] + ao[3]
    // Choose the diagonal with the HIGHER sum (brighter) as the split line.
    // This produces smoother interpolation results.
    //
    // [SODIUM REF] Vertex index remapping:
    //   NORMAL: {0, 1, 2, 3} - triangles: (0,1,2) and (0,2,3) - split along 0-2
    //   FLIP:   {1, 2, 3, 0} - triangles: (1,2,3) and (1,3,0) - split along 1-3
    // ========================================================================

    /**
     * @brief Determine if quad triangulation should be flipped based on AO values
     * @param aoValues Array of 4 AO values for quad vertices [v0, v1, v2, v3]
     * @return true if quad should use FLIP triangulation (1-3 diagonal),
     *         false for NORMAL triangulation (0-2 diagonal)
     *
     * [SODIUM REF] ModelQuadOrientation.orientByBrightness()
     * File: net/caffeinemc/mods/sodium/client/model/quad/properties/ModelQuadOrientation.java
     * Lines 19-45
     */
    static bool ShouldFlipQuad(const float aoValues[4])
    {
        // Calculate brightness sum for each diagonal
        float brightness02 = aoValues[0] + aoValues[2]; // Diagonal 0-2
        float brightness13 = aoValues[1] + aoValues[3]; // Diagonal 1-3

        // [SODIUM REF] Choose diagonal with higher brightness sum
        // If 0-2 is brighter or equal, use NORMAL (no flip)
        // If 1-3 is brighter, use FLIP
        return brightness13 > brightness02;
    }

    /**
     * @brief Offset table for AO neighbor sampling
     * @details For each face direction, defines the offsets to sample for each vertex's
     *          side1, side2, and corner neighbors.
     *
     * Index: [direction][vertex][neighbor_type]
     * - direction: 0-5 (NORTH, SOUTH, EAST, WEST, UP, DOWN)
     * - vertex: 0-3 (v0, v1, v2, v3 in CCW order)
     * - neighbor_type: 0=side1, 1=side2, 2=corner
     *
     * Each offset is {dx, dy, dz} relative to the block position
     */
    struct AOOffset
    {
        int dx, dy, dz;
    };

    // ========================================================================
    // [ENGINE COORDINATE SYSTEM] +X Forward, +Y Left, +Z Up
    // ========================================================================
    // [FIX] AO offset tables must match BlockModelCompiler::CreateElementFace vertex order!
    //
    // BlockModelCompiler vertex order (from CreateElementFace):
    //   EAST (+X):  v0=(to.x,to.y,from.z), v1=(to.x,to.y,to.z), v2=(to.x,from.y,to.z), v3=(to.x,from.y,from.z)
    //   WEST (-X):  v0=(from.x,from.y,from.z), v1=(from.x,from.y,to.z), v2=(from.x,to.y,to.z), v3=(from.x,to.y,from.z)
    //   NORTH (+Y): v0=(from.x,to.y,from.z), v1=(from.x,to.y,to.z), v2=(to.x,to.y,to.z), v3=(to.x,to.y,from.z)
    //   SOUTH (-Y): v0=(to.x,from.y,from.z), v1=(to.x,from.y,to.z), v2=(from.x,from.y,to.z), v3=(from.x,from.y,from.z)
    //   UP (+Z):    v0=(from.x,from.y,to.z), v1=(to.x,from.y,to.z), v2=(to.x,to.y,to.z), v3=(from.x,to.y,to.z)
    //   DOWN (-Z):  v0=(from.x,from.y,from.z), v1=(from.x,to.y,from.z), v2=(to.x,to.y,from.z), v3=(to.x,from.y,from.z)
    //
    // For standard block: from=(0,0,0), to=(1,1,1)
    // ========================================================================

    // UP face (+Z): vertices at z=1
    // v0=(0,0,1)=西南, v1=(1,0,1)=东南, v2=(1,1,1)=东北, v3=(0,1,1)=西北
    static const AOOffset AO_OFFSETS_UP[4][3] = {
        // v0: 西南角 (x=0, y=0, z=1)
        {{-1, 0, 1}, {0, -1, 1}, {-1, -1, 1}}, // side1=west(-X), side2=south(-Y), corner=west-south
        // v1: 东南角 (x=1, y=0, z=1)
        {{1, 0, 1}, {0, -1, 1}, {1, -1, 1}}, // side1=east(+X), side2=south(-Y), corner=east-south
        // v2: 东北角 (x=1, y=1, z=1)
        {{1, 0, 1}, {0, 1, 1}, {1, 1, 1}}, // side1=east(+X), side2=north(+Y), corner=east-north
        // v3: 西北角 (x=0, y=1, z=1)
        {{-1, 0, 1}, {0, 1, 1}, {-1, 1, 1}}, // side1=west(-X), side2=north(+Y), corner=west-north
    };

    // DOWN face (-Z): vertices at z=0
    // v0=(0,0,0)=西南, v1=(0,1,0)=西北, v2=(1,1,0)=东北, v3=(1,0,0)=东南
    static const AOOffset AO_OFFSETS_DOWN[4][3] = {
        // v0: 西南角 (x=0, y=0, z=0)
        {{-1, 0, -1}, {0, -1, -1}, {-1, -1, -1}}, // side1=west, side2=south, corner=west-south
        // v1: 西北角 (x=0, y=1, z=0)
        {{-1, 0, -1}, {0, 1, -1}, {-1, 1, -1}}, // side1=west, side2=north, corner=west-north
        // v2: 东北角 (x=1, y=1, z=0)
        {{1, 0, -1}, {0, 1, -1}, {1, 1, -1}}, // side1=east, side2=north, corner=east-north
        // v3: 东南角 (x=1, y=0, z=0)
        {{1, 0, -1}, {0, -1, -1}, {1, -1, -1}}, // side1=east, side2=south, corner=east-south
    };

    // NORTH face (+Y): vertices at y=1
    // v0=(0,1,0)=西下, v1=(0,1,1)=西上, v2=(1,1,1)=东上, v3=(1,1,0)=东下
    static const AOOffset AO_OFFSETS_NORTH[4][3] = {
        // v0: 西下角 (x=0, y=1, z=0)
        {{-1, 1, 0}, {0, 1, -1}, {-1, 1, -1}}, // side1=west, side2=down, corner=west-down
        // v1: 西上角 (x=0, y=1, z=1)
        {{-1, 1, 0}, {0, 1, 1}, {-1, 1, 1}}, // side1=west, side2=up, corner=west-up
        // v2: 东上角 (x=1, y=1, z=1)
        {{1, 1, 0}, {0, 1, 1}, {1, 1, 1}}, // side1=east, side2=up, corner=east-up
        // v3: 东下角 (x=1, y=1, z=0)
        {{1, 1, 0}, {0, 1, -1}, {1, 1, -1}}, // side1=east, side2=down, corner=east-down
    };

    // SOUTH face (-Y): vertices at y=0
    // v0=(1,0,0)=东下, v1=(1,0,1)=东上, v2=(0,0,1)=西上, v3=(0,0,0)=西下
    static const AOOffset AO_OFFSETS_SOUTH[4][3] = {
        // v0: 东下角 (x=1, y=0, z=0)
        {{1, -1, 0}, {0, -1, -1}, {1, -1, -1}}, // side1=east, side2=down, corner=east-down
        // v1: 东上角 (x=1, y=0, z=1)
        {{1, -1, 0}, {0, -1, 1}, {1, -1, 1}}, // side1=east, side2=up, corner=east-up
        // v2: 西上角 (x=0, y=0, z=1)
        {{-1, -1, 0}, {0, -1, 1}, {-1, -1, 1}}, // side1=west, side2=up, corner=west-up
        // v3: 西下角 (x=0, y=0, z=0)
        {{-1, -1, 0}, {0, -1, -1}, {-1, -1, -1}}, // side1=west, side2=down, corner=west-down
    };

    // EAST face (+X): vertices at x=1
    // v0=(1,1,0)=北下, v1=(1,1,1)=北上, v2=(1,0,1)=南上, v3=(1,0,0)=南下
    static const AOOffset AO_OFFSETS_EAST[4][3] = {
        // v0: 北下角 (x=1, y=1, z=0)
        {{1, 1, 0}, {1, 0, -1}, {1, 1, -1}}, // side1=north, side2=down, corner=north-down
        // v1: 北上角 (x=1, y=1, z=1)
        {{1, 1, 0}, {1, 0, 1}, {1, 1, 1}}, // side1=north, side2=up, corner=north-up
        // v2: 南上角 (x=1, y=0, z=1)
        {{1, -1, 0}, {1, 0, 1}, {1, -1, 1}}, // side1=south, side2=up, corner=south-up
        // v3: 南下角 (x=1, y=0, z=0)
        {{1, -1, 0}, {1, 0, -1}, {1, -1, -1}}, // side1=south, side2=down, corner=south-down
    };

    // WEST face (-X): vertices at x=0
    // v0=(0,0,0)=南下, v1=(0,0,1)=南上, v2=(0,1,1)=北上, v3=(0,1,0)=北下
    static const AOOffset AO_OFFSETS_WEST[4][3] = {
        // v0: 南下角 (x=0, y=0, z=0)
        {{-1, -1, 0}, {-1, 0, -1}, {-1, -1, -1}}, // side1=south, side2=down, corner=south-down
        // v1: 南上角 (x=0, y=0, z=1)
        {{-1, -1, 0}, {-1, 0, 1}, {-1, -1, 1}}, // side1=south, side2=up, corner=south-up
        // v2: 北上角 (x=0, y=1, z=1)
        {{-1, 1, 0}, {-1, 0, 1}, {-1, 1, 1}}, // side1=north, side2=up, corner=north-up
        // v3: 北下角 (x=0, y=1, z=0)
        {{-1, 1, 0}, {-1, 0, -1}, {-1, 1, -1}}, // side1=north, side2=down, corner=north-down
    };

    /**
     * @brief Get AO offset table for a given face direction
     */
    static const AOOffset (* GetAOOffsets(Direction direction))[3]
    {
        switch (direction)
        {
        case Direction::NORTH: return AO_OFFSETS_NORTH;
        case Direction::SOUTH: return AO_OFFSETS_SOUTH;
        case Direction::EAST: return AO_OFFSETS_EAST;
        case Direction::WEST: return AO_OFFSETS_WEST;
        case Direction::UP: return AO_OFFSETS_UP;
        case Direction::DOWN: return AO_OFFSETS_DOWN;
        default: return AO_OFFSETS_UP;
        }
    }

    /**
     * @brief Get block iterator at offset from current position
     * @param baseIter Base block iterator
     * @param dx X offset
     * @param dy Y offset
     * @param dz Z offset
     * @return BlockIterator at offset position (may be invalid if out of bounds)
     */
    static BlockIterator GetBlockAtOffset(const BlockIterator& baseIter, int dx, int dy, int dz)
    {
        BlockIterator result = baseIter;

        // Apply X offset
        if (dx > 0)
        {
            for (int i = 0; i < dx; ++i)
                result = result.GetEast();
        }
        else if (dx < 0)
        {
            for (int i = 0; i < -dx; ++i)
                result = result.GetWest();
        }

        // Apply Y offset
        if (dy > 0)
        {
            for (int i = 0; i < dy; ++i)
                result = result.GetNorth();
        }
        else if (dy < 0)
        {
            for (int i = 0; i < -dy; ++i)
                result = result.GetSouth();
        }

        // Apply Z offset
        if (dz > 0)
        {
            for (int i = 0; i < dz; ++i)
                result = result.GetUp();
        }
        else if (dz < 0)
        {
            for (int i = 0; i < -dz; ++i)
                result = result.GetDown();
        }

        return result;
    }

    /**
     * @brief Calculate AO values for all 4 vertices of a face
     * @param iterator Block iterator at current block position
     * @param direction Face direction
     * @param outAO Output array of 4 AO values [0.2, 1.0]
     *
     * [IRIS REF] separateAo mode: AO stored in vertex color alpha channel
     */
    static void CalculateFaceAO(const BlockIterator& iterator, Direction direction, float outAO[4])
    {
        const AOOffset (*offsets)[3] = GetAOOffsets(direction);

        for (int v = 0; v < 4; ++v)
        {
            // Get the 3 neighbor blocks for this vertex
            BlockIterator side1Iter  = GetBlockAtOffset(iterator, offsets[v][0].dx, offsets[v][0].dy, offsets[v][0].dz);
            BlockIterator side2Iter  = GetBlockAtOffset(iterator, offsets[v][1].dx, offsets[v][1].dy, offsets[v][1].dz);
            BlockIterator cornerIter = GetBlockAtOffset(iterator, offsets[v][2].dx, offsets[v][2].dy, offsets[v][2].dz);

            bool side1  = IsOccluder(side1Iter);
            bool side2  = IsOccluder(side2Iter);
            bool corner = IsOccluder(cornerIter);

            outAO[v] = CalculateVertexAO(side1, side2, corner);
        }
    }

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

            // [AO] Calculate per-vertex AO values for smooth lighting
            float aoValues[4];
            CalculateFaceAO(iterator, direction, aoValues);

            // [FIX] Directional shading via vertex color (Minecraft-style)
            float   directionalShade = GetDirectionalShade(direction);
            uint8_t shade            = static_cast<uint8_t>(directionalShade * 255.0f);

            // Get face normal and lightmap coordinates for G-Buffer output
            Vec3 faceNormal = GetFaceNormal(direction);
            Vec2 lightmapCoord(lighting.blockLight, lighting.skyLight);

            // [OPTIMIZED] Single-pass: directly build TerrainVertex quad
            std::array<graphic::TerrainVertex, 4> terrainQuad;
            for (int vi = 0; vi < 4; ++vi)
            {
                const Vertex_PCU& srcVertex = renderFace->vertices[vi];

                terrainQuad[vi].m_position      = blockToChunkTransform.TransformPosition3D(srcVertex.m_position);
                terrainQuad[vi].m_uvTexCoords   = srcVertex.m_uvTextCoords;
                terrainQuad[vi].m_normal        = faceNormal;
                terrainQuad[vi].m_lightmapCoord = lightmapCoord;

                // [AO] Store AO based on render type (Iris-compatible)
                // - SOLID/CUTOUT: separateAo mode - AO in alpha channel, RGB = directional shade
                // - TRANSLUCENT: traditional mode - AO premultiplied to RGB, alpha = 255 (for blending)
                if (renderType == RenderType::TRANSLUCENT)
                {
                    // Traditional mode: premultiply AO to RGB for translucent blocks
                    uint8_t shadedValue     = static_cast<uint8_t>(shade * aoValues[vi]);
                    terrainQuad[vi].m_color = Rgba8(shadedValue, shadedValue, shadedValue, 255);
                }
                else
                {
                    // Separate AO mode: store AO in alpha channel for SOLID/CUTOUT
                    uint8_t ao              = static_cast<uint8_t>(aoValues[vi] * 255.0f);
                    terrainQuad[vi].m_color = Rgba8(shade, shade, shade, ao);
                }
            }

            // [FLIPPED QUADS] Determine triangulation based on AO values
            // [SODIUM REF] ModelQuadOrientation.orientByBrightness()
            // File: net/caffeinemc/mods/sodium/client/model/quad/properties/ModelQuadOrientation.java
            bool flipQuad = ShouldFlipQuad(aoValues);

            // [REFACTORED] Route to appropriate mesh based on render type
            switch (renderType)
            {
            case RenderType::SOLID:
                chunkMesh->AddOpaqueTerrainQuad(terrainQuad, flipQuad);
                break;
            case RenderType::CUTOUT:
                chunkMesh->AddCutoutTerrainQuad(terrainQuad, flipQuad);
                break;
            case RenderType::TRANSLUCENT:
                chunkMesh->AddTranslucentTerrainQuad(terrainQuad, flipQuad);

                // ============================================================
                // [WATER BACKFACE] Generate backface for water surface (underwater view)
                // ============================================================
                // [SODIUM REF] DefaultFluidRenderer.java - generates separate backface
                // When water surface UP face is rendered and above is air/non-water,
                // we generate an additional backface with flipped vertex order.
                // This allows seeing the water surface from below (underwater).
                //
                // Conditions for backface generation:
                // 1. Current block is a fluid (water/lava)
                // 2. Current face is UP (top surface)
                // 3. Neighbor above is air or non-same-fluid
                // ============================================================
                if (direction == Direction::UP && !blockState->GetFluidState().IsEmpty())
                {
                    // Check if above is air or different fluid (backface needed)
                    BlockIterator upIter       = iterator.GetNeighbor(Direction::UP);
                    bool          needBackface = true;

                    if (upIter.IsValid())
                    {
                        BlockState* upBlock = upIter.GetBlock();
                        if (upBlock && !upBlock->GetFluidState().IsEmpty())
                        {
                            // Same fluid above - no backface needed (would be culled anyway)
                            if (upBlock->GetFluidState().IsSame(blockState->GetFluidState()))
                            {
                                needBackface = false;
                            }
                        }
                    }

                    if (needBackface)
                    {
                        // Generate backface with flipped vertex order and flipped normal
                        std::array<graphic::TerrainVertex, 4> backfaceQuad = terrainQuad;

                        // Flip normal for correct lighting from below
                        Vec3 flippedNormal = -faceNormal;
                        for (int vi = 0; vi < 4; ++vi)
                        {
                            backfaceQuad[vi].m_normal = flippedNormal;
                        }

                        // AddTranslucentTerrainQuadBackface will flip vertex order internally
                        chunkMesh->AddTranslucentTerrainQuadBackface(backfaceQuad, flipQuad);
                    }
                }
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
