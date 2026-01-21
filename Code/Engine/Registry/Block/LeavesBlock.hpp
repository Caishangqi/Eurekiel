#pragma once
#include "Block.hpp"
#include "Engine/Voxel/Property/PropertyTypes.hpp"

namespace enigma::registry::block
{
    /**
     * @brief Leaves block with fixed light blocking behavior
     * 
     * Key behaviors:
     * - GetLightBlock() returns 1 (fixed, not computed from shape)
     * - Does NOT override PropagatesSkylightDown (uses default based on noOcclusion)
     * - Has distance/persistent/waterlogged properties for decay mechanics
     * - Uses CUTOUT render type (alpha test, no depth sorting needed)
     * 
     * [MINECRAFT REF] LeavesBlock.java
     * 
     * [DEVIATION] We simplify the decay mechanism, keeping only light-related behavior.
     * Full decay mechanics can be added later if needed.
     * 
     * Examples: oak_leaves, birch_leaves, spruce_leaves, jungle_leaves, etc.
     */
    class LeavesBlock : public Block
    {
    public:
        // Standard leaves properties
        // [MINECRAFT REF] LeavesBlock.java static properties
        static std::shared_ptr<voxel::IntProperty>     DISTANCE; // Distance from log (1-7)
        static std::shared_ptr<voxel::BooleanProperty> PERSISTENT; // Player-placed (won't decay)
        static std::shared_ptr<voxel::BooleanProperty> WATERLOGGED; // Contains water

        /**
         * @brief Construct a LeavesBlock
         * @param registryName The block's registry name
         * @param namespaceName The namespace (default: empty)
         * @param addDefaultProperties If true, adds DISTANCE, PERSISTENT, WATERLOGGED properties
         */
        explicit LeavesBlock(const std::string& registryName,
                             const std::string& namespaceName        = "",
                             bool               addDefaultProperties = true);

        ~LeavesBlock() override = default;

        /**
         * @brief Fixed light block value of 1
         * 
         * [MINECRAFT REF] LeavesBlock.java:69-71
         * protected int getLightBlock(...) { return 1; }
         * 
         * Unlike the default implementation which computes based on shape,
         * leaves always return 1 regardless of state.
         */
        int GetLightBlock(voxel::BlockState* state, voxel::World* world, const voxel::BlockPos& pos) const override;

        /**
         * @brief Get render type - cutout for alpha testing
         * 
         * Leaves use alpha test (CUTOUT) rather than alpha blend (TRANSLUCENT)
         * because they have hard edges (fully opaque or fully transparent pixels).
         * This avoids the need for depth sorting.
         * 
         * @return RenderType::CUTOUT
         */
        RenderType GetRenderType() const override;

        // Note: PropagatesSkylightDown is NOT overridden
        // Uses default: !IsShapeFullBlock && fluidState.IsEmpty()
        // For leaves with noOcclusion (m_isOpaque = false), this returns true
        // This matches Minecraft behavior where skylight passes through leaves

    private:
        /**
         * @brief Initialize the static property instances
         * Called once when first LeavesBlock is created
         */
        static void InitializeProperties();
        static bool s_propertiesInitialized;
    };
}
