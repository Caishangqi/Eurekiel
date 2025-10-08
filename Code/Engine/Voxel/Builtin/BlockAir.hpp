#pragma once
#include "Engine/Registry/Block/Block.hpp"

namespace enigma::voxel
{
    class BlockAir : public registry::block::Block
    {
    public:
        BlockAir(const std::string& registryName, const std::string& namespaceName);
        ~BlockAir() override;
        const std::string& GetRegistryName() const override;
        const std::string& GetNamespace() const override;
        void               OnPlaced(enigma::voxel::World* world, const enigma::voxel::BlockPos& pos, enigma::voxel::BlockState* state) override;
        void               OnBroken(enigma::voxel::World* world, const enigma::voxel::BlockPos& pos, enigma::voxel::BlockState* state) override;
        void               OnNeighborChanged(enigma::voxel::World* world, const enigma::voxel::BlockPos& pos, enigma::voxel::BlockState* state, Block* neighborBlock) override;
        std::string        GetModelPath(enigma::voxel::BlockState* state) const override;

    protected:
        void InitializeState(enigma::voxel::BlockState* state, const PropertyMap& properties) override;
    };
}
