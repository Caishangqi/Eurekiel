#include "HalfTransparentBlock.hpp"
#include "Engine/Voxel/Block/BlockState.hpp"

namespace enigma::registry::block
{
    HalfTransparentBlock::HalfTransparentBlock(const std::string& registryName, const std::string& namespaceName)
        : Block(registryName, namespaceName)
    {
        // [REFACTOR] Semi-transparent blocks cannot occlude neighboring faces
        m_canOcclude = false;
    }

    bool HalfTransparentBlock::SkipRendering(voxel::BlockState* self, voxel::BlockState* neighbor, voxel::Direction dir) const
    {
        // [MINECRAFT REF] HalfTransparentBlock.java:19-21
        // return blockState2.is(this) ? true : super.skipRendering(blockState, blockState2, direction);

        if (neighbor)
        {
            // Same block type check using pointer comparison for O(1) performance
            // This works because each Block instance is unique in the registry
            if (neighbor->GetBlock() == this)
            {
                return true;
            }
        }

        // Fall back to parent implementation
        return Block::SkipRendering(self, neighbor, dir);
    }

    RenderType HalfTransparentBlock::GetRenderType() const
    {
        // Semi-transparent blocks use alpha blending and require depth sorting
        return RenderType::TRANSLUCENT;
    }
}
