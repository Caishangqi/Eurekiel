#include "ChunkMeshBuilder.hpp"
#include "../../Registry/Block/Block.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Renderer/Model/BlockRenderMesh.hpp"

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
    for (int x = 0; x < 16; ++x)
    {
        for (int y = 0; y < 16; ++y)
        {
            for (int z = 0; z < 16; ++z)
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

    // TODO: Set the new mesh on the chunk
    // This requires the Chunk class to have a SetMesh method
    // For now, we'll just log that we rebuilt it
    core::LogInfo("ChunkMeshBuilder", "Rebuilt mesh for chunk");
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
        core::LogWarn("ChunkMeshBuilder", "Block state has no render mesh: %s",
                      blockState->ToString().c_str());
        return;
    }

    UNUSED(blockPos)
    // Transform and append the block mesh to the chunk mesh
    // blockRenderMesh->TransformAndAppendTo(chunkMesh, blockPos);
    // TODO: Handle the logic into Chunk Mesh Builder ?
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
