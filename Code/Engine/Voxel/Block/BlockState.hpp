#pragma once
#include "../State/StateHolder.hpp"
#include "../Property/PropertyMap.hpp"
#include "BlockPos.hpp"
#include "../Fluid/FluidState.hpp"
#include <memory>

#include "Engine/Registry/Block/Block.hpp"

// Forward declarations
namespace enigma::registry::block
{
    class Block;
}

namespace enigma::renderer::model
{
    class RenderMesh;
}

namespace enigma::voxel
{
    class World;
}

namespace enigma::voxel
{
    /**
     * @brief Runtime instance representing a specific configuration of a Block
     *
     * [MINECRAFT REF] BlockBehaviour.BlockStateBase extends StateHolder<Block, BlockState>
     * File: net/minecraft/world/level/block/state/BlockBehaviour.java:1180
     *
     * Similar to Minecraft's BlockState, this represents a Block with specific
     * property values. Each unique combination of properties gets its own BlockState.
     *
     * [REFACTORED - 2026-01-22]
     * Now inherits from StateHolder<Block, BlockState> following Minecraft's architecture:
     * - StateHolder provides: owner, values (PropertyMap), neighbours table, setValue/cycle
     * - BlockStateBase adds: fluidState cache, lightCache, rendering cache
     *
     * [ARCHITECTURE CHANGE - 2025-11-15]
     * Light data (outdoor/indoor light) and flags (IsSky, IsLightDirty) have been
     * moved to Chunk's independent arrays (Chunk::m_lightData, Chunk::m_flags) to
     * avoid data pollution from shared BlockState instances.
     *
     * BlockState now stores:
     * - [From StateHolder] owner (Block*), values (PropertyMap)
     * - State index (m_stateIndex)
     * - Cached FluidState (m_fluidState) - [NEW]
     * - Cached rendering data (m_cachedMesh)
     * - Cached light properties (m_lightCache)
     */
    class BlockState : public StateHolder<enigma::registry::block::Block, BlockState>
    {
    private:
        // State index within Block's state list
        size_t m_stateIndex = 0;

        // ============================================================
        // [NEW] FluidState cache
        // [MINECRAFT REF] BlockBehaviour.BlockStateBase.fluidState
        // File: net/minecraft/world/level/block/state/BlockBehaviour.java:1195
        // Cached at initCache() time for O(1) access
        // ============================================================
        mutable FluidState m_fluidState;
        mutable bool       m_fluidStateCached = false;

        // Cached rendering data
        mutable std::shared_ptr<enigma::renderer::model::RenderMesh> m_cachedMesh;
        mutable bool                                                 m_meshCacheValid = false;

        // ============================================================
        // Light property cache
        // [MINECRAFT REF] BlockBehaviour.BlockStateBase.Cache inner class
        // File: net/minecraft/world/level/block/state/BlockBehaviour.java:1234-1247
        // Cached at BlockState creation time for O(1) light queries
        // ============================================================
        struct LightCache
        {
            int8_t lightBlock             = -1; // Light attenuation (0-15), -1 = not cached
            int8_t lightEmission          = 0; // Light emission (0-15)
            bool   propagatesSkylightDown = true; // Whether skylight passes through
            bool   isValid                = false; // Whether cache has been initialized
        };

        mutable LightCache m_lightCache;

    public:
        BlockState(enigma::registry::block::Block* blockType, const PropertyMap& properties, size_t stateIndex);

        virtual ~BlockState() = default;

        // ============================================================
        // Basic Accessors
        // [MINECRAFT REF] BlockStateBase.getBlock()
        // ============================================================

        /**
         * @brief Get the Block type for this state
         * [MINECRAFT REF] BlockStateBase.getBlock() -> returns (Block)this.owner
         */
        enigma::registry::block::Block* GetBlock() const { return m_owner; }

        /**
         * @brief Get property values (alias for StateHolder::GetValues())
         */
        const PropertyMap& GetProperties() const { return m_values; }

        /**
         * @brief Get state index within Block's state list
         */
        size_t GetStateIndex() const { return m_stateIndex; }

        // ============================================================
        // Property Access
        // [MINECRAFT REF] StateHolder.getValue() - inherited
        // ============================================================

        /**
         * @brief Get a property value (type-safe)
         * Delegates to StateHolder::GetValue()
         */
        template <typename T>
        T Get(std::shared_ptr<Property<T>> property) const
        {
            return GetValue(property);
        }

        /**
         * @brief Create a new BlockState with one property changed
         * [MINECRAFT REF] StateHolder.setValue()
         */
        template <typename T>
        BlockState* With(std::shared_ptr<Property<T>> property, const T& value) const;

        // ============================================================
        // Comparison and Hashing
        // ============================================================

        bool operator==(const BlockState& other) const
        {
            return m_owner == other.m_owner && m_values == other.m_values;
        }

        bool operator!=(const BlockState& other) const
        {
            return !(*this == other);
        }

        size_t GetHash() const
        {
            return StateHolder::GetHash();
        }

        // ============================================================
        // Block Behavior Delegation
        // [MINECRAFT REF] BlockStateBase delegates to Block methods
        // ============================================================

        /**
         * @brief Check if this block can occlude adjacent faces for face culling
         * [MINECRAFT REF] BlockStateBase.canOcclude()
         */
        inline bool CanOcclude() const
        {
            return m_owner ? m_owner->CanOcclude() : true;
        }

        bool  IsOpaque() const;
        bool  IsFullBlock() const;
        float GetHardness() const;
        float GetResistance() const;

        // ============================================================
        // Model and Rendering
        // ============================================================

        std::string                                          GetModelPath() const;
        std::shared_ptr<enigma::renderer::model::RenderMesh> GetRenderMesh() const;

        void SetRenderMesh(std::shared_ptr<enigma::renderer::model::RenderMesh> mesh) const
        {
            m_cachedMesh     = mesh;
            m_meshCacheValid = true;
        }

        void InvalidateRenderCache() const
        {
            m_meshCacheValid = false;
        }

        // ============================================================
        // World Interaction
        // ============================================================

        void OnPlaced(enigma::voxel::World* world, const BlockPos& pos) const;
        void OnBroken(enigma::voxel::World* world, const BlockPos& pos) const;
        void OnNeighborChanged(enigma::voxel::World* world, const BlockPos& pos, enigma::registry::block::Block* neighborBlock) const;

        // ============================================================
        // Utility
        // ============================================================

        std::string ToString() const;

        virtual bool CanBeReplaced() const { return false; }
        virtual int  GetLightLevel() const { return 0; }
        virtual bool BlocksLight() const { return IsOpaque(); }

        // ============================================================
        // Fluid State (with caching)
        // [MINECRAFT REF] BlockBehaviour.BlockStateBase.getFluidState()
        // File: net/minecraft/world/level/block/state/BlockBehaviour.java:1195
        // ============================================================

        /**
         * @brief Get the fluid state for this block state (cached)
         *
         * [MINECRAFT REF] BlockBehaviour.BlockStateBase.getFluidState()
         * Returns cached fluidState member, initialized in initCache()
         *
         * [OPTIMIZATION] FluidState is now cached instead of computed on every call.
         * Cache is initialized via InitCache() after BlockState creation.
         *
         * Usage (replaces deprecated liquid() method):
         * ```cpp
         * // [OLD - Deprecated in Minecraft]
         * if (blockState->IsLiquid()) { ... }
         *
         * // [NEW - Recommended]
         * if (!blockState->GetFluidState().IsEmpty()) { ... }
         * ```
         *
         * @return FluidState (Empty for most blocks, Water/Lava for LiquidBlock)
         */
        FluidState GetFluidState() const
        {
            if (!m_fluidStateCached)
            {
                m_fluidState       = m_owner ? m_owner->GetFluidState(const_cast<BlockState*>(this)) : FluidState::Empty();
                m_fluidStateCached = true;
            }
            return m_fluidState;
        }

        /**
         * @brief Check if fluid state cache is valid
         */
        bool IsFluidStateCached() const { return m_fluidStateCached; }

        /**
         * @brief Invalidate fluid state cache
         */
        void InvalidateFluidStateCache() const { m_fluidStateCached = false; }

        // ============================================================
        // Light Cache Methods
        // [MINECRAFT REF] BlockBehaviour.java:865-870 cache access
        // ============================================================

        int  GetLightBlock(World* world, const BlockPos& pos) const;
        bool PropagatesSkylightDown(World* world, const BlockPos& pos) const;
        int  GetLightEmission() const;

        /**
         * @brief Initialize all caches (light, fluid state)
         *
         * [MINECRAFT REF] BlockBehaviour.BlockStateBase.initCache()
         * File: net/minecraft/world/level/block/state/BlockBehaviour.java:1246-1247
         *
         * Called after BlockState creation to pre-compute cached properties.
         */
        void InitCache(World* world = nullptr, const BlockPos& pos = BlockPos(0, 0, 0)) const;

        /**
         * @brief Initialize light cache only (legacy compatibility)
         */
        void InitializeLightCache(World* world = nullptr, const BlockPos& pos = BlockPos(0, 0, 0)) const;

        bool IsLightCacheValid() const { return m_lightCache.isValid; }
        void InvalidateLightCache() const { m_lightCache.isValid = false; }

        /**
         * @brief Invalidate all caches
         */
        void InvalidateAllCaches() const
        {
            m_fluidStateCached   = false;
            m_lightCache.isValid = false;
            m_meshCacheValid     = false;
        }
    };
}

// Hash specialization for BlockState
namespace std
{
    template <>
    struct hash<enigma::voxel::BlockState>
    {
        size_t operator()(const enigma::voxel::BlockState& state) const noexcept
        {
            return state.GetHash();
        }
    };
}
