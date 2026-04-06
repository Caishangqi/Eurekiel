#pragma once

#include <cstdint>

#include "ChunkBatchCollector.hpp"

namespace enigma::voxel
{
    class ChunkBatchRenderer
    {
    public:
        ChunkBatchRenderer()                                    = delete;
        ChunkBatchRenderer(const ChunkBatchRenderer&)           = delete;
        ChunkBatchRenderer& operator=(const ChunkBatchRenderer&) = delete;

        static uint32_t Submit(const ChunkBatchCollection& collection);

        static void SubmitDirect(const ChunkBatchDrawItem& item);
    };
}
