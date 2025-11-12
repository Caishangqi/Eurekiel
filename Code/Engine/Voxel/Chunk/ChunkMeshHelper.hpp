#pragma once
#include "Chunk.hpp"
#include "ChunkMesh.hpp"
#include "../Block/BlockState.hpp"
#include "../../Voxel/Property/PropertyTypes.hpp"
#include <memory>

namespace enigma::voxel
{
    /**
     * @brief Pure static utility class for building optimized ChunkMesh from Chunk block data
     * 
     * Design Pattern: Helper Class (shrimp-rules.md 10.3)
     * - All methods are static
     * - Completely stateless
     * - Single responsibility: chunk mesh generation
     * 
     * Assignment 1 simplified implementation:
     * - No face culling (renders all faces of all blocks)
     * - No greedy meshing optimization
     * - Direct block mesh copying to chunk mesh
     */
    class ChunkMeshHelper
    {
    public:
        // Delete constructors to prevent instantiation (Helper class pattern)
        ChunkMeshHelper()                                  = delete;
        ~ChunkMeshHelper()                                 = delete;
        ChunkMeshHelper(const ChunkMeshHelper&)            = delete;
        ChunkMeshHelper& operator=(const ChunkMeshHelper&) = delete;

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
        static std::unique_ptr<ChunkMesh> BuildMesh(Chunk* chunk);

        /**
         * @brief Rebuild an existing chunk's mesh
         * 
         * @param chunk Chunk to rebuild mesh for
         */
        static void RebuildMesh(Chunk* chunk);

    private:
        /**
         * @brief Add a single block's mesh to the chunk mesh
         *
         * @param chunkMesh Target chunk mesh
         * @param blockState Block state to render
         * @param blockPos Position of the block within the chunk (0-15 range)
         * @param iterator Block iterator for face culling
         */
        static void AddBlockToMesh(ChunkMesh* chunkMesh, BlockState* blockState, const BlockPos& blockPos, const class BlockIterator& iterator);

        /**
         * @brief Check if a block should be rendered
         *
         * Assignment 1: Simply check if block is not air
         *
         * @param blockState Block state to check
         * @param air Cached air block reference for comparison
         * @return True if block should be rendered
         */
        static bool ShouldRenderBlock(BlockState* blockState, const std::shared_ptr<registry::block::Block>& air);

        /**
         * @brief Check if a block face should be rendered (face culling)
         *
         * Assignment 2: Face culling implementation
         *
         * @param iterator Block iterator at current position
         * @param direction Direction of the face to check
         * @return True if face should be rendered
         */
        static bool ShouldRenderFace(const class BlockIterator& iterator, Direction direction);


        /**
         * @brief Get block position in chunk coordinates
         *
         * @param x, y, z Block coordinates within chunk (0-15)
         * @return BlockPos position for mesh transformation
         */
        static BlockPos GetBlockPosition(int x, int y, int z);
    };
}
