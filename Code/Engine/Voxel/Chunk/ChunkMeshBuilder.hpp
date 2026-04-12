#pragma once

#include "ChunkMesh.hpp"
#include "MeshBuild/ChunkMeshBuildInput.hpp"
#include "MeshBuild/ChunkMeshBuildResult.hpp"
#include "../Block/BlockState.hpp"
#include "../../Voxel/Property/PropertyTypes.hpp"
#include "Engine/Registry/Block/RenderType.hpp"

#include <memory>

namespace enigma::voxel
{
    class Chunk;
    struct ChunkMeshingSnapshot;

    class ChunkMeshBuilder
    {
    public:
        ChunkMeshBuilder();
        ~ChunkMeshBuilder() = default;

        ChunkMeshBuildResult       Build(const ChunkMeshBuildInput& input) const;
        std::unique_ptr<ChunkMesh> BuildMesh(Chunk* chunk) const;

    private:
        void AddBlockToMesh(ChunkMesh& chunkMesh,
                            BlockState* blockState,
                            const BlockPos& blockPos,
                            const ChunkMeshingSnapshot& snapshot,
                            int32_t x,
                            int32_t y,
                            int32_t z) const;
        bool ShouldRenderBlock(BlockState* blockState) const;
        bool ShouldRenderFace(const ChunkMeshingSnapshot& snapshot,
                              BlockState* currentBlock,
                              int32_t x,
                              int32_t y,
                              int32_t z,
                              Direction direction) const;
        registry::block::RenderType GetBlockRenderType(BlockState* blockState) const;
        BlockPos GetBlockPosition(int x, int y, int z) const;

    private:
        std::shared_ptr<registry::block::Block> m_air = nullptr;
    };
}
