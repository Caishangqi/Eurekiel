#pragma once

#include <vector>

#include "../World/TerrainVertexLayout.hpp"
#include "ChunkBatchTypes.hpp"

namespace enigma::voxel
{
    class Chunk;

    struct ChunkBatchRegionBuildInput
    {
        ChunkBatchRegionId        regionId;
        std::vector<const Chunk*> chunks;
    };

    struct ChunkBatchRegionBuildOutput
    {
        ChunkBatchRegionGeometry           geometry;
        std::vector<graphic::TerrainVertex> vertices;
        std::vector<uint32_t>               indices;

        bool HasCpuGeometry() const
        {
            return !vertices.empty() && !indices.empty();
        }
    };

    class ChunkBatchRegionBuilder
    {
    public:
        ChunkBatchRegionBuilder() = delete;

        static ChunkBatchRegionBuildOutput BuildRegionGeometry(const ChunkBatchRegionBuildInput& input);
    };
}
