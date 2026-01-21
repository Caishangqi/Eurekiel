#include "LiquidBlock.hpp"
#include "Engine/Voxel/Block/BlockState.hpp"
#include "Engine/Voxel/Block/VoxelShape.hpp"
#include "Engine/Core/EngineCommon.hpp"

namespace enigma::registry::block
{
    // Static member initialization
    bool                                LiquidBlock::s_propertiesInitialized = false;
    std::shared_ptr<voxel::IntProperty> LiquidBlock::LEVEL                   = nullptr;

    void LiquidBlock::InitializeProperties()
    {
        if (s_propertiesInitialized)
        {
            return;
        }

        // [MINECRAFT REF] LiquidBlock.java LEVEL property
        // Level 0 = source block (full height)
        // Level 1-7 = flowing water (decreasing height)
        // Level 8-15 = falling water
        LEVEL = std::make_shared<voxel::IntProperty>("level", 0, 15);

        s_propertiesInitialized = true;
    }

    LiquidBlock::LiquidBlock(const std::string& registryName,
                             const std::string& namespaceName,
                             FluidType          fluidType)
        : Block(registryName, namespaceName)
          , m_fluidType(fluidType)
    {
        // Initialize static properties if needed
        InitializeProperties();

        // [REFACTOR] Liquids cannot occlude neighboring faces
        m_canOcclude = false;

        // Liquids are not full blocks (no collision)
        m_isFullBlock = false;

        // Add the LEVEL property
        AddProperty(LEVEL);
    }

    bool LiquidBlock::PropagatesSkylightDown(voxel::BlockState* state, voxel::World* world, const voxel::BlockPos& pos) const
    {
        UNUSED(state);
        UNUSED(world);
        UNUSED(pos);

        // [MINECRAFT REF] LiquidBlock.java:78-80
        // protected boolean propagatesSkylightDown(BlockState state, BlockGetter getter, BlockPos pos) {
        //     return false;
        // }
        //
        // Liquids block skylight from propagating downward.
        // This is why underwater areas are dark even during the day.
        return false;
    }

    bool LiquidBlock::SkipRendering(voxel::BlockState* self, voxel::BlockState* neighbor, voxel::Direction dir) const
    {
        UNUSED(self);
        UNUSED(dir);

        // [MINECRAFT REF] LiquidBlock.java:91-93
        // protected boolean skipRendering(BlockState state, BlockState neighbor, Direction dir) {
        //     return neighbor.getFluidState().getType().isSame(this.fluid);
        // }

        if (!neighbor)
        {
            return false;
        }

        // Check if neighbor is also a LiquidBlock with the same fluid type
        const Block*       neighborBlock  = neighbor->GetBlock();
        const LiquidBlock* neighborLiquid = dynamic_cast<const LiquidBlock*>(neighborBlock);

        if (neighborLiquid && neighborLiquid->GetFluidType() == m_fluidType)
        {
            return true;
        }

        return false;
    }

    RenderShape LiquidBlock::GetRenderShape(voxel::BlockState* state) const
    {
        UNUSED(state);

        // Returns MODEL to use standard block rendering pipeline.
        // TODO: Implement dedicated FluidRenderer (SubRenderer pattern) for:
        //   - Animated water/lava textures
        //   - Variable height based on LEVEL property (0=full, 1-7=decreasing)
        //   - Flow direction visualization
        //   - Proper face culling between adjacent liquid blocks
        // Reference: Minecraft's LiquidBlockRenderer + Iris MixinDefaultFluidRenderer
        return RenderShape::MODEL;
    }

    RenderType LiquidBlock::GetRenderType() const
    {
        // Water is translucent (alpha blend)
        // Lava is opaque (no transparency)
        if (m_fluidType == FluidType::WATER)
        {
            return RenderType::TRANSLUCENT;
        }
        return RenderType::SOLID;
    }

    bool LiquidBlock::CanBeReplaced(voxel::BlockState* state, const voxel::PlacementContext& ctx) const
    {
        UNUSED(state);
        UNUSED(ctx);

        // Liquids can always be replaced by placing blocks into them
        return true;
    }

    voxel::VoxelShape LiquidBlock::GetCollisionShape(voxel::BlockState* state) const
    {
        UNUSED(state);

        // Liquids have no collision - entities pass through them
        // (Swimming physics are handled separately)
        return voxel::Shapes::Empty();
    }
}
