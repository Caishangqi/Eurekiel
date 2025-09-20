#pragma once
#include <memory>

namespace enigma::registry::block
{
    class Block;
}

namespace enigma::voxel::block
{
    inline std::shared_ptr<registry::block::Block> AIR = nullptr;
}
