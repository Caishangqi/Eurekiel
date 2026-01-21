#pragma once
#include "Chunk.hpp"
#include "ChunkMesh.hpp"
#include "../Block/BlockState.hpp"
#include "../../Voxel/Property/PropertyTypes.hpp"
#include "Engine/Registry/Block/RenderType.hpp"
#include "Engine/Registry/Block/RenderShape.hpp"
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
     * [REFACTORED] Now supports three render types:
     * - SOLID: Fully opaque blocks (stone, dirt)
     * - CUTOUT: Alpha-tested blocks (leaves, grass) - no depth sorting
     * - TRANSLUCENT: Alpha-blended blocks (water, glass) - requires depth sorting
     * 
     * [MINECRAFT REF] ItemBlockRenderTypes / RenderType classification
     * [MINECRAFT REF] ModelBlockRenderer face culling with SkipRendering
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
         * [REFACTORED] Algorithm:
         * 1. Iterate through all blocks in chunk
         * 2. Skip air blocks and INVISIBLE render shape blocks
         * 3. Determine render type (SOLID/CUTOUT/TRANSLUCENT)
         * 4. Apply face culling with SkipRendering support
         * 5. Route faces to appropriate mesh buffer
         * 
         * @param chunk Source chunk data
         * @return Compiled chunk mesh ready for rendering
         */
        static std::unique_ptr<ChunkMesh> BuildMesh(Chunk* chunk);

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
         * [REFACTORED] Checks:
         * 1. Block is not air
         * 2. RenderShape is not INVISIBLE (liquids handled by dedicated renderer)
         *
         * @param blockState Block state to check
         * @param air Cached air block reference for comparison
         * @return True if block should be rendered
         */
        static bool ShouldRenderBlock(BlockState* blockState, const std::shared_ptr<registry::block::Block>& air);

        /**
         * @brief Check if a block face should be rendered (face culling)
         *
         * [REFACTORED] Now uses Block::SkipRendering for same-type culling:
         * - HalfTransparentBlock: culls same-type faces (glass-glass)
         * - LiquidBlock: culls same-fluid faces (water-water)
         *
         * [MINECRAFT REF] ModelBlockRenderer face culling
         *
         * @param iterator Block iterator at current position
         * @param direction Direction of the face to check
         * @return True if face should be rendered
         */
        static bool ShouldRenderFace(const class BlockIterator& iterator, Direction direction);

        /**
         * @brief Determine render type for a block
         *
         * [NEW] Priority:
         * 1. RenderShape::INVISIBLE -> skip entirely
         * 2. Block's explicit RenderType from GetRenderType()
         *
         * [MINECRAFT REF] ItemBlockRenderTypes classification
         *
         * @param blockState Block state to classify
         * @return RenderType (SOLID, CUTOUT, or TRANSLUCENT)
         */
        static registry::block::RenderType GetBlockRenderType(BlockState* blockState);

        /**
         * @brief Get block position in chunk coordinates
         *
         * @param x, y, z Block coordinates within chunk (0-15)
         * @return BlockPos position for mesh transformation
         */
        static BlockPos GetBlockPosition(int x, int y, int z);
    };
}
