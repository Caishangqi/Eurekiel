#pragma once
#include "Engine/Registry/Block/Block.hpp"
#include "Engine/Voxel/Property/PropertyTypes.hpp"
#include "Engine/Voxel/Block/SlabType.hpp"
#include "Engine/Voxel/Block/PlacementContext.hpp"

namespace enigma::voxel
{
    /**
     * @brief Slab block implementation with half-detection and merging logic
     *
     * SlabBlock represents partial blocks that can exist in three states:
     * - BOTTOM: Lower half of the block (Y: 0.0 - 0.5)
     * - TOP: Upper half of the block (Y: 0.5 - 1.0)
     * - DOUBLE: Full block created by merging two slabs (Y: 0.0 - 1.0)
     *
     * Key Features:
     * - Raycast-based half detection using PlacementContext::IsTopHalf()
     * - Auto-merging when placing on same slab type (BOTTOM + TOP → DOUBLE)
     * - Per-state opacity: Only DOUBLE slabs are opaque and block light
     * - Block-level properties: opaque=false, full_block=false (YAML can override for DOUBLE)
     *
     * Placement Logic:
     * 1. Click bottom half → Place BOTTOM slab
     * 2. Click top half → Place TOP slab
     * 3. Click opposite half of existing slab → Merge to DOUBLE slab
     *
     * Example Usage:
     * @code
     * auto oakSlab = std::make_shared<SlabBlock>("oak_slab", "simpleminer");
     * registry->Register(oakSlab);
     * @endcode
     */
    class SlabBlock : public registry::block::Block
    {
    private:
        std::shared_ptr<EnumProperty<SlabType>> m_typeProperty; ///< Property defining slab type (bottom/top/double)

    public:
        /**
         * @brief Construct a new SlabBlock
         * @param registryName Unique registry name (e.g., "oak_slab")
         * @param ns Namespace (default: "", uses global namespace)
         *
         * Constructor automatically:
         * - Creates EnumProperty<SlabType> with 3 values (bottom, top, double)
         * - Sets block-level flags: opaque=false, full_block=false
         * - Registers the property and generates block states (3 states total)
         */
        explicit SlabBlock(const std::string& registryName, const std::string& ns = "");

        /**
         * @brief Destructor
         */
        ~SlabBlock() override = default;

        /**
         * @brief Determine BlockState for placement based on raycast hit point
         * @param ctx Placement context containing hit point and player state
         * @return BlockState with type=BOTTOM (hit lower half) or TOP (hit upper half)
         *
         * Uses PlacementContext::IsTopHalf() to detect which half was clicked:
         * - hitPoint.z < 0.5 → BOTTOM slab
         * - hitPoint.z >= 0.5 → TOP slab
         *
         * Note: This method does NOT handle merging. Merging is handled by
         * CanBeReplaced() which allows World::SetBlock() to replace existing slabs.
         */
        enigma::voxel::BlockState* GetStateForPlacement(const PlacementContext& ctx) const override;

        /**
         * @brief Check if existing slab can be replaced to form a Double Slab
         * @param state Current BlockState at the target position
         * @param ctx Placement context (held item, hit point)
         * @return True if replacement should occur (merge to DOUBLE), false otherwise
         *
         * Replacement Logic:
         * 1. Must hold the SAME slab type (ctx.heldItemBlock == this)
         * 2. Cannot replace DOUBLE slabs (already full)
         * 3. Can merge if clicking opposite half:
         *    - BOTTOM slab + click top half → DOUBLE
         *    - TOP slab + click bottom half → DOUBLE
         *
         * When this returns true, World::SetBlock() will:
         * 1. Delete the old slab BlockState
         * 2. Place new DOUBLE slab BlockState
         */
        bool CanBeReplaced(enigma::voxel::BlockState* state, const PlacementContext& ctx) const override;

        /**
         * @brief Check if specific BlockState is opaque (blocks light)
         * @param state BlockState to check
         * @return True only if state type is DOUBLE
         *
         * Opacity Rules:
         * - BOTTOM: Not opaque (empty top half allows light)
         * - TOP: Not opaque (empty bottom half allows light)
         * - DOUBLE: Opaque (full block, blocks light completely)
         *
         * This override enables per-state lighting calculations in the voxel renderer.
         */
        bool IsOpaque(enigma::voxel::BlockState* state) const override;

        /**
         * @brief Get model path for specific slab state
         * @param state BlockState to get model for
         * @return Model path string (e.g., "simpleminer:block/oak_slab_bottom")
         *
         * Model Path Format:
         * - BOTTOM: "{namespace}:block/{registryName}_bottom"
         * - TOP: "{namespace}:block/{registryName}_top"
         * - DOUBLE: "{namespace}:block/{registryName}_double"
         *
         * Example:
         * - oak_slab (BOTTOM) → "simpleminer:block/oak_slab_bottom"
         * - oak_slab (TOP) → "simpleminer:block/oak_slab_top"
         * - oak_slab (DOUBLE) → "simpleminer:block/oak_slab_double"
         */
        std::string GetModelPath(enigma::voxel::BlockState* state) const override;

    protected:
        /**
         * @brief Initialize BlockState during state generation
         * @param state BlockState being initialized
         * @param properties Property map for this state
         *
         * Called by Block::GenerateBlockStates() for each property combination.
         * Currently no custom initialization needed beyond base class.
         */
        void InitializeState(enigma::voxel::BlockState* state, const PropertyMap& properties) override;
    };
}
