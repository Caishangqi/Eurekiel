#pragma once
#include "../Property/PropertyMap.hpp"
#include "BlockPos.hpp"
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
     * Similar to Minecraft's BlockState, this represents a Block with specific
     * property values. Each unique combination of properties gets its own BlockState.
     * 
     * [ARCHITECTURE CHANGE - 2025-11-15]
     * Light data (outdoor/indoor light) and flags (IsSky, IsLightDirty) have been
     * moved to Chunk's independent arrays (Chunk::m_lightData, Chunk::m_flags) to
     * avoid data pollution from shared BlockState instances.
     * 
     * BlockState now only stores:
     * - Block type pointer (m_blockType)
     * - Property values (m_properties)
     * - State index (m_stateIndex)
     * - Cached rendering data (m_cachedMesh)
     */
    class BlockState
    {
    private:
        enigma::registry::block::Block* m_blockType;
        PropertyMap                     m_properties;
        size_t                          m_stateIndex;

        // Cached rendering data
        mutable std::shared_ptr<enigma::renderer::model::RenderMesh> m_cachedMesh;
        mutable bool                                                 m_meshCacheValid = false;

        // ============================================================
        // [NEW] Light property cache
        // [MINECRAFT REF] BlockBehaviour.java:1234-1247 Cache inner class
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

        // Basic accessors
        enigma::registry::block::Block* GetBlock() const { return m_blockType; }
        const PropertyMap&              GetProperties() const { return m_properties; }
        size_t                          GetStateIndex() const { return m_stateIndex; }

        // Property access
        template <typename T>
        T Get(std::shared_ptr<Property<T>> property) const
        {
            return m_properties.Get(property);
        }

        /**
         * @brief Create a new BlockState with one property changed
         */
        template <typename T>
        BlockState* With(std::shared_ptr<Property<T>> property, const T& value) const;

        // Comparison (used for fast state lookup)
        bool operator==(const BlockState& other) const
        {
            return m_blockType == other.m_blockType && m_properties == other.m_properties;
        }

        bool operator!=(const BlockState& other) const
        {
            return !(*this == other);
        }

        // Hash for fast lookup
        size_t GetHash() const
        {
            size_t hash = std::hash<void*>{}(m_blockType);
            hash        ^= m_properties.GetHash() + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            return hash;
        }

        // [REMOVED] Light data and flag accessors - migrated to Chunk class
        // Use Chunk::GetSkyLight(), Chunk::SetSkyLight(), etc. instead

        /**
         * @brief Check if this block can occlude adjacent faces for face culling
         * @details Used by ChunkMeshHelper::ShouldRenderFace() to determine if
         *          a neighbor block should cause the current block's face to be culled.
         *          
         * [MINECRAFT REF] BlockStateBase.canOcclude() - Returns true for full solid blocks
         * that can hide adjacent faces. Blocks like leaves return false even though
         * they may block light, because they have transparent parts.
         * 
         * @return true if this block can occlude adjacent faces (full solid blocks)
         * @return false if adjacent faces should always render (leaves, glass, etc.)
         */
        inline bool CanOcclude() const
        {
            return m_blockType ? m_blockType->CanOcclude() : true;
        }

        // Block behavior delegation
        bool  IsOpaque() const;
        bool  IsFullBlock() const;
        float GetHardness() const;
        float GetResistance() const;

        // Model and rendering
        std::string                                          GetModelPath() const;
        std::shared_ptr<enigma::renderer::model::RenderMesh> GetRenderMesh() const;

        /**
         * @brief Set the cached render mesh for this block state
         * Called by BlockModelCompiler after compiling the model
         */
        void SetRenderMesh(std::shared_ptr<enigma::renderer::model::RenderMesh> mesh) const
        {
            m_cachedMesh     = mesh;
            m_meshCacheValid = true;
        }

        /**
         * @brief Invalidate cached rendering data
         * Call when model resources change
         */
        void InvalidateRenderCache() const
        {
            m_meshCacheValid = false;
        }

        // World interaction
        void OnPlaced(enigma::voxel::World* world, const BlockPos& pos) const;
        void OnBroken(enigma::voxel::World* world, const BlockPos& pos) const;
        void OnNeighborChanged(enigma::voxel::World* world, const BlockPos& pos, enigma::registry::block::Block* neighborBlock) const;

        // Utility
        std::string ToString() const;

        /**
         * @brief Check if this state can be replaced by another block
         * (e.g., air, water, tall grass can usually be replaced)
         */
        virtual bool CanBeReplaced() const { return false; }

        /**
         * @brief Get light level emitted by this block state
         */
        virtual int GetLightLevel() const { return 0; }

        /**
         * @brief Check if this block state blocks light
         */
        virtual bool BlocksLight() const { return IsOpaque(); }

        // ============================================================
        // [NEW] Light cache methods
        // [MINECRAFT REF] BlockBehaviour.java:865-870 cache access
        // ============================================================

        /**
         * @brief Get light attenuation value with caching
         * @param world The world (may be nullptr for cached value)
         * @param pos The block position
         * @return Light attenuation 0-15
         * 
         * [MINECRAFT REF] BlockBehaviour.java:869-870
         * return this.cache != null ? this.cache.lightBlock : this.getBlock().getLightBlock(...)
         */
        int GetLightBlock(World* world, const BlockPos& pos) const;

        /**
         * @brief Check if skylight propagates down through this block with caching
         * @param world The world (may be nullptr for cached value)
         * @param pos The block position
         * @return True if skylight passes through vertically
         * 
         * [MINECRAFT REF] BlockBehaviour.java:865-866
         */
        bool PropagatesSkylightDown(World* world, const BlockPos& pos) const;

        /**
         * @brief Get light emission value with caching
         * @return Light emission 0-15
         */
        int GetLightEmission() const;

        /**
         * @brief Initialize the light cache
         * Called after BlockState creation to pre-compute light properties.
         * 
         * [MINECRAFT REF] BlockBehaviour.java:1246-1247
         * Cache is populated during block registration.
         * 
         * @param world The world (may be nullptr for default calculation)
         * @param pos The block position (may be origin for default calculation)
         */
        void InitializeLightCache(World* world = nullptr, const BlockPos& pos = BlockPos(0, 0, 0)) const;

        /**
         * @brief Check if light cache is valid
         */
        bool IsLightCacheValid() const { return m_lightCache.isValid; }

        /**
         * @brief Invalidate light cache (call when block properties change)
         */
        void InvalidateLightCache() const { m_lightCache.isValid = false; }
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
