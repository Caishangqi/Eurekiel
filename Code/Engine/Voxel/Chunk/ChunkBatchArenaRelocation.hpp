#pragma once

#include <vector>

#include "ChunkBatchTypes.hpp"

namespace enigma::voxel
{
    class ChunkBatchArenaRelocationPlanner
    {
    public:
        ChunkBatchArenaRelocationPlanner() = delete;

        static ChunkBatchArenaRelocationPlan BuildPlanFromFreeRanges(
            ChunkBatchArenaKind                          arenaKind,
            uint32_t                                     currentCapacity,
            uint32_t                                     minimumRequestedCapacity,
            uint32_t                                     defaultCapacity,
            const std::vector<ChunkBatchArenaAllocation>& freeRanges);

        static std::vector<ChunkBatchArenaCopySpan> BuildCopySpans(
            uint32_t                                     currentCapacity,
            const std::vector<ChunkBatchArenaAllocation>& freeRanges);

        static ChunkBatchArenaAllocation ComputeRelocatedAllocation(
            const ChunkBatchArenaRelocationPlan& relocationPlan,
            const ChunkBatchArenaAllocation&     allocation);
    };
}
