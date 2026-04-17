#pragma once

#include "../Chunk/MeshBuild/ChunkMeshNeighborReadiness.hpp"

#include <cstddef>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace enigma::voxel
{
    class ChunkMeshNeighborWaitRegistry
    {
    public:
        void RegisterWait(const ChunkMeshNeighborWaitEntry& entry);
        bool CancelWait(IntVec2 targetChunkCoords, uint64_t targetChunkInstanceId, uint64_t buildVersion);
        bool CancelWaitForTarget(IntVec2 targetChunkCoords);

        const ChunkMeshNeighborWaitEntry* FindWaitEntry(IntVec2 targetChunkCoords) const;
        bool HasWaitEntry(IntVec2 targetChunkCoords) const;

        std::vector<ChunkMeshNeighborWaitEntry> CollectWakeEntries(IntVec2 readableNeighborCoords) const;
        void PruneInvalidEntries(const std::function<bool(const ChunkMeshNeighborWaitEntry&)>& shouldPrune);
        void Clear();

        size_t GetEntryCount() const noexcept
        {
            return m_entriesByTarget.size();
        }

    private:
        using PackedChunkCoords = int64_t;

        static PackedChunkCoords PackChunkCoords(IntVec2 chunkCoords);
        static IntVec2           GetNeighborChunkCoords(IntVec2 chunkCoords, ChunkMeshHorizontalNeighbor neighbor);

        void AddReverseDependencies(const ChunkMeshNeighborWaitEntry& entry);
        void RemoveReverseDependencies(const ChunkMeshNeighborWaitEntry& entry);
        bool ErasePackedEntry(PackedChunkCoords packedTargetCoords);

        std::unordered_map<PackedChunkCoords, ChunkMeshNeighborWaitEntry>          m_entriesByTarget;
        std::unordered_map<PackedChunkCoords, std::unordered_set<PackedChunkCoords>> m_targetsByNeighbor;
    };
}
