#include "LeavesBlock.hpp"
#include "Engine/Voxel/Block/BlockState.hpp"
#include "Engine/Core/EngineCommon.hpp"

namespace enigma::registry::block
{
    // Static member initialization
    bool                                    LeavesBlock::s_propertiesInitialized = false;
    std::shared_ptr<voxel::IntProperty>     LeavesBlock::DISTANCE                = nullptr;
    std::shared_ptr<voxel::BooleanProperty> LeavesBlock::PERSISTENT              = nullptr;
    std::shared_ptr<voxel::BooleanProperty> LeavesBlock::WATERLOGGED             = nullptr;

    void LeavesBlock::InitializeProperties()
    {
        if (s_propertiesInitialized)
        {
            return;
        }

        // [MINECRAFT REF] LeavesBlock.java static properties
        // DISTANCE: 1-7, represents distance from nearest log
        // PERSISTENT: true if player-placed (won't decay)
        // WATERLOGGED: true if contains water
        DISTANCE    = std::make_shared<voxel::IntProperty>("distance", 1, 7);
        PERSISTENT  = std::make_shared<voxel::BooleanProperty>("persistent");
        WATERLOGGED = std::make_shared<voxel::BooleanProperty>("waterlogged");

        s_propertiesInitialized = true;
    }

    LeavesBlock::LeavesBlock(const std::string& registryName,
                             const std::string& namespaceName,
                             bool               addDefaultProperties)
        : Block(registryName, namespaceName)
    {
        // Initialize static properties if needed
        InitializeProperties();

        // [REFACTOR] Leaves cannot occlude neighboring faces (light passes through)
        m_canOcclude = false;

        // Leaves are full blocks (for collision purposes)
        m_isFullBlock = true;

        // Add default properties if requested
        if (addDefaultProperties)
        {
            AddProperty(DISTANCE);
            AddProperty(PERSISTENT);
            AddProperty(WATERLOGGED);
        }
    }

    int LeavesBlock::GetLightBlock(voxel::BlockState* state, voxel::World* world, const voxel::BlockPos& pos) const
    {
        UNUSED(state);
        UNUSED(world);
        UNUSED(pos);

        // [MINECRAFT REF] LeavesBlock.java:69-71
        // protected int getLightBlock(BlockState state, BlockGetter getter, BlockPos pos) {
        //     return 1;
        // }
        //
        // Leaves always return 1 regardless of state or position.
        // This is different from the default calculation which depends on shape.
        return 1;
    }

    RenderType LeavesBlock::GetRenderType() const
    {
        // Leaves use alpha test (CUTOUT) for rendering
        // This is more efficient than TRANSLUCENT because:
        // 1. No depth sorting required
        // 2. Hard edges (pixels are either fully opaque or fully transparent)
        // 3. Better performance with many overlapping leaves
        return RenderType::CUTOUT;
    }
}
