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
        PropertyMap newProperties = m_values.With(property, value);

        // Find the corresponding state in the block's state list
        return m_owner->GetState(newProperties);
    }

    BlockState::BlockState(enigma::registry::block::Block* blockType, const PropertyMap& properties, size_t stateIndex)
        : StateHolder<enigma::registry::block::Block, BlockState>(blockType, properties)
          , m_stateIndex(stateIndex)
    {
        // [MINECRAFT REF] BlockStateBase constructor
        // Caches are initialized lazily or via InitCache()
    }

    bool BlockState::IsOpaque() const
    {
        return m_owner ? m_owner->IsOpaque(const_cast<BlockState*>(this)) : true;
    }

    bool BlockState::IsFullBlock() const
    {
        return m_owner ? m_owner->IsFullBlock() : true;
    }

    float BlockState::GetHardness() const
    {
        return m_owner ? m_owner->GetHardness() : 1.0f;
    }

    float BlockState::GetResistance() const
    {
        return m_owner ? m_owner->GetResistance() : 1.0f;
    }

    std::string BlockState::GetModelPath() const
    {
        return m_owner ? m_owner->GetModelPath(const_cast<BlockState*>(this)) : "";
    }

    std::shared_ptr<enigma::renderer::model::RenderMesh> BlockState::GetRenderMesh() const
    {
        if (!m_meshCacheValid || !m_cachedMesh)
        {
            if (m_owner)
            {
                std::string blockName            = m_owner->GetNamespace() + ":" + m_owner->GetRegistryName();
                auto        blockStateDefinition = enigma::registry::block::BlockRegistry::GetBlockStateDefinition(blockName);

                if (blockStateDefinition)
                {
                    // Convert property map to string for variant lookup
                    std::string propertyString = "";
                    if (!m_values.Empty())
                    {
                        std::string fullString = m_values.ToString();
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
        if (m_owner)
        {
            return m_owner->GetRegistryName() + m_values.ToString();
        }
        return m_values.ToString();
    }

    void BlockState::OnPlaced(enigma::voxel::World* world, const BlockPos& pos) const
    {
        if (m_owner)
        {
            m_owner->OnPlaced(world, pos, const_cast<BlockState*>(this));
        }
    }

    void BlockState::OnBroken(enigma::voxel::World* world, const BlockPos& pos) const
    {
        if (m_owner)
        {
            m_owner->OnBroken(world, pos, const_cast<BlockState*>(this));
        }
    }

    void BlockState::OnNeighborChanged(enigma::voxel::World* world, const BlockPos& pos, enigma::registry::block::Block* neighborBlock) const
    {
        if (m_owner)
        {
            m_owner->OnNeighborChanged(world, pos, const_cast<BlockState*>(this), neighborBlock);
        }
    }

    // Explicit template instantiations for common property types
    template BlockState* BlockState::With<bool>(std::shared_ptr<Property<bool>>, const bool&) const;
    template BlockState* BlockState::With<int>(std::shared_ptr<Property<int>>, const int&) const;
    template BlockState* BlockState::With<Direction>(std::shared_ptr<Property<Direction>>, const Direction&) const;

    // ============================================================
    // Cache Initialization
    // [MINECRAFT REF] BlockBehaviour.BlockStateBase.initCache()
    // ============================================================

    void BlockState::InitCache(World* world, const BlockPos& pos) const
    {
        // Initialize FluidState cache
        // [MINECRAFT REF] this.fluidState = ((Block)this.owner).getFluidState(this.asState());
        if (!m_fluidStateCached)
        {
            m_fluidState       = m_owner ? m_owner->GetFluidState(const_cast<BlockState*>(this)) : FluidState::Empty();
            m_fluidStateCached = true;
        }

        // Initialize light cache
        InitializeLightCache(world, pos);
    }

    // ============================================================
    // Light cache methods implementation
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
        if (m_owner)
        {
            return m_owner->GetLightBlock(const_cast<BlockState*>(this), world, pos);
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
        if (m_owner)
        {
            return m_owner->PropagatesSkylightDown(const_cast<BlockState*>(this), world, pos);
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
        if (m_owner)
        {
            return m_owner->GetLightEmission(const_cast<BlockState*>(this));
        }

        // Default: no light emission
        return 0;
    }

    void BlockState::InitializeLightCache(World* world, const BlockPos& pos) const
    {
        // [MINECRAFT REF] BlockBehaviour.java:1246-1247
        // Cache is populated during block registration for O(1) access

        if (!m_owner)
        {
            // No block type, use defaults
            m_lightCache.lightBlock             = 15;
            m_lightCache.lightEmission          = 0;
            m_lightCache.propagatesSkylightDown = false;
            m_lightCache.isValid                = true;
            return;
        }

        // Compute and cache all light properties
        m_lightCache.propagatesSkylightDown = m_owner->PropagatesSkylightDown(
            const_cast<BlockState*>(this), world, pos);

        m_lightCache.lightBlock = static_cast<int8_t>(m_owner->GetLightBlock(
            const_cast<BlockState*>(this), world, pos));

        m_lightCache.lightEmission = static_cast<int8_t>(m_owner->GetLightEmission(
            const_cast<BlockState*>(this)));

        m_lightCache.isValid = true;
    }
}
