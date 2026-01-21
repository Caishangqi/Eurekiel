#pragma once
#include "RenderShape.hpp"
#include "RenderType.hpp"

namespace enigma::registry::block
{
    /**
     * @brief Block behaviour configuration using Builder pattern
     * 
     * This class defines the fixed properties of a block type (hardness, light blocking, etc.)
     * These are configured at block creation time and do not change at runtime.
     * 
     * [MINECRAFT REF] BlockBehaviour.Properties
     * 
     * [IMPORTANT] This class is completely different from enigma::voxel::Property<T>:
     * - Property<T>: BlockState runtime properties (e.g., distance, waterlogged)
     * - BlockBehaviourProperties: Block fixed configuration (e.g., hardness, lightBlock)
     * 
     * Usage example:
     *   auto props = BlockBehaviourProperties::Of()
     *       .Strength(0.2f, 0.2f)
     *       .NoOcclusion()
     *       .LightBlock(1)
     *       .SetRenderType(RenderType::CUTOUT);
     */
    class BlockBehaviourProperties
    {
    public:
        // ============================================================
        // Light Properties
        // ============================================================

        int  lightBlock         = -1; // Light attenuation (0-15), -1 = use default calculation
        int  lightEmission      = 0; // Light emission level (0-15)
        bool propagatesSkylight = true; // Whether skylight passes through vertically

        // ============================================================
        // Rendering Properties
        // ============================================================

        bool        canOcclude  = true; // Whether this block occludes neighbors
        RenderShape renderShape = RenderShape::MODEL; // How the block is rendered
        RenderType  renderType  = RenderType::SOLID; // Which render pass to use

        // ============================================================
        // Physics Properties
        // ============================================================

        bool  hasCollision        = true; // Whether entities collide with this block
        float destroyTime         = 1.0f; // Time to break (hardness)
        float explosionResistance = 1.0f; // Resistance to explosions

        // ============================================================
        // Builder Methods (Chainable)
        // ============================================================

        /**
         * @brief Disable occlusion (for transparent/translucent blocks)
         * [MINECRAFT REF] BlockBehaviour.Properties.noOcclusion()
         */
        BlockBehaviourProperties& NoOcclusion()
        {
            canOcclude = false;
            return *this;
        }

        /**
         * @brief Set light emission level
         * @param level Light level (0-15)
         * [MINECRAFT REF] BlockBehaviour.Properties.lightLevel()
         */
        BlockBehaviourProperties& LightLevel(int level)
        {
            lightEmission = (level > 15) ? 15 : ((level < 0) ? 0 : level);
            return *this;
        }

        /**
         * @brief Set light blocking value
         * @param block Light attenuation (0-15), -1 for default calculation
         */
        BlockBehaviourProperties& LightBlock(int block)
        {
            lightBlock = (block > 15) ? 15 : block;
            return *this;
        }

        /**
         * @brief Set whether skylight propagates down through this block
         */
        BlockBehaviourProperties& PropagatesSkylight(bool propagates)
        {
            propagatesSkylight = propagates;
            return *this;
        }

        /**
         * @brief Set block strength (hardness and resistance)
         * @param hardness Time to break
         * @param resistance Explosion resistance
         * [MINECRAFT REF] BlockBehaviour.Properties.strength()
         */
        BlockBehaviourProperties& Strength(float hardness, float resistance)
        {
            destroyTime         = hardness;
            explosionResistance = resistance;
            return *this;
        }

        /**
         * @brief Set block strength with same value for both
         * @param strength Both hardness and resistance
         */
        BlockBehaviourProperties& Strength(float strength)
        {
            destroyTime         = strength;
            explosionResistance = strength;
            return *this;
        }

        /**
         * @brief Disable collision (for non-solid blocks like air, flowers)
         * [MINECRAFT REF] BlockBehaviour.Properties.noCollission()
         */
        BlockBehaviourProperties& NoCollision()
        {
            hasCollision = false;
            canOcclude   = false;
            return *this;
        }

        /**
         * @brief Set render type for pass classification
         * @param type SOLID, CUTOUT, or TRANSLUCENT
         */
        BlockBehaviourProperties& SetRenderType(RenderType type)
        {
            renderType = type;
            return *this;
        }

        /**
         * @brief Set render shape
         * @param shape MODEL, INVISIBLE, or ENTITYBLOCK_ANIMATED
         */
        BlockBehaviourProperties& SetRenderShape(RenderShape shape)
        {
            renderShape = shape;
            return *this;
        }

        /**
         * @brief Make block indestructible (like bedrock)
         * [MINECRAFT REF] BlockBehaviour.Properties.indestructible()
         */
        BlockBehaviourProperties& Indestructible()
        {
            destroyTime         = -1.0f;
            explosionResistance = 3600000.0f;
            return *this;
        }

        // ============================================================
        // Factory Method
        // ============================================================

        /**
         * @brief Create a new properties instance with default values
         */
        static BlockBehaviourProperties Of()
        {
            return BlockBehaviourProperties();
        }

        /**
         * @brief Copy properties from another block type
         * [MINECRAFT REF] BlockBehaviour.Properties.ofFullCopy()
         */
        static BlockBehaviourProperties Copy(const BlockBehaviourProperties& other)
        {
            return other;
        }
    };
}
