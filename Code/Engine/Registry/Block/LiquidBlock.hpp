#pragma once
#include "Block.hpp"
#include "FluidType.hpp"
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

    private:
        FluidType m_fluidType;

        /**
         * @brief Initialize the static property instances
         */
        static void InitializeProperties();
        static bool s_propertiesInitialized;
    };
}
