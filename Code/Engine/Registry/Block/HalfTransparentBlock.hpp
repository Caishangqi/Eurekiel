#pragma once
#include "Block.hpp"

namespace enigma::registry::block
{
    /**
     * @brief Base class for semi-transparent blocks (stained glass, ice, etc.)
     * 
     * Key behavior: Same-type face culling via SkipRendering()
     * When two blocks of the same type are adjacent, the shared face is not rendered.
     * 
     * [MINECRAFT REF] HalfTransparentBlock.java:19-21
     * 
     * Examples of blocks that should inherit from this:
     * - Stained glass (all colors)
     * - Ice
     * - Slime block
     * - Honey block
     */
    class HalfTransparentBlock : public Block
    {
    public:
        /**
         * @brief Construct a HalfTransparentBlock
         * @param registryName The block's registry name
         * @param namespaceName The namespace (default: empty)
         */
        explicit HalfTransparentBlock(const std::string& registryName, const std::string& namespaceName = "");

        ~HalfTransparentBlock() override = default;

        /**
         * @brief Skip rendering when neighbor is same block type
         * 
         * [MINECRAFT REF] HalfTransparentBlock.java:19-21
         * return blockState2.is(this) ? true : super.skipRendering(blockState, blockState2, direction);
         * 
         * @param self The block state being rendered
         * @param neighbor The neighboring block state
         * @param dir The direction from self to neighbor
         * @return True if the face should not be rendered
         */
        bool SkipRendering(voxel::BlockState* self, voxel::BlockState* neighbor, voxel::Direction dir) const override;

        /**
         * @brief Get render type - translucent for alpha blending
         * @return RenderType::TRANSLUCENT
         */
        RenderType GetRenderType() const override;
    };
}
