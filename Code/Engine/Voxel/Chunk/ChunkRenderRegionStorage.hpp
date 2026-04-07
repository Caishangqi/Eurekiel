#pragma once

#include <cstdint>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ChunkBatchTypes.hpp"

namespace enigma::graphic
{
    struct TerrainVertex;
}

namespace enigma::voxel
{
    class Chunk;
    class World;
    struct ChunkBatchRegionBuildOutput;

    struct ChunkBatchChunkRuntimeSlice
    {
        ChunkBatchArenaAllocation vertexAllocation;
        ChunkBatchChunkLayerSlice opaque;
        ChunkBatchChunkLayerSlice cutout;
        ChunkBatchChunkLayerSlice translucent;
        AABB3                     worldBounds;
        bool                      hasWorldBounds = false;

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
    };

    struct ChunkRenderRegion
    {
        ChunkBatchRegionId                          id;
        ChunkBatchRegionGeometry                    geometry;
        std::unordered_set<int64_t> chunkKeys;
        std::unordered_map<int64_t, ChunkBatchChunkRuntimeSlice> chunkSlices;
        std::unordered_set<int64_t>               dirtyChunkKeys;
        bool                                      dirty = false;
        bool                                      buildFailed = false;

        bool HasResidentChunks() const
        {
            return !chunkKeys.empty();
        }

        bool HasValidBatchGeometry() const
        {
            return geometry.gpuDataValid &&
                geometry.vertexBuffer != nullptr &&
                geometry.indexBuffer != nullptr &&
                geometry.HasValidArenaAllocations() &&
                !geometry.IsEmpty();
        }
    };

    struct ChunkBatchVertexArenaState
    {
        std::shared_ptr<graphic::D12VertexBuffer> buffer;
        uint32_t                                  capacityVertices = 0;
        std::vector<ChunkBatchArenaAllocation>    freeRanges;
    };

    struct ChunkBatchIndexArenaState
    {
        std::shared_ptr<graphic::D12IndexBuffer> buffer;
        uint32_t                                 capacityIndices = 0;
        std::vector<ChunkBatchArenaAllocation>   freeRanges;
    };

    class ChunkRenderRegionStorage
    {
    public:
        explicit ChunkRenderRegionStorage(World* world = nullptr);

        void         SetWorld(World* world);
        World*       GetWorld() { return m_world; }
        const World* GetWorld() const { return m_world; }

        void MarkChunkDirty(const IntVec2& chunkCoords);
        void NotifyChunkMeshReady(Chunk* chunk);
        void NotifyChunkUnloaded(const IntVec2& chunkCoords);

        uint32_t RebuildDirtyRegions(uint32_t maxRegionsPerFrame);
        uint32_t RebuildRegionsNow(const std::vector<ChunkBatchRegionId>& regionIds);

        ChunkRenderRegion*       GetRegion(const ChunkBatchRegionId& id);
        const ChunkRenderRegion* GetRegion(const ChunkBatchRegionId& id) const;

        const std::unordered_map<ChunkBatchRegionId, ChunkRenderRegion>& GetRegions() const { return m_regions; }
        uint32_t GetDirtyRegionCount() const { return static_cast<uint32_t>(m_dirtyRegionQueue.size()); }
        bool     HasDirtyRegions() const { return !m_dirtyRegionQueue.empty(); }
        uint32_t GetReplacementUploadCount() const { return m_replacementUploadCount; }
        uint32_t GetReplacementFallbackCount() const { return m_replacementFallbackCount; }
        uint32_t GetVertexArenaCapacity() const { return m_vertexArena.capacityVertices; }
        uint32_t GetVertexArenaRemainingCapacity() const;
        uint32_t GetIndexArenaCapacity() const { return m_indexArena.capacityIndices; }
        uint32_t GetIndexArenaRemainingCapacity() const;

    private:
        ChunkRenderRegion& EnsureRegion(const ChunkBatchRegionId& regionId);
        void               EnqueueDirtyRegion(const ChunkBatchRegionId& regionId);
        void               RemoveQueuedDirtyRegion(const ChunkBatchRegionId& regionId);
        void               ClearRegionGeometry(ChunkRenderRegion& region, bool buildFailed);
        void               ReleaseRegionArenaAllocations(ChunkRenderRegion& region);
        bool               CommitBuildOutput(ChunkRenderRegion& region, const ChunkBatchRegionBuildOutput& buildOutput);
        std::vector<const Chunk*> GatherRegionChunks(ChunkRenderRegion& region);
        bool               RebuildRegionInternal(const ChunkBatchRegionId& regionId);
        bool               TryApplyDirtyChunkReplacements(ChunkRenderRegion& region);
        bool               AllocateVertexArenaSlice(uint32_t vertexCount, ChunkBatchArenaAllocation& outAllocation);
        bool               AllocateIndexArenaSlice(uint32_t indexCount, ChunkBatchArenaAllocation& outAllocation);
        void               FreeVertexArenaSlice(const ChunkBatchArenaAllocation& allocation);
        void               FreeIndexArenaSlice(const ChunkBatchArenaAllocation& allocation);
        bool               EnsureVertexArenaCapacity(uint32_t minimumContiguousVertices);
        bool               EnsureIndexArenaCapacity(uint32_t minimumContiguousIndices);
        bool               UploadVertexArenaData(const ChunkBatchArenaAllocation& allocation, const graphic::TerrainVertex* vertices, uint32_t vertexCount) const;
        bool               UploadIndexArenaData(const ChunkBatchArenaAllocation& allocation, const std::vector<uint32_t>& indices) const;
        void               RefreshVertexArenaBindings();
        void               RefreshIndexArenaBindings();

    private:
        World*                                            m_world = nullptr;
        std::unordered_map<ChunkBatchRegionId, ChunkRenderRegion> m_regions;
        std::unordered_map<int64_t, ChunkBatchRegionId>   m_chunkToRegion;
        std::deque<ChunkBatchRegionId>                    m_dirtyRegionQueue;
        std::unordered_set<ChunkBatchRegionId>            m_dirtyRegionSet;
        ChunkBatchVertexArenaState                        m_vertexArena;
        ChunkBatchIndexArenaState                         m_indexArena;
        uint32_t                                          m_replacementUploadCount = 0;
        uint32_t                                          m_replacementFallbackCount = 0;
    };
}
