#include "BlockAir.hpp"
using namespace enigma::voxel;

BlockAir::BlockAir(const std::string& registryName = "enigma", const std::string& namespaceName = "air") : Block(registryName, namespaceName)
{
}

BlockAir::~BlockAir()
{
}

const std::string& BlockAir::GetRegistryName() const
{
    return Block::GetRegistryName();
}

const std::string& BlockAir::GetNamespace() const
{
    return Block::GetNamespace();
}

void BlockAir::OnPlaced(enigma::voxel::World* world, const enigma::voxel::BlockPos& pos, enigma::voxel::BlockState* state)
{
    Block::OnPlaced(world, pos, state);
}

void BlockAir::OnBroken(enigma::voxel::World* world, const enigma::voxel::BlockPos& pos, enigma::voxel::BlockState* state)
{
    Block::OnBroken(world, pos, state);
}

void BlockAir::OnNeighborChanged(enigma::voxel::World* world, const enigma::voxel::BlockPos& pos, enigma::voxel::BlockState* state, Block* neighborBlock)
{
    Block::OnNeighborChanged(world, pos, state, neighborBlock);
}

std::string BlockAir::GetModelPath(enigma::voxel::BlockState* state) const
{
    return Block::GetModelPath(state);
}

void BlockAir::InitializeState(enigma::voxel::BlockState* state, const PropertyMap& properties)
{
    Block::InitializeState(state, properties);
}
