#pragma once

#include <cstdint>
#include <iterator>
#include <vector>

#include "ChunkBatchTypes.hpp"

namespace enigma::graphic
{
    class ICamera;
}

namespace enigma::voxel
{
    class World;

    struct ChunkBatchViewContext
    {
        World*                   world          = nullptr;
        const graphic::ICamera*  camera         = nullptr;
        const void*              visibilityHint = nullptr;
    };

    struct ChunkBatchCollection
    {
        std::vector<ChunkBatchDrawItem> batchItems;

        bool IsEmpty() const
        {
            return batchItems.empty();
        }

        void Append(ChunkBatchCollection&& other)
        {
            batchItems.insert(
                batchItems.end(),
                std::make_move_iterator(other.batchItems.begin()),
                std::make_move_iterator(other.batchItems.end()));
        }
    };

    class ChunkBatchCollector
    {
    public:
        ChunkBatchCollector()                                     = delete;
        ChunkBatchCollector(const ChunkBatchCollector&)           = delete;
        ChunkBatchCollector& operator=(const ChunkBatchCollector&) = delete;

        static ChunkBatchCollection Collect(
            const ChunkBatchViewContext& view,
            ChunkBatchLayer              layer);
    };
}
