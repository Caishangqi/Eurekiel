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

    struct ChunkBatchChunkBuildOutput
    {
        IntVec2                               chunkCoords = IntVec2::ZERO;
        ChunkBatchArenaAllocation             vertexAllocation;
        std::vector<graphic::TerrainVertex>   vertices;
        std::vector<uint32_t>                 opaqueIndices;
        std::vector<uint32_t>                 cutoutIndices;
        std::vector<uint32_t>                 translucentIndices;
        ChunkBatchChunkLayerSlice             opaque;
        ChunkBatchChunkLayerSlice             cutout;
        ChunkBatchChunkLayerSlice             translucent;
        AABB3                                 worldBounds;
        bool                                  hasWorldBounds = false;

        bool HasGeometry() const
        {
            return !vertices.empty();
        }

        ChunkBatchChunkLayerSlice& GetLayerSlice(ChunkBatchLayer layer)
        {
            switch (layer)
            {
            case ChunkBatchLayer::Opaque: return opaque;
            case ChunkBatchLayer::Cutout: return cutout;
            case ChunkBatchLayer::Translucent: return translucent;
            default: return opaque;
            }
        }

        const ChunkBatchChunkLayerSlice& GetLayerSlice(ChunkBatchLayer layer) const
        {
            switch (layer)
            {
            case ChunkBatchLayer::Opaque: return opaque;
            case ChunkBatchLayer::Cutout: return cutout;
            case ChunkBatchLayer::Translucent: return translucent;
            default: return opaque;
            }
        }

        std::vector<uint32_t>& GetIndicesForLayer(ChunkBatchLayer layer)
        {
            switch (layer)
            {
            case ChunkBatchLayer::Opaque: return opaqueIndices;
            case ChunkBatchLayer::Cutout: return cutoutIndices;
            case ChunkBatchLayer::Translucent: return translucentIndices;
            default: return opaqueIndices;
            }
        }

        const std::vector<uint32_t>& GetIndicesForLayer(ChunkBatchLayer layer) const
        {
            switch (layer)
            {
            case ChunkBatchLayer::Opaque: return opaqueIndices;
            case ChunkBatchLayer::Cutout: return cutoutIndices;
            case ChunkBatchLayer::Translucent: return translucentIndices;
            default: return opaqueIndices;
            }
        }
    };

    struct ChunkBatchRegionBuildOutput
    {
        ChunkBatchRegionGeometry           geometry;
        std::vector<graphic::TerrainVertex> vertices;
        std::vector<uint32_t>               indices;
        std::vector<ChunkBatchChunkBuildOutput> chunkOutputs;
        uint32_t                            totalVertexCapacity = 0;
        uint32_t                            totalIndexCapacity = 0;

        bool HasCpuGeometry() const
        {
            return totalVertexCapacity > 0u && totalIndexCapacity > 0u;
        }
    };

    class ChunkBatchRegionBuilder
    {
    public:
        ChunkBatchRegionBuilder() = delete;

        static ChunkBatchChunkBuildOutput  BuildChunkGeometry(const Chunk& chunk, const ChunkBatchRegionId& regionId);
        static ChunkBatchRegionBuildOutput BuildRegionGeometry(const ChunkBatchRegionBuildInput& input);
    };
}
