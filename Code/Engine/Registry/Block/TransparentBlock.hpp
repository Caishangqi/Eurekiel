#pragma once
#include "HalfTransparentBlock.hpp"

namespace enigma::registry::block
{
    /**
     * @brief Fully transparent block (glass, glass panes, etc.)
     * 
     * Inherits from HalfTransparentBlock and adds:
     * - PropagatesSkylightDown() returns true (skylight passes through)
     * - GetLightBlock() returns 0 (no light attenuation)
     * 
     * [MINECRAFT REF] TransparentBlock.java extends HalfTransparentBlock
     * 
     * Key difference from HalfTransparentBlock:
     * - HalfTransparentBlock: blocks some light (ice, stained glass)
     * - TransparentBlock: blocks no light (clear glass)
     * 
     * Examples: glass, glass_pane, barrier (invisible)
     */
    class TransparentBlock : public HalfTransparentBlock
    {
    public:
        /**
         * @brief Construct a TransparentBlock
         * @param registryName The block's registry name
         * @param namespaceName The namespace (default: empty)
         */
        explicit TransparentBlock(const std::string& registryName, const std::string& namespaceName = "");

        ~TransparentBlock() override = default;

        /**
         * @brief Skylight propagates through transparent blocks
         * 
         * [MINECRAFT REF] TransparentBlock.java overrides to return true
         * This allows skylight to pass through glass without attenuation.
         */
        bool PropagatesSkylightDown(voxel::BlockState* state, voxel::World* world, const voxel::BlockPos& pos) const override;

        /**
         * @brief Transparent blocks do not attenuate light
         * 
         * @return 0 (no light blocking)
         */
        int GetLightBlock(voxel::BlockState* state, voxel::World* world, const voxel::BlockPos& pos) const override;
    };
}
