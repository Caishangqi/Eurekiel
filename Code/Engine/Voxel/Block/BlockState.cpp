#include "BlockState.hpp"
#include "../../Registry/Block/Block.hpp"
#include "../../Renderer/Model/BlockRenderMesh.hpp"
#include "../../Model/ModelSubsystem.hpp"
#include "../../Core/EngineCommon.hpp"
#include "../../Registry/Block/BlockRegistry.hpp"
#include "../../Resource/BlockState/BlockStateDefinition.hpp"

namespace enigma::voxel
{
    template <typename T>
    BlockState* BlockState::With(std::shared_ptr<Property<T>> property, const T& value) const
    {
        // Get the new property map
        PropertyMap newProperties = m_properties.With(property, value);

        // Find the corresponding state in the block's state list
        return m_blockType->GetState(newProperties);
    }

    BlockState::BlockState(enigma::registry::block::Block* blockType, const PropertyMap& properties, size_t stateIndex)
        : m_blockType(blockType), m_properties(properties), m_stateIndex(stateIndex)
    {
        // [REMOVED] Flag initialization - flags now managed by Chunk class
        // No initialization needed here
    }

    bool BlockState::IsOpaque() const
    {
        return m_blockType ? m_blockType->IsOpaque(const_cast<BlockState*>(this)) : true;
    }

    bool BlockState::IsFullBlock() const
    {
        return m_blockType ? m_blockType->IsFullBlock() : true;
    }

    float BlockState::GetHardness() const
    {
        return m_blockType ? m_blockType->GetHardness() : 1.0f;
    }

    float BlockState::GetResistance() const
    {
        return m_blockType ? m_blockType->GetResistance() : 1.0f;
    }

    std::string BlockState::GetModelPath() const
    {
        return m_blockType ? m_blockType->GetModelPath(const_cast<BlockState*>(this)) : "";
    }

    std::shared_ptr<enigma::renderer::model::RenderMesh> BlockState::GetRenderMesh() const
    {
        if (!m_meshCacheValid || !m_cachedMesh)
        {
            if (m_blockType)
            {
                std::string blockName            = m_blockType->GetNamespace() + ":" + m_blockType->GetRegistryName();
                auto        blockStateDefinition = enigma::registry::block::BlockRegistry::GetBlockStateDefinition(blockName);

                if (blockStateDefinition)
                {
                    // Convert property map to string for variant lookup
                    std::string propertyString = "";
                    if (!m_properties.Empty())
                    {
                        std::string fullString = m_properties.ToString();
                        if (fullString.size() >= 2 && fullString.front() == '{' && fullString.back() == '}')
                        {
                            propertyString = fullString.substr(1, fullString.size() - 2);
                        }
                        else
                        {
                            propertyString = fullString;
                        }
                    }

                    const auto* variants = blockStateDefinition->GetVariants(propertyString);
                    if (variants && !variants->empty())
                    {
                        m_cachedMesh = variants->front().compiledMesh;
                    }
                }
            }
            m_meshCacheValid = true;
        }
        return m_cachedMesh;
    }

    std::string BlockState::ToString() const
    {
        if (m_blockType)
        {
            return m_blockType->GetRegistryName() + m_properties.ToString();
        }
        return m_properties.ToString();
    }

    void BlockState::OnPlaced(enigma::voxel::World* world, const BlockPos& pos) const
    {
        if (m_blockType)
        {
            m_blockType->OnPlaced(world, pos, const_cast<BlockState*>(this));
        }
    }

    void BlockState::OnBroken(enigma::voxel::World* world, const BlockPos& pos) const
    {
        if (m_blockType)
        {
            m_blockType->OnBroken(world, pos, const_cast<BlockState*>(this));
        }
    }

    void BlockState::OnNeighborChanged(enigma::voxel::World* world, const BlockPos& pos, enigma::registry::block::Block* neighborBlock) const
    {
        if (m_blockType)
        {
            m_blockType->OnNeighborChanged(world, pos, const_cast<BlockState*>(this), neighborBlock);
        }
    }

    // Explicit template instantiations for common property types
    template BlockState* BlockState::With<bool>(std::shared_ptr<Property<bool>>, const bool&) const;
    template BlockState* BlockState::With<int>(std::shared_ptr<Property<int>>, const int&) const;
    template BlockState* BlockState::With<Direction>(std::shared_ptr<Property<Direction>>, const Direction&) const;

    // ============================================================
    // [NEW] Light cache methods implementation
    // [MINECRAFT REF] BlockBehaviour.java:865-870, 1234-1247
    // ============================================================

    int BlockState::GetLightBlock(World* world, const BlockPos& pos) const
    {
        // [MINECRAFT REF] BlockBehaviour.java:869-870
        // return this.cache != null ? this.cache.lightBlock : this.getBlock().getLightBlock(...)

        // If cache is valid and has a computed value, use it
        if (m_lightCache.isValid && m_lightCache.lightBlock >= 0)
        {
            return m_lightCache.lightBlock;
        }

        // Otherwise, compute from block type
        if (m_blockType)
        {
            return m_blockType->GetLightBlock(const_cast<BlockState*>(this), world, pos);
        }

        // Default: fully opaque
        return 15;
    }

    bool BlockState::PropagatesSkylightDown(World* world, const BlockPos& pos) const
    {
        // [MINECRAFT REF] BlockBehaviour.java:865-866
        // return this.cache != null ? this.cache.propagatesSkylightDown : ...

        // If cache is valid, use cached value
        if (m_lightCache.isValid)
        {
            return m_lightCache.propagatesSkylightDown;
        }

        // Otherwise, compute from block type
        if (m_blockType)
        {
            return m_blockType->PropagatesSkylightDown(const_cast<BlockState*>(this), world, pos);
        }

        // Default: opaque blocks don't propagate skylight
        return false;
    }

    int BlockState::GetLightEmission() const
    {
        // If cache is valid, use cached value
        if (m_lightCache.isValid)
        {
            return m_lightCache.lightEmission;
        }

        // Otherwise, compute from block type
        if (m_blockType)
        {
            return m_blockType->GetLightEmission(const_cast<BlockState*>(this));
        }

        // Default: no light emission
        return 0;
    }

    void BlockState::InitializeLightCache(World* world, const BlockPos& pos) const
    {
        // [MINECRAFT REF] BlockBehaviour.java:1246-1247
        // Cache is populated during block registration for O(1) access

        if (!m_blockType)
        {
            // No block type, use defaults
            m_lightCache.lightBlock             = 15;
            m_lightCache.lightEmission          = 0;
            m_lightCache.propagatesSkylightDown = false;
            m_lightCache.isValid                = true;
            return;
        }

        // Compute and cache all light properties
        m_lightCache.propagatesSkylightDown = m_blockType->PropagatesSkylightDown(
            const_cast<BlockState*>(this), world, pos);

        m_lightCache.lightBlock = static_cast<int8_t>(m_blockType->GetLightBlock(
            const_cast<BlockState*>(this), world, pos));

        m_lightCache.lightEmission = static_cast<int8_t>(m_blockType->GetLightEmission(
            const_cast<BlockState*>(this)));

        m_lightCache.isValid = true;
    }
}
