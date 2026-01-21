#include "TransparentBlock.hpp"
#include "Engine/Voxel/Block/BlockState.hpp"
#include "Engine/Core/EngineCommon.hpp"

namespace enigma::registry::block
{
    TransparentBlock::TransparentBlock(const std::string& registryName, const std::string& namespaceName)
        : HalfTransparentBlock(registryName, namespaceName)
    {
        // [REFACTOR] Transparent blocks cannot occlude (inherited from HalfTransparentBlock)
        // m_canOcclude is already set to false in HalfTransparentBlock constructor
    }

    bool TransparentBlock::PropagatesSkylightDown(voxel::BlockState* state, voxel::World* world, const voxel::BlockPos& pos) const
    {
        UNUSED(state);
        UNUSED(world);
        UNUSED(pos);

        // [MINECRAFT REF] TransparentBlock.java
        // Transparent blocks allow skylight to pass through completely.
        // This is the key difference from HalfTransparentBlock.
        //
        // Example: Standing under glass during the day, you still get full skylight.
        return true;
    }

    int TransparentBlock::GetLightBlock(voxel::BlockState* state, voxel::World* world, const voxel::BlockPos& pos) const
    {
        UNUSED(state);
        UNUSED(world);
        UNUSED(pos);

        // Transparent blocks do not attenuate light at all.
        // Light passes through without any reduction.
        return 0;
    }
}
