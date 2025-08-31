#include "BlockState.hpp"
#include "../../Registry/Block/Block.hpp"
#include "../../Renderer/Model/RenderMesh.hpp"

namespace enigma::voxel::block
{
    template<typename T>
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
            // TODO: Load and compile the mesh from the model resource
            // This will be implemented when we have the ModelCompiler
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
