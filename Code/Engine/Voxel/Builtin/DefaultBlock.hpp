#pragma once
#include <memory>

namespace enigma::registry::block
{
    class Block;
}

namespace enigma::voxel
{
    inline std::shared_ptr<registry::block::Block> AIR = nullptr;
}
