#pragma once

#include <cstdint>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ChunkBatchRegionBuilder.hpp"
#include "ChunkBatchTypes.hpp"

namespace enigma::voxel
{
    class Chunk;
    class World;

    struct ChunkRenderRegion
    {
        ChunkBatchRegionId         id;
        ChunkBatchRegionGeometry   geometry;
        std::unordered_set<int64_t> chunkKeys;
        bool                      dirty = false;
        bool                      buildFailed = false;

        bool HasResidentChunks() const
        {
            return !chunkKeys.empty();
        }

        bool HasValidBatchGeometry() const
        {
            return geometry.gpuDataValid &&
                geometry.vertexBuffer != nullptr &&
                geometry.indexBuffer != nullptr &&
                !geometry.IsEmpty();
        }
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

    private:
        ChunkRenderRegion& EnsureRegion(const ChunkBatchRegionId& regionId);
        void               EnqueueDirtyRegion(const ChunkBatchRegionId& regionId);
        void               RemoveQueuedDirtyRegion(const ChunkBatchRegionId& regionId);
        void               ClearRegionGeometry(ChunkRenderRegion& region, bool buildFailed);
        bool               CommitBuildOutput(ChunkRenderRegion& region, const ChunkBatchRegionBuildOutput& buildOutput);
        std::vector<const Chunk*> GatherRegionChunks(ChunkRenderRegion& region);
        bool               RebuildRegionInternal(const ChunkBatchRegionId& regionId);

    private:
        World*                                            m_world = nullptr;
        std::unordered_map<ChunkBatchRegionId, ChunkRenderRegion> m_regions;
        std::unordered_map<int64_t, ChunkBatchRegionId>   m_chunkToRegion;
        std::deque<ChunkBatchRegionId>                    m_dirtyRegionQueue;
        std::unordered_set<ChunkBatchRegionId>            m_dirtyRegionSet;
    };
}
