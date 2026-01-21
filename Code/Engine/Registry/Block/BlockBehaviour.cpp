#include "BlockBehaviour.hpp"
#include "Engine/Voxel/Block/BlockState.hpp"
#include "Engine/Voxel/Block/BlockPos.hpp"
#include "Engine/Core/EngineCommon.hpp"

namespace enigma::registry::block
{
    // Static default properties instance
    const BlockBehaviourProperties BlockBehaviour::s_defaultProperties = BlockBehaviourProperties::Of();

    // ============================================================
    // Light Properties Implementation
    // ============================================================

    int BlockBehaviour::GetLightBlock(voxel::BlockState* state, voxel::World* world, const voxel::BlockPos& pos) const
    {
        UNUSED(state);
        UNUSED(world);
        UNUSED(pos);

        const auto& props = GetBehaviourProperties();

        // If explicitly set, use that value
        if (props.lightBlock >= 0)
        {
            return props.lightBlock;
        }

        // [MINECRAFT REF] BlockBehaviour.java:278-284
        // Default logic:
        // - Solid blocks that can occlude: return 15 (fully opaque)
        // - Blocks that propagate skylight: return 0
        // - Otherwise: return 1

        if (props.canOcclude)
        {
            return 15;
        }

        if (props.propagatesSkylight)
        {
            return 0;
        }

        return 1;
    }

    bool BlockBehaviour::PropagatesSkylightDown(voxel::BlockState* state, voxel::World* world, const voxel::BlockPos& pos) const
    {
        UNUSED(state);
        UNUSED(world);
        UNUSED(pos);

        const auto& props = GetBehaviourProperties();

        // [MINECRAFT REF] BlockBehaviour.java:368-370
        // Default: !Block::IsShapeFullBlock(shape) && fluidState.IsEmpty()
        // 
        // Simplified: use the explicit propagatesSkylight property
        // For blocks with noOcclusion(), this should be true
        // For solid blocks, this should be false

        return props.propagatesSkylight && !props.canOcclude;
    }

    int BlockBehaviour::GetLightEmission(voxel::BlockState* state) const
    {
        UNUSED(state);

        const auto& props = GetBehaviourProperties();
        return props.lightEmission;
    }

    // ============================================================
    // Rendering Implementation
    // ============================================================

    bool BlockBehaviour::SkipRendering(voxel::BlockState* self, voxel::BlockState* neighbor, voxel::Direction dir) const
    {
        UNUSED(self);
        UNUSED(neighbor);
        UNUSED(dir);

        // [MINECRAFT REF] BlockBehaviour.java default returns false
        // Subclasses override for same-type culling (HalfTransparentBlock, LiquidBlock)
        return false;
    }

    RenderShape BlockBehaviour::GetRenderShape(voxel::BlockState* state) const
    {
        UNUSED(state);

        const auto& props = GetBehaviourProperties();
        return props.renderShape;
    }

    RenderType BlockBehaviour::GetRenderType() const
    {
        const auto& props = GetBehaviourProperties();
        return props.renderType;
    }

    // ============================================================
    // Collision and Shape Implementation
    // ============================================================

    voxel::VoxelShape BlockBehaviour::GetShape(voxel::BlockState* state, voxel::World* world, const voxel::BlockPos& pos) const
    {
        UNUSED(state);
        UNUSED(world);
        UNUSED(pos);

        const auto& props = GetBehaviourProperties();

        // Return full block shape if has collision, empty otherwise
        if (props.hasCollision)
        {
            return voxel::VoxelShape::Block();
        }
        return voxel::VoxelShape::Empty();
    }

    voxel::VoxelShape BlockBehaviour::GetCollisionShape(voxel::BlockState* state, voxel::World* world, const voxel::BlockPos& pos) const
    {
        UNUSED(state);
        UNUSED(world);
        UNUSED(pos);

        const auto& props = GetBehaviourProperties();

        // Return full block shape if has collision, empty otherwise
        if (props.hasCollision)
        {
            return voxel::VoxelShape::Block();
        }
        return voxel::VoxelShape::Empty();
    }

    // ============================================================
    // Properties Access Implementation
    // ============================================================

    const BlockBehaviourProperties& BlockBehaviour::GetBehaviourProperties() const
    {
        if (m_behaviourProperties)
        {
            return *m_behaviourProperties;
        }
        return s_defaultProperties;
    }

    bool BlockBehaviour::CanOcclude() const
    {
        return GetBehaviourProperties().canOcclude;
    }

    bool BlockBehaviour::HasCollision() const
    {
        return GetBehaviourProperties().hasCollision;
    }
}
