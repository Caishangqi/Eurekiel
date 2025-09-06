#include "BlockState.hpp"
#include "../../Registry/Block/Block.hpp"
#include "../../Renderer/Model/BlockRenderMesh.hpp"
#include "../../Model/ModelSubsystem.hpp"
#include "../../Core/EngineCommon.hpp"
#include "../../Registry/Block/BlockRegistry.hpp"
#include "../../Resource/BlockState/BlockStateDefinition.hpp"

namespace enigma::voxel::block
{
    template <typename T>
    BlockState* BlockState::With(std::shared_ptr<Property<T>> property, const T& value) const
    {
        // Get the new property map
        PropertyMap newProperties = m_properties.With(property, value);

        // Find the corresponding state in the block's state list
        return m_blockType->GetState(newProperties);
    }

    bool BlockState::IsOpaque() const
    {
        return m_blockType ? m_blockType->IsOpaque() : true;
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
            // Get the BlockStateDefinition from the registry to access compiled mesh
            if (m_blockType)
            {
                std::string blockName            = m_blockType->GetNamespace() + ":" + m_blockType->GetRegistryName();
                auto        blockStateDefinition = enigma::registry::block::BlockRegistry::GetBlockStateDefinition(blockName);

                if (blockStateDefinition)
                {
                    // Convert our property map to string for variant lookup
                    std::string propertyString = "";
                    if (!m_properties.Empty())
                    {
                        std::string fullString = m_properties.ToString();
                        // Remove braces {} from the string
                        if (fullString.size() >= 2 && fullString.front() == '{' && fullString.back() == '}')
                        {
                            propertyString = fullString.substr(1, fullString.size() - 2);
                        }
                        else
                        {
                            propertyString = fullString;
                        }
                    }

                    // Get the variants for this property combination
                    const auto* variants = blockStateDefinition->GetVariants(propertyString);
                    if (variants && !variants->empty())
                    {
                        // Use the first variant (in the future, we might support weighted random selection)
                        const auto& variant = variants->front();
                        m_cachedMesh        = variant.compiledMesh;
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

    void BlockState::OnPlaced(enigma::voxel::world::World* world, const BlockPos& pos) const
    {
        if (m_blockType)
        {
            m_blockType->OnPlaced(world, pos, const_cast<BlockState*>(this));
        }
    }

    void BlockState::OnBroken(enigma::voxel::world::World* world, const BlockPos& pos) const
    {
        if (m_blockType)
        {
            m_blockType->OnBroken(world, pos, const_cast<BlockState*>(this));
        }
    }

    void BlockState::OnNeighborChanged(enigma::voxel::world::World* world, const BlockPos& pos, enigma::registry::block::Block* neighborBlock) const
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
}
