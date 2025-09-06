#include "ChunkMeshBuilder.hpp"
#include "Chunk.hpp"
#include "../../Registry/Block/Block.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Renderer/Model/BlockRenderMesh.hpp"
#include "Engine/Math/Mat44.hpp"
#include "../../Voxel/Property/PropertyTypes.hpp"

using namespace enigma::voxel::chunk;

std::unique_ptr<ChunkMesh> ChunkMeshBuilder::BuildMesh(Chunk* chunk)
{
    if (!chunk)
    {
        core::LogWarn("ChunkMeshBuilder", "Attempted to build mesh for null chunk");
        return nullptr;
    }

    auto chunkMesh  = std::make_unique<ChunkMesh>();
    int  blockCount = 0;

    core::LogInfo("ChunkMeshBuilder", "Building mesh for chunk...");

    // Iterate through all blocks in the chunk
    for (int x = 0; x < Chunk::CHUNK_SIZE; ++x)
    {
        for (int y = 0; y < Chunk::CHUNK_SIZE; ++y)
        {
            for (int z = 0; z < Chunk::CHUNK_HEIGHT; ++z)
            {
                BlockState blockState = chunk->GetBlock(x, y, z);

                if (ShouldRenderBlock(&blockState))
                {
                    Vec3 blockPos = GetBlockPosition(x, y, z);
                    AddBlockToMesh(chunkMesh.get(), &blockState, blockPos);
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

void ChunkMeshBuilder::AddBlockToMesh(ChunkMesh* chunkMesh, BlockState* blockState, const Vec3& blockPos)
{
    if (!chunkMesh || !blockState)
    {
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
    const std::vector<enigma::voxel::property::Direction> allDirections = {
        enigma::voxel::property::Direction::NORTH,
        enigma::voxel::property::Direction::SOUTH,
        enigma::voxel::property::Direction::EAST,
        enigma::voxel::property::Direction::WEST,
        enigma::voxel::property::Direction::UP,
        enigma::voxel::property::Direction::DOWN
    };

    // Create transformation matrix from block space to chunk space
    Mat44 blockToChunkTransform = Mat44::MakeTranslation3D(blockPos);

    // Iterate through all faces of the block render mesh
    for (const auto& direction : allDirections)
    {
        const auto* renderFace = blockRenderMesh->GetFace(direction);
        if (!renderFace || renderFace->vertices.empty())
        {
            continue; // Skip faces without geometry
        }

        // Transform vertices from block space to chunk space
        std::vector<Vertex_PCU> transformedVertices;
        transformedVertices.reserve(renderFace->vertices.size());

        for (const auto& vertex : renderFace->vertices)
        {
            Vertex_PCU transformedVertex = vertex;
            // Transform position from block space (0,0,0)-(1,1,1) to chunk space
            transformedVertex.m_position = blockToChunkTransform.TransformPosition3D(vertex.m_position);
            transformedVertices.push_back(transformedVertex);
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
            core::LogWarn("ChunkMeshBuilder", "Face has %zu vertices, expected 4 for quad conversion",
                          transformedVertices.size());
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

    // Get block registry name to check for air
    std::string blockName = blockState->GetBlock()->GetRegistryName();

    // Don't render air blocks
    if (blockName == "air" || blockName == "minecraft:air" || blockName.empty())
    {
        return false;
    }

    return true;
}

Vec3 ChunkMeshBuilder::GetBlockPosition(int x, int y, int z) const
{
    // Convert chunk-local coordinates (0-15) to world position
    // Each block is 1 unit in size
    return Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
}
