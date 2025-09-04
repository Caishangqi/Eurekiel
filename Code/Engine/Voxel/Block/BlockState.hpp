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

namespace enigma::voxel::world
{
    class World;
}

namespace enigma::voxel::block
{
    using namespace enigma::voxel::property;

    /**
     * @brief Runtime instance representing a specific configuration of a Block
     * 
     * Similar to Minecraft's BlockState, this represents a Block with specific
     * property values. Each unique combination of properties gets its own BlockState.
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

    public:
        BlockState(enigma::registry::block::Block* blockType, const PropertyMap& properties, size_t stateIndex)
            : m_blockType(blockType), m_properties(properties), m_stateIndex(stateIndex)
        {
        }

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
            hash ^= m_properties.GetHash() + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            return hash;
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
        void OnPlaced(enigma::voxel::world::World* world, const BlockPos& pos) const;
        void OnBroken(enigma::voxel::world::World* world, const BlockPos& pos) const;
        void OnNeighborChanged(enigma::voxel::world::World* world, const BlockPos& pos, enigma::registry::block::Block* neighborBlock) const;

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
    struct hash<enigma::voxel::block::BlockState>
    {
        size_t operator()(const enigma::voxel::block::BlockState& state) const noexcept
        {
            return state.GetHash();
        }
    };
}
