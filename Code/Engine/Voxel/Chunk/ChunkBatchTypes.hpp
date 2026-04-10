#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "Engine/Core/EngineCommon.hpp"
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

    enum class ChunkBatchArenaKind : uint8_t
    {
        Vertex = 0,
        Index
    };

    enum class ChunkBatchArenaFallbackReason : uint8_t
    {
        None = 0,
        ReplacementStateInvalid,
        ReplacementVertexOverflow,
        ReplacementIndexOverflow,
        ReplacementVertexUploadFailed,
        ReplacementIndexUploadFailed,
        VertexArenaGrowFailed,
        IndexArenaGrowFailed,
        VertexArenaUploadFailed,
        IndexArenaUploadFailed
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

        uint32_t GetDrawEndIndex() const
        {
            return startIndex + indexCount;
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

    struct ChunkBatchArenaCopySpan
    {
        ChunkBatchArenaAllocation sourceAllocation;
        ChunkBatchArenaAllocation destinationAllocation;

        bool IsValid() const
        {
            return sourceAllocation.IsValid() &&
                destinationAllocation.IsValid() &&
                sourceAllocation.elementCount == destinationAllocation.elementCount;
        }
    };

    struct ChunkBatchArenaRelocationPlan
    {
        ChunkBatchArenaKind                    arenaKind = ChunkBatchArenaKind::Vertex;
        uint32_t                               oldCapacity = 0;
        uint32_t                               newCapacity = 0;
        uint32_t                               minimumRequestedCapacity = 0;
        uint32_t                               relocatedElementCount = 0;
        std::vector<ChunkBatchArenaCopySpan>   copySpans;

        bool HasCopySpans() const
        {
            return !copySpans.empty();
        }

        bool IsGrowRequired() const
        {
            return newCapacity > oldCapacity;
        }
    };

    struct ChunkBatchArenaSideDiagnostics
    {
        uint32_t growCountLifetime = 0;
        uint32_t relocationCountLifetime = 0;
        uint32_t relocatedElementCountLifetime = 0;
        uint32_t lastRelocationElementCount = 0;
        uint32_t lastRelocationSpanCount = 0;
        uint32_t lastRequestedCapacity = 0;
        uint32_t lastCapacityBeforeGrow = 0;
        uint32_t lastCapacityAfterGrow = 0;
    };

    struct ChunkBatchArenaFallbackDiagnostics
    {
        uint32_t totalCountLifetime = 0;
        uint32_t replacementStateInvalidCountLifetime = 0;
        uint32_t replacementVertexOverflowCountLifetime = 0;
        uint32_t replacementIndexOverflowCountLifetime = 0;
        uint32_t replacementVertexUploadFailureCountLifetime = 0;
        uint32_t replacementIndexUploadFailureCountLifetime = 0;
        uint32_t vertexArenaGrowFailureCountLifetime = 0;
        uint32_t indexArenaGrowFailureCountLifetime = 0;
        uint32_t vertexArenaUploadFailureCountLifetime = 0;
        uint32_t indexArenaUploadFailureCountLifetime = 0;
        ChunkBatchArenaFallbackReason lastReason = ChunkBatchArenaFallbackReason::None;
    };

    struct ChunkBatchArenaDiagnostics
    {
        ChunkBatchArenaSideDiagnostics     vertex;
        ChunkBatchArenaSideDiagnostics     index;
        ChunkBatchArenaFallbackDiagnostics fallback;
    };

    struct ChunkBatchSubDraw
    {
        uint32_t startIndex = 0;
        uint32_t indexCount = 0;

        bool IsValid() const
        {
            return indexCount > 0;
        }

        uint32_t GetEndIndex() const
        {
            return startIndex + indexCount;
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
        std::vector<ChunkBatchSubDraw> opaqueSubDraws;
        std::vector<ChunkBatchSubDraw> cutoutSubDraws;
        std::vector<ChunkBatchSubDraw> translucentSubDraws;

        Mat44 regionModelMatrix = Mat44::IDENTITY;
        AABB3 worldBounds;
        bool  gpuDataValid = false;

        bool IsEmpty() const
        {
            return opaqueSubDraws.empty() && cutoutSubDraws.empty() && translucentSubDraws.empty();
        }

        bool HasValidArenaAllocations() const
        {
            return vertexAllocation.IsValid() && indexAllocation.IsValid();
        }

        bool HasValidDirectPreciseBuffers() const
        {
            return gpuDataValid &&
                vertexBuffer != nullptr &&
                indexBuffer != nullptr &&
                HasValidArenaAllocations();
        }

        bool SupportsDirectPreciseLayer(ChunkBatchLayer layer) const
        {
            return HasValidDirectPreciseBuffers() &&
                !GetSubDrawsForLayer(layer).empty();
        }

        uint32_t ResolveDirectPreciseStartIndex(const ChunkBatchSubDraw& subDraw) const
        {
            if (!HasValidDirectPreciseBuffers() || !subDraw.IsValid())
            {
                ERROR_AND_DIE("ChunkBatchRegionGeometry: Cannot resolve direct precise start index from invalid region geometry");
            }

            return indexAllocation.startElement + subDraw.startIndex;
        }

        int32_t ResolveDirectPreciseBaseVertex() const
        {
            if (!HasValidDirectPreciseBuffers())
            {
                ERROR_AND_DIE("ChunkBatchRegionGeometry: Cannot resolve direct precise base vertex from invalid region geometry");
            }

            return static_cast<int32_t>(vertexAllocation.startElement);
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

        std::vector<ChunkBatchSubDraw>& GetSubDrawsForLayer(ChunkBatchLayer layer)
        {
            switch (layer)
            {
            case ChunkBatchLayer::Opaque: return opaqueSubDraws;
            case ChunkBatchLayer::Cutout: return cutoutSubDraws;
            case ChunkBatchLayer::Translucent: return translucentSubDraws;
            default: return opaqueSubDraws;
            }
        }

        const std::vector<ChunkBatchSubDraw>& GetSubDrawsForLayer(ChunkBatchLayer layer) const
        {
            switch (layer)
            {
            case ChunkBatchLayer::Opaque: return opaqueSubDraws;
            case ChunkBatchLayer::Cutout: return cutoutSubDraws;
            case ChunkBatchLayer::Translucent: return translucentSubDraws;
            default: return opaqueSubDraws;
            }
        }
    };

    struct ChunkBatchDrawItem
    {
        ChunkBatchRegionId                regionId;
        const ChunkBatchRegionGeometry*   geometry = nullptr;
        const ChunkBatchSubDraw*          subDraw = nullptr;

        bool IsValid() const
        {
            return geometry != nullptr &&
                subDraw != nullptr &&
                subDraw->IsValid() &&
                geometry->HasValidDirectPreciseBuffers();
        }

        uint32_t ResolveDirectPreciseStartIndex() const
        {
            if (!IsValid())
            {
                ERROR_AND_DIE("ChunkBatchDrawItem: Cannot resolve direct precise start index from an invalid batch item");
            }

            return geometry->ResolveDirectPreciseStartIndex(*subDraw);
        }

        int32_t ResolveDirectPreciseBaseVertex() const
        {
            if (!IsValid())
            {
                ERROR_AND_DIE("ChunkBatchDrawItem: Cannot resolve direct precise base vertex from an invalid batch item");
            }

            return geometry->ResolveDirectPreciseBaseVertex();
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
