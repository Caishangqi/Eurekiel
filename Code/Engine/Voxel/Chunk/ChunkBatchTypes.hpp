#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/Mat44.hpp"

namespace enigma::graphic
{
    class D12IndexBuffer;
    class D12VertexBuffer;
}

namespace enigma::voxel
{
    // Phase-1 chunk batching keeps the current engine coordinate system:
    // +X forward, +Y left, +Z up.
    constexpr int CHUNK_BATCH_REGION_SIZE_X = 4;
    constexpr int CHUNK_BATCH_REGION_SIZE_Y = 4;
    inline const IntVec2 CHUNK_BATCH_REGION_DIMENSIONS_IN_CHUNKS = IntVec2(CHUNK_BATCH_REGION_SIZE_X, CHUNK_BATCH_REGION_SIZE_Y);

    enum class ChunkBatchLayer : uint8_t
    {
        Opaque = 0,
        Cutout,
        Translucent
    };

    struct ChunkBatchRegionId
    {
        IntVec2 regionCoords = IntVec2::ZERO;

        bool operator==(const ChunkBatchRegionId& other) const
        {
            return regionCoords == other.regionCoords;
        }

        bool operator!=(const ChunkBatchRegionId& other) const
        {
            return !(*this == other);
        }
    };

    struct ChunkBatchLayerSpan
    {
        uint32_t startIndex = 0;
        uint32_t indexCount = 0;

        bool IsEmpty() const
        {
            return indexCount == 0;
        }
    };

    struct ChunkBatchChunkLayerSlice
    {
        uint32_t startIndex = 0;
        uint32_t indexCount = 0;
        uint32_t reservedIndexCount = 0;

        bool HasGeometry() const
        {
            return indexCount > 0;
        }

        bool HasReservedCapacity() const
        {
            return reservedIndexCount > 0;
        }

        uint32_t GetReservedEndIndex() const
        {
            return startIndex + reservedIndexCount;
        }
    };

    struct ChunkBatchArenaAllocation
    {
        uint32_t startElement = 0;
        uint32_t elementCount = 0;

        bool IsValid() const
        {
            return elementCount > 0;
        }

        void Reset()
        {
            startElement = 0;
            elementCount = 0;
        }
    };

    struct ChunkBatchRegionGeometry
    {
        std::shared_ptr<graphic::D12VertexBuffer> vertexBuffer;
        std::shared_ptr<graphic::D12IndexBuffer>  indexBuffer;
        ChunkBatchArenaAllocation                 vertexAllocation;
        ChunkBatchArenaAllocation                 indexAllocation;

        ChunkBatchLayerSpan opaque;
        ChunkBatchLayerSpan cutout;
        ChunkBatchLayerSpan translucent;

        Mat44 regionModelMatrix = Mat44::IDENTITY;
        AABB3 worldBounds;
        bool  gpuDataValid = false;

        bool IsEmpty() const
        {
            return opaque.IsEmpty() && cutout.IsEmpty() && translucent.IsEmpty();
        }

        bool HasValidArenaAllocations() const
        {
            return vertexAllocation.IsValid() && indexAllocation.IsValid();
        }

        ChunkBatchLayerSpan& GetSpanForLayer(ChunkBatchLayer layer)
        {
            switch (layer)
            {
            case ChunkBatchLayer::Opaque: return opaque;
            case ChunkBatchLayer::Cutout: return cutout;
            case ChunkBatchLayer::Translucent: return translucent;
            default: return opaque;
            }
        }

        const ChunkBatchLayerSpan& GetSpanForLayer(ChunkBatchLayer layer) const
        {
            switch (layer)
            {
            case ChunkBatchLayer::Opaque: return opaque;
            case ChunkBatchLayer::Cutout: return cutout;
            case ChunkBatchLayer::Translucent: return translucent;
            default: return opaque;
            }
        }
    };

    struct ChunkBatchDrawItem
    {
        ChunkBatchRegionId                regionId;
        const ChunkBatchRegionGeometry*   geometry = nullptr;
        uint32_t                          startIndex = 0;
        uint32_t                          indexCount = 0;

        bool IsValid() const
        {
            return geometry != nullptr &&
                geometry->gpuDataValid &&
                geometry->vertexBuffer != nullptr &&
                geometry->indexBuffer != nullptr &&
                geometry->HasValidArenaAllocations() &&
                indexCount > 0;
        }
    };

    struct ChunkBatchStats
    {
        uint32_t visibleRegions = 0;
        uint32_t culledRegions = 0;
        uint32_t shadowVisibleRegions = 0;
        uint32_t shadowCulledRegions = 0;
        uint32_t visibleChunks = 0;
        uint32_t batchedDraws = 0;
        uint32_t dirtyRegionRebuilds = 0;

        void ResetFrameCounters()
        {
            visibleRegions = 0;
            culledRegions = 0;
            shadowVisibleRegions = 0;
            shadowCulledRegions = 0;
            visibleChunks = 0;
            batchedDraws = 0;
            dirtyRegionRebuilds = 0;
        }
    };

    constexpr int FloorDivideToRegionCoord(int chunkCoord, int regionSizeInChunks)
    {
        return (chunkCoord >= 0) ?
            (chunkCoord / regionSizeInChunks) :
            -(((-chunkCoord) + regionSizeInChunks - 1) / regionSizeInChunks);
    }

    inline ChunkBatchRegionId GetChunkBatchRegionIdForChunk(const IntVec2& chunkCoords)
    {
        return ChunkBatchRegionId{
            IntVec2(
                FloorDivideToRegionCoord(chunkCoords.x, CHUNK_BATCH_REGION_SIZE_X),
                FloorDivideToRegionCoord(chunkCoords.y, CHUNK_BATCH_REGION_SIZE_Y))
        };
    }

    inline IntVec2 GetChunkBatchRegionMinChunkCoords(const ChunkBatchRegionId& regionId)
    {
        return IntVec2(
            regionId.regionCoords.x * CHUNK_BATCH_REGION_SIZE_X,
            regionId.regionCoords.y * CHUNK_BATCH_REGION_SIZE_Y);
    }

    inline IntVec2 GetChunkBatchRegionMaxChunkCoords(const ChunkBatchRegionId& regionId)
    {
        const IntVec2 mins = GetChunkBatchRegionMinChunkCoords(regionId);
        return IntVec2(
            mins.x + CHUNK_BATCH_REGION_SIZE_X - 1,
            mins.y + CHUNK_BATCH_REGION_SIZE_Y - 1);
    }
}

namespace std
{
    template <>
    struct hash<enigma::voxel::ChunkBatchRegionId>
    {
        size_t operator()(const enigma::voxel::ChunkBatchRegionId& value) const
        {
            return std::hash<IntVec2>{}(value.regionCoords);
        }
    };
}
