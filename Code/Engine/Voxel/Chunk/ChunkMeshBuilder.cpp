#include "ChunkMeshBuilder.hpp"
#include "Chunk.hpp"
#include "../../Registry/Block/Block.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Renderer/Model/BlockRenderMesh.hpp"
#include "Engine/Math/Mat44.hpp"
#include "../../Voxel/Property/PropertyTypes.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"

using namespace enigma::voxel;

ChunkMeshBuilder::ChunkMeshBuilder()
{
    air = registry::block::BlockRegistry::GetBlock("simpleminer", "air");
}

std::unique_ptr<ChunkMesh> ChunkMeshBuilder::BuildMesh(Chunk* chunk)
{
    // ===== Phase 4: 入口状态检查（防护点1） =====
    if (!chunk)
    {
        core::LogWarn("ChunkMeshBuilder", "Attempted to build mesh for null chunk");
        return nullptr;
    }

    ChunkState state = chunk->GetState();
    if (state != ChunkState::Active && state != ChunkState::BuildingMesh)
    {
        core::LogDebug("ChunkMeshBuilder", "BuildMesh: chunk not in valid state (state=%s), aborting", chunk->GetStateName());
        return nullptr;
    }

    auto chunkMesh  = std::make_unique<ChunkMesh>();
    int  blockCount = 0;

    core::LogInfo("ChunkMeshBuilder", "Building mesh for chunk...");

    // Assignment 2: Two-pass optimization
    // First pass: Count visible faces to pre-allocate memory
    size_t opaqueQuadCount      = 0;
    size_t transparentQuadCount = 0;

    // First pass: Count quads needed
    for (int x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
    {
        for (int y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
        {
            for (int z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
            {
                BlockState* blockState = chunk->GetBlock(x, y, z);

                if (ShouldRenderBlock(blockState))
                {
                    BlockPos blockPos = GetBlockPosition(x, y, z);

                    // Count visible faces for this block
                    static const std::vector<enigma::voxel::Direction> allDirections = {
                        enigma::voxel::Direction::NORTH,
                        enigma::voxel::Direction::SOUTH,
                        enigma::voxel::Direction::EAST,
                        enigma::voxel::Direction::WEST,
                        enigma::voxel::Direction::UP,
                        enigma::voxel::Direction::DOWN
                    };

                    for (const auto& direction : allDirections)
                    {
                        if (ShouldRenderFace(chunk, blockPos, direction))
                        {
                            // For now, treat all as opaque (can be refined later for transparent blocks)
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
                BlockState* blockState = chunk->GetBlock(x, y, z);

                if (ShouldRenderBlock(blockState))
                {
                    BlockPos blockPos = GetBlockPosition(x, y, z);
                    AddBlockToMesh(chunkMesh.get(), blockState, blockPos, chunk);
                    blockCount++;
                }
            }
        }
    }

    core::LogInfo("ChunkMeshBuilder", "Chunk mesh built successfully. Blocks: %d, Vertices: %zu, Triangles: %zu",
                  blockCount, chunkMesh->GetOpaqueVertexCount(), chunkMesh->GetOpaqueTriangleCount());

    return chunkMesh;
}

void ChunkMeshBuilder::RebuildMesh(Chunk* chunk)
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
        core::LogInfo("ChunkMeshBuilder", "Successfully rebuilt and set mesh for chunk");
    }
    else
    {
        core::LogError("ChunkMeshBuilder", "Failed to rebuild mesh for chunk");
    }
}

void ChunkMeshBuilder::AddBlockToMesh(ChunkMesh* chunkMesh, BlockState* blockState, const BlockPos& blockPos, Chunk* chunk)
{
    // ===== Phase 4: 关键位置状态验证（防护点2） =====
    if (!chunkMesh || !blockState)
    {
        return;
    }

    if (!chunk || chunk->GetState() != ChunkState::Active)
    {
        core::LogDebug("ChunkMeshBuilder", "AddBlockToMesh: chunk invalid or not Active, aborting");
        return;
    }

    // Get the compiled render mesh for this block state
    auto blockRenderMesh = blockState->GetRenderMesh();

    if (!blockRenderMesh || blockRenderMesh->IsEmpty())
    {
        // No render mesh available - this is normal for air blocks
        return;
    }

    // All 6 directions for face iteration
    static const std::vector<enigma::voxel::Direction> allDirections = {
        enigma::voxel::Direction::NORTH,
        enigma::voxel::Direction::SOUTH,
        enigma::voxel::Direction::EAST,
        enigma::voxel::Direction::WEST,
        enigma::voxel::Direction::UP,
        enigma::voxel::Direction::DOWN
    };

    // Create transformation matrix from block space to chunk space
    Vec3  blockPosVec3(static_cast<float>(blockPos.x), static_cast<float>(blockPos.y), static_cast<float>(blockPos.z));
    Mat44 blockToChunkTransform = Mat44::MakeTranslation3D(blockPosVec3);

    // Iterate through all faces of the block render mesh
    for (const auto& direction : allDirections)
    {
        // ===== Phase 4: 面级别状态检查（防护点2.1） =====
        if (chunk->GetState() != ChunkState::Active)
        {
            core::LogDebug("ChunkMeshBuilder", "AddBlockToMesh: chunk state changed during face iteration, aborting");
            return;
        }

        // Assignment 2: Face culling - skip faces that are not visible
        if (!ShouldRenderFace(chunk, blockPos, direction))
        {
            continue; // Skip this face as it's occluded
        }

        const auto* renderFace = blockRenderMesh->GetFace(direction);
        if (!renderFace || renderFace->vertices.empty())
        {
            continue; // Skip faces without geometry
        }

        // Transform vertices from block space to chunk space
        std::vector<Vertex_PCU> transformedVertices((int)renderFace->vertices.size(), Vertex_PCU());

        for (int i = 0; i < (int)renderFace->vertices.size(); ++i)
        {
            Vertex_PCU transformedVertex = renderFace->vertices[i];
            // Transform position from block space (0,0,0)-(1,1,1) to chunk space
            transformedVertex.m_position = blockToChunkTransform.TransformPosition3D(renderFace->vertices[i].m_position);
            transformedVertices[i]       = transformedVertex;
        }
        /*for (const auto& vertex : renderFace->vertices)
        {
            Vertex_PCU transformedVertex = vertex;
            // Transform position from block space (0,0,0)-(1,1,1) to chunk space
            transformedVertex.m_position = blockToChunkTransform.TransformPosition3D(vertex.m_position);
            transformedVertices.push_back(transformedVertex);
        }*/

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
            core::LogWarn("ChunkMeshBuilder", "Face has %zu vertices, expected 4 for quad conversion", transformedVertices.size());
        }
    }
}

bool ChunkMeshBuilder::ShouldRenderBlock(BlockState* blockState) const
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

    /*if (!blockState || blockState->GetBlock() == airBlock.get())
    {
        return false;
    }*/

    // Get block registry name to check for air
    //std::string blockName = blockState->GetBlock()->GetRegistryName();

    // Don't render air blocks
    if (blockState->GetBlock() == air.get())
    {
        return false;
    }

    return true;
}

bool ChunkMeshBuilder::ShouldRenderFace(Chunk* chunk, const BlockPos& blockPos, Direction direction) const
{
    // ===== Phase 4: 当前chunk状态检查（防护点3.1） =====
    if (!chunk || chunk->GetState() != ChunkState::Active)
    {
        core::LogDebug("ChunkMeshBuilder", "ShouldRenderFace: chunk invalid or not Active");
        return false;
    }

    // Assignment 2: Face culling implementation
    // A face should be rendered if the adjacent block in that direction is:
    // 1. Air (empty)
    // 2. Transparent
    // 3. Outside chunk bounds (edge faces are always visible)

    BlockPos neighborPos = GetNeighborPosition(blockPos, direction);

    // Check if neighbor is outside chunk bounds - render edge faces
    if (neighborPos.x < 0 || neighborPos.x >= Chunk::CHUNK_SIZE_X ||
        neighborPos.y < 0 || neighborPos.y >= Chunk::CHUNK_SIZE_Y ||
        neighborPos.z < 0 || neighborPos.z >= Chunk::CHUNK_SIZE_Z)
    {
        return true; // Edge face, always render
    }

    // ===== Phase 4: 再次验证chunk状态（防护点3.2） =====
    // 防止在GetNeighborPosition和GetBlock之间chunk被删除
    if (chunk->GetState() != ChunkState::Active)
    {
        core::LogDebug("ChunkMeshBuilder", "ShouldRenderFace: chunk state changed, conservative render");
        return true; // 保守渲染策略：状态无效时渲染面，避免视觉缺失
    }

    // If we have chunk reference, check the neighbor block
    if (chunk)
    {
        BlockState* neighborBlock = chunk->GetBlock(neighborPos.x, neighborPos.y, neighborPos.z);

        // Render face if neighbor is air
        if (!ShouldRenderBlock(neighborBlock))
        {
            return true; // Neighbor is air, render this face
        }

        // TODO: Add transparent block check here
        // For now, assume all non-air blocks are opaque
        return false; // Neighbor is solid, cull this face
    }

    // No chunk reference - render by default (fallback)
    return true;
}

BlockPos ChunkMeshBuilder::GetNeighborPosition(const BlockPos& blockPos, Direction direction) const
{
    return blockPos.GetRelative(direction);
}

BlockPos ChunkMeshBuilder::GetBlockPosition(int x, int y, int z) const
{
    return BlockPos(x, y, z);
}
