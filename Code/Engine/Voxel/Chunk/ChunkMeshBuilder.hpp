#pragma once
#include "Chunk.hpp"
#include "ChunkMesh.hpp"
#include "../Block/BlockState.hpp"
#include "../../Voxel/Property/PropertyTypes.hpp"
#include <memory>

namespace enigma::voxel::chunk
{
    using namespace enigma::voxel::block;
    using namespace enigma::voxel::property;

    /**
     * @brief Builds optimized ChunkMesh from Chunk block data
     * 
     * Assignment 1 simplified implementation:
     * - No face culling (renders all faces of all blocks)
     * - No greedy meshing optimization
     * - Direct block mesh copying to chunk mesh
     */
    class ChunkMeshBuilder
    {
    public:
        ChunkMeshBuilder();
        ~ChunkMeshBuilder() = default;

        /**
         * @brief Build a mesh from chunk data
         * 
         * Assignment 1 simplified algorithm:
         * 1. Iterate through all blocks in chunk
         * 2. Skip air blocks
         * 3. Get BlockState's compiled BlockRenderMesh
         * 4. Transform and append mesh vertices to chunk mesh
         * 
         * @param chunk Source chunk data
         * @return Compiled chunk mesh ready for rendering
         */
        std::unique_ptr<ChunkMesh> BuildMesh(Chunk* chunk);

        /**
         * @brief Rebuild an existing chunk's mesh
         * 
         * @param chunk Chunk to rebuild mesh for
         */
        void RebuildMesh(Chunk* chunk);

    private:
        /**
         * @brief Add a single block's mesh to the chunk mesh
         *
         * @param chunkMesh Target chunk mesh
         * @param blockState Block state to render
         * @param blockPos Position of the block within the chunk (0-15 range)
         * @param chunk Source chunk for face culling
         */
        void AddBlockToMesh(ChunkMesh* chunkMesh, BlockState* blockState, const BlockPos& blockPos, Chunk* chunk);

        /**
         * @brief Check if a block should be rendered
         *
         * Assignment 1: Simply check if block is not air
         *
         * @param blockState Block state to check
         * @return True if block should be rendered
         */
        bool ShouldRenderBlock(BlockState* blockState) const;

        /**
         * @brief Check if a block face should be rendered (face culling)
         *
         * Assignment 2: Face culling implementation
         *
         * @param chunk Source chunk
         * @param blockPos Position of the block within chunk (0-15 range)
         * @param direction Direction of the face to check
         * @return True if face should be rendered
         */
        bool ShouldRenderFace(Chunk* chunk, const BlockPos& blockPos, Direction direction) const;

        /**
         * @brief Get neighbor block position in given direction
         *
         * @param blockPos Current block position
         * @param direction Direction to get neighbor
         * @return Neighbor position
         */
        BlockPos GetNeighborPosition(const BlockPos& blockPos, Direction direction) const;

        /**
         * @brief Get block position in chunk coordinates
         *
         * @param x, y, z Block coordinates within chunk (0-15)
         * @return BlockPos position for mesh transformation
         */
        BlockPos GetBlockPosition(int x, int y, int z) const;

    private:
        std::shared_ptr<registry::block::Block> air = nullptr;
    };
}
