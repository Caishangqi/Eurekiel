#include "ChunkMeshBuilder.hpp"

#include "Chunk.hpp"
#include "MeshBuild/ChunkMeshBuildInputFactory.hpp"
#include "MeshBuild/ChunkMeshingSnapshot.hpp"
#include "../../Registry/Block/Block.hpp"
#include "../Fluid/FluidState.hpp"
#include "../World/TerrainVertexLayout.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"
#include "Engine/Registry/Block/RenderShape.hpp"
#include "Engine/Renderer/Model/BlockRenderMesh.hpp"

#include <algorithm>
#include <array>
#include <string>

using namespace enigma::voxel;
using namespace enigma::registry::block;

namespace
{
    static const std::array<Direction, 6> kAllDirections = {
        Direction::NORTH,
        Direction::SOUTH,
        Direction::EAST,
        Direction::WEST,
        Direction::UP,
        Direction::DOWN
    };

    static const Vec3 FACE_NORMALS[6] = {
        Vec3(0.0f, 1.0f, 0.0f),
        Vec3(0.0f, -1.0f, 0.0f),
        Vec3(1.0f, 0.0f, 0.0f),
        Vec3(-1.0f, 0.0f, 0.0f),
        Vec3(0.0f, 0.0f, 1.0f),
        Vec3(0.0f, 0.0f, -1.0f),
    };

    struct LightingData
    {
        float skyLight   = 1.0f / 15.0f;
        float blockLight = 0.0f;
    };

    struct AOOffset
    {
        int dx;
        int dy;
        int dz;
    };

    static const float AO_CURVE[4] = { 1.0f, 0.7f, 0.5f, 0.2f };

    static const AOOffset AO_OFFSETS_UP[4][3] = {
        {{-1, 0, 1}, {0, -1, 1}, {-1, -1, 1}},
        {{1, 0, 1}, {0, -1, 1}, {1, -1, 1}},
        {{1, 0, 1}, {0, 1, 1}, {1, 1, 1}},
        {{-1, 0, 1}, {0, 1, 1}, {-1, 1, 1}},
    };

    static const AOOffset AO_OFFSETS_DOWN[4][3] = {
        {{-1, 0, -1}, {0, -1, -1}, {-1, -1, -1}},
        {{-1, 0, -1}, {0, 1, -1}, {-1, 1, -1}},
        {{1, 0, -1}, {0, 1, -1}, {1, 1, -1}},
        {{1, 0, -1}, {0, -1, -1}, {1, -1, -1}},
    };

    static const AOOffset AO_OFFSETS_NORTH[4][3] = {
        {{-1, 1, 0}, {0, 1, -1}, {-1, 1, -1}},
        {{-1, 1, 0}, {0, 1, 1}, {-1, 1, 1}},
        {{1, 1, 0}, {0, 1, 1}, {1, 1, 1}},
        {{1, 1, 0}, {0, 1, -1}, {1, 1, -1}},
    };

    static const AOOffset AO_OFFSETS_SOUTH[4][3] = {
        {{1, -1, 0}, {0, -1, -1}, {1, -1, -1}},
        {{1, -1, 0}, {0, -1, 1}, {1, -1, 1}},
        {{-1, -1, 0}, {0, -1, 1}, {-1, -1, 1}},
        {{-1, -1, 0}, {0, -1, -1}, {-1, -1, -1}},
    };

    static const AOOffset AO_OFFSETS_EAST[4][3] = {
        {{1, 1, 0}, {1, 0, -1}, {1, 1, -1}},
        {{1, 1, 0}, {1, 0, 1}, {1, 1, 1}},
        {{1, -1, 0}, {1, 0, 1}, {1, -1, 1}},
        {{1, -1, 0}, {1, 0, -1}, {1, -1, -1}},
    };

    static const AOOffset AO_OFFSETS_WEST[4][3] = {
        {{-1, -1, 0}, {-1, 0, -1}, {-1, -1, -1}},
        {{-1, -1, 0}, {-1, 0, 1}, {-1, -1, 1}},
        {{-1, 1, 0}, {-1, 0, 1}, {-1, 1, 1}},
        {{-1, 1, 0}, {-1, 0, -1}, {-1, 1, -1}},
    };

    Vec3 GetFaceNormal(Direction direction)
    {
        return FACE_NORMALS[static_cast<int>(direction)];
    }

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
        default: return 1.0f;
        }
    }

    bool IsOccluder(BlockState* blockState)
    {
        return blockState != nullptr &&
               blockState->GetBlock() != nullptr &&
               blockState->CanOcclude();
    }

    float CalculateVertexAO(bool side1, bool side2, bool corner)
    {
        int occluderCount = 0;
        if (side1 && side2)
        {
            occluderCount = 3;
        }
        else
        {
            occluderCount = (side1 ? 1 : 0) + (side2 ? 1 : 0) + (corner ? 1 : 0);
        }

        return AO_CURVE[occluderCount];
    }

    bool ShouldFlipQuad(const float aoValues[4])
    {
        const float brightness02 = aoValues[0] + aoValues[2];
        const float brightness13 = aoValues[1] + aoValues[3];
        return brightness13 > brightness02;
    }

    const AOOffset (*GetAOOffsets(Direction direction))[3]
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

    BlockState* GetBlockAtOffset(const ChunkMeshingSnapshot& snapshot,
                                 int32_t x,
                                 int32_t y,
                                 int32_t z,
                                 int dx,
                                 int dy,
                                 int dz)
    {
        return snapshot.GetBlock(x + dx, y + dy, z + dz);
    }

    void CalculateFaceAO(const ChunkMeshingSnapshot& snapshot,
                         int32_t x,
                         int32_t y,
                         int32_t z,
                         Direction direction,
                         float outAO[4])
    {
        const AOOffset (*offsets)[3] = GetAOOffsets(direction);

        for (int vertexIndex = 0; vertexIndex < 4; ++vertexIndex)
        {
            BlockState* side1Block =
                GetBlockAtOffset(snapshot, x, y, z, offsets[vertexIndex][0].dx, offsets[vertexIndex][0].dy, offsets[vertexIndex][0].dz);
            BlockState* side2Block =
                GetBlockAtOffset(snapshot, x, y, z, offsets[vertexIndex][1].dx, offsets[vertexIndex][1].dy, offsets[vertexIndex][1].dz);
            BlockState* cornerBlock =
                GetBlockAtOffset(snapshot, x, y, z, offsets[vertexIndex][2].dx, offsets[vertexIndex][2].dy, offsets[vertexIndex][2].dz);

            outAO[vertexIndex] = CalculateVertexAO(IsOccluder(side1Block),
                                                   IsOccluder(side2Block),
                                                   IsOccluder(cornerBlock));
        }
    }

    LightingData GetNeighborLighting(const ChunkMeshingSnapshot& snapshot,
                                     int32_t x,
                                     int32_t y,
                                     int32_t z,
                                     Direction direction)
    {
        int dx = 0;
        int dy = 0;
        int dz = 0;

        switch (direction)
        {
        case Direction::NORTH: dy = 1; break;
        case Direction::SOUTH: dy = -1; break;
        case Direction::EAST: dx = 1; break;
        case Direction::WEST: dx = -1; break;
        case Direction::UP: dz = 1; break;
        case Direction::DOWN: dz = -1; break;
        }

        const ChunkMeshingLightSample sample = snapshot.GetLight(x + dx, y + dy, z + dz);

        LightingData result;
        result.skyLight   = static_cast<float>(sample.skyLight) / 15.0f;
        result.blockLight = static_cast<float>(sample.blockLight) / 15.0f;

        const float totalLight = (std::max)(result.skyLight, result.blockLight);
        if (totalLight < 1.0f / 15.0f)
        {
            result.skyLight = 1.0f / 15.0f;
        }

        return result;
    }
}

ChunkMeshBuilder::ChunkMeshBuilder()
{
    m_air = registry::block::BlockRegistry::GetBlock("simpleminer", "air");
}

ChunkMeshBuildResult ChunkMeshBuilder::Build(const ChunkMeshBuildInput& input) const
{
    ChunkMeshBuildResult result;
    result.chunkCoords     = input.GetChunkCoords();
    result.chunkInstanceId = input.GetChunkInstanceId();
    result.buildVersion    = input.GetBuildVersion();
    result.reloadGeneration = input.GetReloadGeneration();

    if (!input.HasSnapshot())
    {
        result.status = ChunkMeshBuildResultStatus::Failed;
        result.detail = "MissingSnapshot";
        core::LogWarn("ChunkMeshBuilder", "Build called without snapshot for chunk (%d, %d)",
                      input.GetChunkCoords().x, input.GetChunkCoords().y);
        return result;
    }

    const ChunkMeshingSnapshot& snapshot = *input.snapshot;

    auto   chunkMesh = std::make_unique<ChunkMesh>();
    int    blockCount = 0;
    size_t opaqueQuadCount = 0;
    size_t cutoutQuadCount = 0;
    size_t translucentQuadCount = 0;

    for (int32_t x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
    {
        for (int32_t y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
        {
            for (int32_t z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
            {
                BlockState* blockState = snapshot.GetCenterBlock(x, y, z);
                if (!ShouldRenderBlock(blockState))
                {
                    continue;
                }

                const RenderType renderType = GetBlockRenderType(blockState);
                for (Direction direction : kAllDirections)
                {
                    if (!ShouldRenderFace(snapshot, blockState, x, y, z, direction))
                    {
                        continue;
                    }

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

    chunkMesh->Reserve(opaqueQuadCount, cutoutQuadCount, translucentQuadCount);

    for (int32_t x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
    {
        for (int32_t y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
        {
            for (int32_t z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
            {
                BlockState* blockState = snapshot.GetCenterBlock(x, y, z);
                if (!ShouldRenderBlock(blockState))
                {
                    continue;
                }

                AddBlockToMesh(*chunkMesh, blockState, GetBlockPosition(x, y, z), snapshot, x, y, z);
                blockCount++;
            }
        }
    }

    result.metrics.opaqueVertexCount      = chunkMesh->GetOpaqueVertexCount();
    result.metrics.cutoutVertexCount      = chunkMesh->GetCutoutVertexCount();
    result.metrics.translucentVertexCount = chunkMesh->GetTranslucentVertexCount();
    result.metrics.opaqueIndexCount       = chunkMesh->GetOpaqueIndexCount();
    result.metrics.cutoutIndexCount       = chunkMesh->GetCutoutIndexCount();
    result.metrics.translucentIndexCount  = chunkMesh->GetTranslucentIndexCount();
    result.mesh                           = std::move(chunkMesh);
    result.status                         = ChunkMeshBuildResultStatus::Built;
    result.detail                         = "Built";

    core::LogDebug("ChunkMeshBuilder",
                   "Built snapshot mesh for chunk (%d, %d), blocks=%d, opaque=%llu, cutout=%llu, translucent=%llu",
                   input.GetChunkCoords().x,
                   input.GetChunkCoords().y,
                   blockCount,
                   result.metrics.opaqueVertexCount,
                   result.metrics.cutoutVertexCount,
                   result.metrics.translucentVertexCount);
    return result;
}

std::unique_ptr<ChunkMesh> ChunkMeshBuilder::BuildMesh(Chunk* chunk) const
{
    if (chunk == nullptr)
    {
        core::LogWarn("ChunkMeshBuilder", "Attempted to build mesh for null chunk");
        return nullptr;
    }

    ChunkMeshBuildInput input;
    if (!ChunkMeshBuildInputFactory::TryCreate(*chunk, 0, false, input))
    {
        core::LogDebug("ChunkMeshBuilder", "Snapshot creation deferred for chunk (%d, %d)",
                       chunk->GetChunkX(), chunk->GetChunkY());
        return nullptr;
    }

    ChunkMeshBuildResult result = Build(input);
    if (result.status != ChunkMeshBuildResultStatus::Built)
    {
        if (result.status == ChunkMeshBuildResultStatus::Failed)
        {
            core::LogWarn("ChunkMeshBuilder", "Failed to build snapshot mesh for chunk (%d, %d): %s",
                          chunk->GetChunkX(), chunk->GetChunkY(), result.detail.c_str());
        }
        return nullptr;
    }

    return std::move(result.mesh);
}

void ChunkMeshBuilder::AddBlockToMesh(ChunkMesh& chunkMesh,
                                      BlockState* blockState,
                                      const BlockPos& blockPos,
                                      const ChunkMeshingSnapshot& snapshot,
                                      int32_t x,
                                      int32_t y,
                                      int32_t z) const
{
    if (blockState == nullptr)
    {
        return;
    }

    auto blockRenderMesh = blockState->GetRenderMesh();
    if (!blockRenderMesh || blockRenderMesh->IsEmpty())
    {
        return;
    }

    const RenderType renderType = GetBlockRenderType(blockState);
    const Vec3       blockPosVec3(static_cast<float>(blockPos.x), static_cast<float>(blockPos.y), static_cast<float>(blockPos.z));
    const Mat44      blockToChunkTransform = Mat44::MakeTranslation3D(blockPosVec3);

    for (Direction direction : kAllDirections)
    {
        if (!ShouldRenderFace(snapshot, blockState, x, y, z, direction))
        {
            continue;
        }

        const auto renderFaces = blockRenderMesh->GetFaces(direction);
        if (renderFaces.empty())
        {
            continue;
        }

        const LightingData lighting = GetNeighborLighting(snapshot, x, y, z, direction);
        float              aoValues[4] = {};
        CalculateFaceAO(snapshot, x, y, z, direction, aoValues);
        const bool         flipQuad = ShouldFlipQuad(aoValues);
        const float        directionalShade = GetDirectionalShade(direction);
        const uint8_t      shade = static_cast<uint8_t>(directionalShade * 255.0f);
        const Vec3         faceNormal = GetFaceNormal(direction);
        const Vec2         lightmapCoord(lighting.blockLight, lighting.skyLight);

        for (const auto* renderFace : renderFaces)
        {
            if (renderFace == nullptr || renderFace->vertices.size() < 4)
            {
                continue;
            }

            std::array<graphic::TerrainVertex, 4> terrainQuad;
            for (int vertexIndex = 0; vertexIndex < 4; ++vertexIndex)
            {
                const Vertex_PCU& srcVertex = renderFace->vertices[vertexIndex];
                terrainQuad[vertexIndex].m_position      = blockToChunkTransform.TransformPosition3D(srcVertex.m_position);
                terrainQuad[vertexIndex].m_uvTexCoords   = srcVertex.m_uvTextCoords;
                terrainQuad[vertexIndex].m_normal        = faceNormal;
                terrainQuad[vertexIndex].m_lightmapCoord = lightmapCoord;

                if (renderType == RenderType::TRANSLUCENT)
                {
                    const uint8_t shadedValue = static_cast<uint8_t>(shade * aoValues[vertexIndex]);
                    terrainQuad[vertexIndex].m_color = Rgba8(shadedValue, shadedValue, shadedValue, 255);
                }
                else
                {
                    const uint8_t ao = static_cast<uint8_t>(aoValues[vertexIndex] * 255.0f);
                    terrainQuad[vertexIndex].m_color = Rgba8(shade, shade, shade, ao);
                }
            }

            if (graphic::TerrainVertexLayout::OnBuildVertexLayout.HasListeners())
            {
                const std::string namespacedBlockName =
                    blockState->GetBlock()->GetNamespace() + ":" + blockState->GetBlock()->GetRegistryName();
                graphic::TerrainVertexLayout::OnBuildVertexLayout.Broadcast(terrainQuad.data(), namespacedBlockName);
            }

            switch (renderType)
            {
            case RenderType::SOLID:
                chunkMesh.AddOpaqueTerrainQuad(terrainQuad, flipQuad);
                break;
            case RenderType::CUTOUT:
                chunkMesh.AddCutoutTerrainQuad(terrainQuad, flipQuad);
                break;
            case RenderType::TRANSLUCENT:
                chunkMesh.AddTranslucentTerrainQuad(terrainQuad, flipQuad);

                if (direction == Direction::UP && !blockState->GetFluidState().IsEmpty())
                {
                    BlockState* upBlock = snapshot.GetBlock(x, y, z + 1);
                    bool        needBackface = true;
                    if (upBlock != nullptr && !upBlock->GetFluidState().IsEmpty() &&
                        upBlock->GetFluidState().IsSame(blockState->GetFluidState()))
                    {
                        needBackface = false;
                    }

                    if (needBackface)
                    {
                        std::array<graphic::TerrainVertex, 4> backfaceQuad = terrainQuad;
                        const Vec3 flippedNormal = -faceNormal;
                        for (graphic::TerrainVertex& vertex : backfaceQuad)
                        {
                            vertex.m_normal = flippedNormal;
                        }

                        chunkMesh.AddTranslucentTerrainQuadBackface(backfaceQuad, flipQuad);
                    }
                }
                break;
            }
        }
    }
}

bool ChunkMeshBuilder::ShouldRenderBlock(BlockState* blockState) const
{
    if (blockState == nullptr || blockState->GetBlock() == nullptr)
    {
        return false;
    }

    if (blockState->GetBlock() == m_air.get())
    {
        return false;
    }

    const RenderShape renderShape = blockState->GetBlock()->GetRenderShape(blockState);
    return renderShape != RenderShape::INVISIBLE;
}

bool ChunkMeshBuilder::ShouldRenderFace(const ChunkMeshingSnapshot& snapshot,
                                        BlockState* currentBlock,
                                        int32_t x,
                                        int32_t y,
                                        int32_t z,
                                        Direction direction) const
{
    if (currentBlock == nullptr || currentBlock->GetBlock() == nullptr)
    {
        return false;
    }

    int dx = 0;
    int dy = 0;
    int dz = 0;

    switch (direction)
    {
    case Direction::NORTH: dy = 1; break;
    case Direction::SOUTH: dy = -1; break;
    case Direction::EAST: dx = 1; break;
    case Direction::WEST: dx = -1; break;
    case Direction::UP: dz = 1; break;
    case Direction::DOWN: dz = -1; break;
    }

    BlockState* neighborBlock = snapshot.GetBlock(x + dx, y + dy, z + dz);
    if (neighborBlock == nullptr || neighborBlock->GetBlock() == nullptr)
    {
        return true;
    }

    if (currentBlock->GetBlock()->SkipRendering(currentBlock, neighborBlock, direction))
    {
        return false;
    }

    const RenderType currentRenderType = GetBlockRenderType(currentBlock);
    if (neighborBlock->CanOcclude())
    {
        if (currentRenderType == RenderType::SOLID)
        {
            return false;
        }

        if (currentRenderType == RenderType::TRANSLUCENT && !currentBlock->GetFluidState().IsEmpty())
        {
            return false;
        }

        return true;
    }

    return true;
}

RenderType ChunkMeshBuilder::GetBlockRenderType(BlockState* blockState) const
{
    if (blockState == nullptr || blockState->GetBlock() == nullptr)
    {
        return RenderType::SOLID;
    }

    return blockState->GetBlock()->GetRenderType();
}

BlockPos ChunkMeshBuilder::GetBlockPosition(int x, int y, int z) const
{
    return BlockPos(x, y, z);
}
