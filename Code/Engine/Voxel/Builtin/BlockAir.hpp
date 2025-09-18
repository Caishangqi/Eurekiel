#pragma once
#include "Engine/Registry/Block/Block.hpp"

namespace enigma::voxel::block
{
    class BlockAir : public registry::block::Block
    {
    public:
        BlockAir(const std::string& registryName, const std::string& namespaceName);
        ~BlockAir() override;
        const std::string& GetRegistryName() const override;
        const std::string& GetNamespace() const override;
        void               OnPlaced(enigma::voxel::world::World* world, const enigma::voxel::block::BlockPos& pos, enigma::voxel::block::BlockState* state) override;
        void               OnBroken(enigma::voxel::world::World* world, const enigma::voxel::block::BlockPos& pos, enigma::voxel::block::BlockState* state) override;
        void               OnNeighborChanged(enigma::voxel::world::World* world, const enigma::voxel::block::BlockPos& pos, enigma::voxel::block::BlockState* state, Block* neighborBlock) override;
        std::string        GetModelPath(enigma::voxel::block::BlockState* state) const override;

    protected:
        void InitializeState(enigma::voxel::block::BlockState* state, const property::PropertyMap& properties) override;
    };
}
