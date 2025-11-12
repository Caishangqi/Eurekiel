#pragma once
#include "../Property/PropertyMap.hpp"
#include "BlockPos.hpp"
#include <memory>

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
     */
    class BlockState
    {
    public:
        /**
         * @brief Flag bits for block state properties
         * Each flag occupies one bit in m_flags (uint8_t)
         */
        enum class Flags : uint8_t
        {
            IS_SKY = 0x01, // Bit 0: Block is exposed to sky
            IS_LIGHT_DIRTY = 0x02, // Bit 1: Light needs recalculation
            IS_FULL_OPAQUE = 0x04, // Bit 2: Block is fully opaque (no light passes through)
            IS_SOLID = 0x08, // Bit 3: Block is solid (collision)
            IS_VISIBLE = 0x10 // Bit 4: Block is visible (should be rendered)
        };

    private:
        enigma::registry::block::Block* m_blockType;
        PropertyMap                     m_properties;
        size_t                          m_stateIndex;

        // Cached rendering data
        mutable std::shared_ptr<enigma::renderer::model::RenderMesh> m_cachedMesh;
        mutable bool                                                 m_meshCacheValid = false;

        // Light data and flags
        uint8_t m_lightData = 0; // High 4 bits: outdoor light (0-15), Low 4 bits: indoor light (0-15)
        uint8_t m_flags     = 0; // Bit flags: IsSky, IsLightDirty, IsFullOpaque, IsSolid, IsVisible

        // Flag bit operations (private helpers)
        inline bool HasFlag(Flags flag) const
        {
            return (m_flags & static_cast<uint8_t>(flag)) != 0;
        }

        inline void SetFlag(Flags flag, bool value)
        {
            if (value)
                m_flags |= static_cast<uint8_t>(flag);
            else
                m_flags &= ~static_cast<uint8_t>(flag);
        }

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

        // Light data access (bit-packed: high 4 bits = outdoor, low 4 bits = indoor)
        inline uint8_t GetOutdoorLight() const
        {
            return (m_lightData >> 4) & 0x0F;
        }

        inline void SetOutdoorLight(uint8_t level)
        {
            m_lightData = (m_lightData & 0x0F) | ((level & 0x0F) << 4);
        }

        inline uint8_t GetIndoorLight() const
        {
            return m_lightData & 0x0F;
        }

        inline void SetIndoorLight(uint8_t level)
        {
            m_lightData = (m_lightData & 0xF0) | (level & 0x0F);
        }

        // Flag accessors (semantic interface for bit flags)
        inline bool IsSky() const { return HasFlag(Flags::IS_SKY); }
        inline void SetIsSky(bool value) { SetFlag(Flags::IS_SKY, value); }

        inline bool IsLightDirty() const { return HasFlag(Flags::IS_LIGHT_DIRTY); }
        inline void SetIsLightDirty(bool value) { SetFlag(Flags::IS_LIGHT_DIRTY, value); }

        inline bool IsFullOpaque() const { return HasFlag(Flags::IS_FULL_OPAQUE); }
        inline void SetIsFullOpaque(bool value) { SetFlag(Flags::IS_FULL_OPAQUE, value); }

        inline bool IsSolid() const { return HasFlag(Flags::IS_SOLID); }
        inline void SetIsSolid(bool value) { SetFlag(Flags::IS_SOLID, value); }

        inline bool IsVisible() const { return HasFlag(Flags::IS_VISIBLE); }
        inline void SetIsVisible(bool value) { SetFlag(Flags::IS_VISIBLE, value); }

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
