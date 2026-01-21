#pragma once
#include "BlockBehaviourProperties.hpp"
#include "RenderShape.hpp"
#include "RenderType.hpp"
#include "Engine/Voxel/Block/VoxelShape.hpp"
#include "Engine/Voxel/Property/PropertyTypes.hpp"
#include <memory>

// Forward declarations
namespace enigma::voxel
{
    class BlockState;
    class World;
    struct BlockPos;
}

namespace enigma::registry::block
{
    /**
     * @brief Abstract base class defining overridable block behaviors
     * 
     * This class defines the interface for all block behaviors that can be
     * overridden by specific block types. It follows Minecraft's BlockBehaviour
     * pattern but adapted for C++.
     * 
     * [MINECRAFT REF] BlockBehaviour.java
     * [DEVIATION] Minecraft uses Java abstract class, we use C++ virtual functions with default implementations
     * 
     * Key behaviors defined here:
     * - Light properties (GetLightBlock, PropagatesSkylightDown, GetLightEmission)
     * - Rendering (SkipRendering, GetRenderShape)
     * - Collision and shape (GetShape, GetCollisionShape)
     */
    class BlockBehaviour
    {
    public:
        virtual ~BlockBehaviour() = default;

        // ============================================================
        // Light Properties
        // [MINECRAFT REF] BlockBehaviour.java:278-284, 368-370
        // ============================================================

        /**
         * @brief Get light attenuation value for this block state
         * @param state The block state to query
         * @param world The world (may be nullptr for default calculation)
         * @param pos The block position
         * @return Light attenuation 0-15, where 15 = fully opaque, 0 = fully transparent
         * 
         * [MINECRAFT REF] BlockBehaviour.java:278-284
         * Default logic:
         * - If properties specify lightBlock >= 0: return that value
         * - If solid render (canOcclude): return 15
         * - If propagates skylight: return 0
         * - Otherwise: return 1
         */
        virtual int GetLightBlock(voxel::BlockState* state, voxel::World* world, const voxel::BlockPos& pos) const;

        /**
         * @brief Check if skylight propagates downward through this block
         * @param state The block state to query
         * @param world The world (may be nullptr for default calculation)
         * @param pos The block position
         * @return True if skylight passes through vertically without attenuation
         * 
         * [MINECRAFT REF] BlockBehaviour.java:368-370
         * Default: !IsShapeFullBlock(shape) && fluidState.IsEmpty()
         * 
         * Override examples:
         * - LiquidBlock: returns false (liquids block skylight)
         * - TransparentBlock: returns true (glass allows skylight)
         */
        virtual bool PropagatesSkylightDown(voxel::BlockState* state, voxel::World* world, const voxel::BlockPos& pos) const;

        /**
         * @brief Get light emission level for this block state
         * @param state The block state to query
         * @return Light emission 0-15, where 0 = no light, 15 = maximum brightness
         * 
         * [MINECRAFT REF] Properties.lightEmission (ToIntFunction<BlockState>)
         */
        virtual int GetLightEmission(voxel::BlockState* state) const;

        // ============================================================
        // Rendering
        // [MINECRAFT REF] BlockBehaviour.java skipRendering, getRenderShape
        // ============================================================

        /**
         * @brief Check if face should be skipped when rendering
         * @param self The block state being rendered
         * @param neighbor The neighboring block state
         * @param dir The direction from self to neighbor
         * @return True if the face should not be rendered
         * 
         * [MINECRAFT REF] BlockBehaviour.java (default returns false)
         * 
         * Override examples:
         * - HalfTransparentBlock: returns true if neighbor is same block type
         * - LiquidBlock: returns true if neighbor has same fluid type
         */
        virtual bool SkipRendering(voxel::BlockState* self, voxel::BlockState* neighbor, voxel::Direction dir) const;

        /**
         * @brief Get render shape type for this block state
         * @param state The block state to query
         * @return RenderShape determining how the block is rendered
         * 
         * [MINECRAFT REF] BlockBehaviour.java (default returns MODEL)
         * 
         * Override examples:
         * - LiquidBlock: returns INVISIBLE (uses dedicated LiquidBlockRenderer)
         * - Chest: returns ENTITYBLOCK_ANIMATED
         */
        virtual RenderShape GetRenderShape(voxel::BlockState* state) const;

        /**
         * @brief Get render type for pass classification
         * @return RenderType determining which render pass to use
         */
        virtual RenderType GetRenderType() const;

        // ============================================================
        // Collision and Shape
        // ============================================================

        /**
         * @brief Get the visual/selection shape for this block state
         * @param state The block state to query
         * @param world The world
         * @param pos The block position
         * @return VoxelShape representing the visual bounds
         */
        virtual voxel::VoxelShape GetShape(voxel::BlockState* state, voxel::World* world, const voxel::BlockPos& pos) const;

        /**
         * @brief Get the collision shape for this block state
         * @param state The block state to query
         * @param world The world
         * @param pos The block position
         * @return VoxelShape representing the collision bounds
         */
        virtual voxel::VoxelShape GetCollisionShape(voxel::BlockState* state, voxel::World* world, const voxel::BlockPos& pos) const;

        // ============================================================
        // Properties Access
        // ============================================================

        /**
         * @brief Get the behaviour properties
         * @return Const reference to properties, or default if not set
         */
        const BlockBehaviourProperties& GetBehaviourProperties() const;

        /**
         * @brief Check if this block can occlude neighbors
         */
        bool CanOcclude() const;

        /**
         * @brief Check if this block has collision
         */
        bool HasCollision() const;

    protected:
        // Properties holder (similar to Minecraft's BlockBehaviour.Properties)
        std::unique_ptr<BlockBehaviourProperties> m_behaviourProperties;

        // Default properties instance for when m_behaviourProperties is null
        static const BlockBehaviourProperties s_defaultProperties;
    };
}
