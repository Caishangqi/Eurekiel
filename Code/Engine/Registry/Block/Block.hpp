#pragma once
#include "../Core/IRegistrable.hpp"
#include "BlockBehaviour.hpp"
#include "../../Voxel/Block/VoxelShape.hpp"
#include "../../Voxel/Property/Property.hpp"
#include "../../Voxel/Property/PropertyMap.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "Engine/Core/EngineCommon.hpp"

namespace enigma::model
{
    class ModelSubsystem;
}

namespace enigma::resource
{
    class AtlasManager;
}

namespace enigma::voxel
{
    class BlockState;
    class FluidState;
    struct BlockPos;
    struct PlacementContext; // [NEW] Forward declaration for placement logic
}

namespace enigma::voxel
{
    class World;
}

namespace enigma::registry::block
{
    using namespace enigma::core;
    using namespace enigma::voxel;
    /**
     * @brief Private implementation class for Block
     * This allows us to hide the BlockState vector from the header
     */
    class BlockImpl;

    /**
     * @brief Base class for all block types
     * 
     * Similar to Minecraft's Block class, this defines the behavior and properties
     * of a block type. Individual BlockState instances represent specific configurations.
     * 
     * [MINECRAFT REF] Block.java extends BlockBehaviour
     * Inherits overridable behaviors from BlockBehaviour (light, rendering, collision)
     */
    class Block : public IRegistrable, public BlockBehaviour
    {
    protected:
        std::string                             m_registryName;
        std::string                             m_namespace;
        std::vector<std::shared_ptr<IProperty>> m_properties;

        // Use Pimpl to hide BlockState vector
        std::unique_ptr<BlockImpl> m_impl;

        // Block behavior properties
        float       m_hardness    = 1.0f;
        float       m_resistance  = 1.0f;
        bool        m_canOcclude  = true; // Whether this block can occlude neighbor faces (face culling)
        bool        m_isFullBlock = true;
        bool        m_isVisible   = true; // Block visibility flag (default: visible)
        std::string m_blockstatePath; // Path to blockstate definition
        uint8_t     m_blockLightEmission = 0; // Indoor light emission level (0-15)

    public:
        explicit Block(const std::string& registryName, const std::string& namespaceName = "");
        ~Block() override;

        // IRegistrable interface
        const std::string& GetRegistryName() const override { return m_registryName; }
        const std::string& GetNamespace() const override { return m_namespace; }

        // Property management
        template <typename T>
        void AddProperty(std::shared_ptr<Property<T>> property)
        {
            m_properties.push_back(property);
        }

        void AddProperty(std::shared_ptr<IProperty> property)
        {
            m_properties.push_back(property);
        }

        const std::vector<std::shared_ptr<IProperty>>& GetProperties() const { return m_properties; }

        /**
         * @brief Generate all possible BlockState combinations
         * Must be called after all properties are added
         */
        void GenerateBlockStates();

        /**
         * @brief Get all possible states for this block
         */
        std::vector<enigma::voxel::BlockState*> GetAllStates() const;

        /**
         * @brief Get the default state (usually first state)
         */
        enigma::voxel::BlockState* GetDefaultState() const;

        /**
         * @brief Get a specific state by property values
         */
        enigma::voxel::BlockState* GetState(const PropertyMap& properties) const;

        /**
         * @brief Get state by index (for fast lookup)
         */
        enigma::voxel::BlockState* GetStateByIndex(size_t index) const;

        size_t GetStateCount() const;

        // Block behavior properties
        float GetHardness() const { return m_hardness; }
        void  SetHardness(float hardness) { m_hardness = hardness; }

        float GetResistance() const { return m_resistance; }
        void  SetResistance(float resistance) { m_resistance = resistance; }

        bool CanOcclude() const { return m_canOcclude; }
        void SetCanOcclude(bool canOcclude) { m_canOcclude = canOcclude; }

        bool IsFullBlock() const { return m_isFullBlock; }
        void SetFullBlock(bool fullBlock) { m_isFullBlock = fullBlock; }

        bool IsVisible() const { return m_isVisible; }
        void SetVisible(bool visible) { m_isVisible = visible; }

        /**
         * @brief Get the indoor light emission level of this block type
         * @return Light level (0-15), where 0 = no light, 15 = maximum brightness
         */
        uint8_t GetBlockLightEmission() const { return m_blockLightEmission; }

        /**
         * @brief Set the indoor light emission level for this block type
         * @param level Light level (0-15), values > 15 will be clamped to 15
         */
        void SetBlockLightEmission(uint8_t level)
        {
            m_blockLightEmission = (level > 15) ? 15 : level;
        }

        // Blockstate path management
        const std::string& GetBlockstatePath() const { return m_blockstatePath; }
        void               SetBlockstatePath(const std::string& path) { m_blockstatePath = path; }

        // Virtual methods for block behavior
        virtual void OnPlaced(enigma::voxel::World* world, const enigma::voxel::BlockPos& pos, enigma::voxel::BlockState* state)
        {
            UNUSED(world);
            UNUSED(pos);
            UNUSED(state);
        }

        virtual void OnBroken(enigma::voxel::World* world, const enigma::voxel::BlockPos& pos, enigma::voxel::BlockState* state)
        {
            UNUSED(world);
            UNUSED(pos);
            UNUSED(state);
        }

        virtual void OnNeighborChanged(enigma::voxel::World* world, const enigma::voxel::BlockPos& pos, enigma::voxel::BlockState* state, Block* neighborBlock)
        {
            UNUSED(world);
            UNUSED(pos);
            UNUSED(state);
            UNUSED(neighborBlock);
        }

        /**
         * @brief Compile models for all block states
         * 
         * Should be called after GenerateBlockStates() and after ModelRegistry
         * and AtlasManager are initialized. This compiles the model for each
         * BlockState and caches the result.
         * 
         * @param modelRegistry Registry for loading models
         * @param atlasManager Manager for texture atlas access  
         */
        void CompileModels(enigma::model::ModelSubsystem*        modelSubsystem,
                           const enigma::resource::AtlasManager* atlasManager) const;

        /**
         * @brief Get the model path for a specific state
         * Override this to provide custom model selection logic
         */
        virtual std::string GetModelPath(enigma::voxel::BlockState* state) const;

        // [NEW] Virtual methods for advanced block placement and behavior (Non-Breaking)

        /**
         * @brief Determine the BlockState to use when placing this block
         * @param ctx Placement context (position, player aim, face, etc.)
         * @return The BlockState to place (default: GetDefaultState())
         * 
         * Override this to provide custom placement logic (e.g., stairs/slabs orientation).
         * Default implementation ignores context and returns the default state.
         */
        virtual enigma::voxel::BlockState* GetStateForPlacement(const PlacementContext& ctx) const;

        /**
         * @brief Check if a specific BlockState is opaque (blocks light)
         * @param state The BlockState to check
         * @return True if the state is opaque (default: m_isOpaque)
         * 
         * Override this to provide per-state opacity checks (e.g., slabs are opaque when double).
         * Default implementation uses the block-level m_isOpaque flag.
         */
        virtual bool IsOpaque(enigma::voxel::BlockState* state) const;

        /**
         * @brief Get the collision shape for a specific BlockState
         * @param state The BlockState to get collision shape for
         * @return VoxelShape representing the collision geometry
         * 
         * Override this to provide custom collision shapes for non-full blocks:
         * - Full blocks: Return Shapes::FullBlock()
         * - Slabs: Return Shapes::SlabBottom() or Shapes::SlabTop()
         * - Stairs: Return compound shape with multiple AABBs
         * - Air/Transparent: Return Shapes::Empty()
         * 
         * Default implementation returns full block shape if m_isFullBlock is true,
         * otherwise returns empty shape.
         * 
         * Used by World::RaycastVsBlocks() for precise collision detection.
         */
        virtual VoxelShape GetCollisionShape(enigma::voxel::BlockState* state) const;

        /**
         * @brief Check if this BlockState can be replaced during block placement
         * @param state The BlockState to check
         * @param ctx Placement context
         * @return True if the block can be replaced (default: false)
         * 
         * Override this to allow replacement (e.g., air, water, tall grass).
         * Default implementation returns false (blocks cannot be replaced).
         */
        virtual bool CanBeReplaced(enigma::voxel::BlockState* state, const PlacementContext& ctx) const;

        // ============================================================
        // BlockBehaviour overrides
        // [MINECRAFT REF] Block.java overrides from BlockBehaviour
        // ============================================================

        /**
         * @brief Get light attenuation value for this block state
         * Override from BlockBehaviour - uses m_canOcclude for default calculation
         */
        int GetLightBlock(voxel::BlockState* state, voxel::World* world, const voxel::BlockPos& pos) const override;

        /**
         * @brief Check if skylight propagates downward through this block
         * Override from BlockBehaviour - uses m_canOcclude and m_isFullBlock
         */
        bool PropagatesSkylightDown(voxel::BlockState* state, voxel::World* world, const voxel::BlockPos& pos) const override;

        /**
         * @brief Get light emission level for this block state
         * Override from BlockBehaviour - uses m_blockLightEmission
         */
        int GetLightEmission(voxel::BlockState* state) const override;

        /**
         * @brief Check if face should be skipped when rendering
         * Override from BlockBehaviour - default returns false
         * Subclasses (HalfTransparentBlock, LiquidBlock) override for same-type culling
         */
        bool SkipRendering(voxel::BlockState* self, voxel::BlockState* neighbor, voxel::Direction dir) const override;

        /**
         * @brief Get render shape type for this block state
         * Override from BlockBehaviour - returns MODEL by default
         */
        RenderShape GetRenderShape(voxel::BlockState* state) const override;

        /**
         * @brief Get render type for pass classification
         * Override from BlockBehaviour - returns SOLID by default
         */
        RenderType GetRenderType() const override;

        // ============================================================
        // Fluid State
        // [MINECRAFT REF] BlockBehaviour.getFluidState()
        // ============================================================

        /**
         * @brief Get the fluid state for this block state
         *
         * [MINECRAFT REF] BlockBehaviour.java:214-216
         * protected FluidState getFluidState(BlockState state) {
         *     return Fluids.EMPTY.defaultFluidState();
         * }
         *
         * Default implementation returns empty fluid state.
         * LiquidBlock overrides to return water/lava fluid state.
         *
         * Usage (replaces deprecated liquid() method):
         * ```cpp
         * // [OLD - Deprecated]
         * if (blockState->IsLiquid()) { ... }
         *
         * // [NEW - Recommended]
         * if (!block->GetFluidState(blockState).IsEmpty()) { ... }
         * ```
         *
         * @param state The block state to get fluid state for
         * @return FluidState (Empty for most blocks, Water/Lava for LiquidBlock)
         */
        virtual voxel::FluidState GetFluidState(voxel::BlockState* state) const;

    protected:
        /**
         * @brief Called during state generation to initialize each state
         * Override to add custom state initialization
         */
        virtual void InitializeState(enigma::voxel::BlockState* state, const PropertyMap& properties)
        {
            UNUSED(state);
            UNUSED(properties);
        }

    private:
        void GenerateStatesRecursive(size_t propertyIndex, PropertyMap& currentMap, std::vector<PropertyMap>& allCombinations);
    };
}
