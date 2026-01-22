#pragma once
#include "Block.hpp"
#include "Engine/Voxel/Fluid/FluidState.hpp"
#include "Engine/Voxel/Fluid/FluidType.hpp"
#include "Engine/Voxel/Property/PropertyTypes.hpp"

namespace enigma::registry::block
{
    /**
     * @brief Liquid block (water, lava)
     * 
     * Key behaviors:
     * - PropagatesSkylightDown() returns false (liquids block skylight)
     * - SkipRendering() checks FluidType match (same fluid type = skip face)
     * - GetRenderShape() returns INVISIBLE (uses dedicated LiquidBlockRenderer)
     * - Has LEVEL property (0-15) for fluid height
     * 
     * [MINECRAFT REF] LiquidBlock.java:78-97
     * 
     * Examples: water, lava
     */
    class LiquidBlock : public Block
    {
    public:
        // Standard liquid property
        // [MINECRAFT REF] LiquidBlock.java LEVEL property
        static std::shared_ptr<voxel::IntProperty> LEVEL; // Fluid level (0-15, 0 = source)

        /**
         * @brief Construct a LiquidBlock
         * @param registryName The block's registry name
         * @param namespaceName The namespace (default: empty)
         * @param fluidType The type of fluid (WATER or LAVA)
         */
        explicit LiquidBlock(const std::string& registryName,
                             const std::string& namespaceName,
                             FluidType          fluidType);

        ~LiquidBlock() override = default;

        /**
         * @brief Get the fluid type of this liquid block
         */
        FluidType GetFluidType() const { return m_fluidType; }

        /**
         * @brief Liquids do not propagate skylight
         * 
         * [MINECRAFT REF] LiquidBlock.java:78-80
         * protected boolean propagatesSkylightDown(...) { return false; }
         */
        bool PropagatesSkylightDown(voxel::BlockState* state, voxel::World* world, const voxel::BlockPos& pos) const override;

        /**
         * @brief Skip rendering when neighbor has same fluid type
         * 
         * [MINECRAFT REF] LiquidBlock.java:91-93
         * return neighbor.getFluidState().getType().isSame(this.fluid);
         * 
         * This prevents rendering internal faces between adjacent water/lava blocks.
         */
        bool SkipRendering(voxel::BlockState* self, voxel::BlockState* neighbor, voxel::Direction dir) const override;

        /**
         * @brief Liquids use dedicated renderer, not standard model
         * 
         * [MINECRAFT REF] LiquidBlock.java:95-97
         * protected RenderShape getRenderShape(...) { return RenderShape.INVISIBLE; }
         * 
         * Liquids are rendered by LiquidBlockRenderer, not the standard block renderer.
         */
        RenderShape GetRenderShape(voxel::BlockState* state) const override;

        /**
         * @brief Get render type - translucent for water, opaque for lava
         */
        RenderType GetRenderType() const override;

        /**
         * @brief Liquids can always be replaced
         */
        bool CanBeReplaced(voxel::BlockState* state, const voxel::PlacementContext& ctx) const override;

        /**
         * @brief Liquids have no collision
         */
        voxel::VoxelShape GetCollisionShape(voxel::BlockState* state) const override;

        /**
         * @brief Get the fluid state for this liquid block (cached)
         *
         * [MINECRAFT REF] LiquidBlock.java getFluidState()
         * File: net/minecraft/world/level/block/LiquidBlock.java:67-70
         *
         * ```java
         * protected FluidState getFluidState(BlockState blockState) {
         *     int i = (Integer)blockState.getValue(LEVEL);
         *     return (FluidState)this.stateCache.get(Math.min(i, 8));
         * }
         * ```
         *
         * [OPTIMIZATION] Returns cached FluidState for O(1) access.
         * Since our FluidState is simplified (no LEVEL property in FluidState),
         * we cache a single FluidState per fluid type.
         *
         * @param state The block state (unused in simplified version)
         * @return Cached FluidState with the appropriate fluid type
         */
        voxel::FluidState GetFluidState(voxel::BlockState* state) const override;

    private:
        FluidType m_fluidType;

        // ============================================================
        // [NEW] FluidState cache
        // [MINECRAFT REF] LiquidBlock.java stateCache
        // File: net/minecraft/world/level/block/LiquidBlock.java:35
        //
        // Minecraft caches 9 FluidStates (source + 7 flowing + 1 falling).
        // Our simplified version caches a single FluidState per fluid type.
        // ============================================================
        mutable FluidState m_cachedFluidState;
        mutable bool       m_fluidStateCacheInitialized = false;

        /**
         * @brief Initialize the static property instances
         */
        static void InitializeProperties();
        static bool s_propertiesInitialized;
    };
}
