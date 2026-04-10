#include "ChunkBatchArenaRelocation.hpp"

#include <algorithm>

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/StringUtils.hpp"

namespace
{
    using enigma::voxel::ChunkBatchArenaAllocation;
    using enigma::voxel::ChunkBatchArenaCopySpan;
    using enigma::voxel::ChunkBatchArenaKind;
    using enigma::voxel::ChunkBatchArenaRelocationPlan;
    using enigma::voxel::ChunkBatchArenaRelocationPlanner;

    uint32_t RoundUpToPowerOfTwo(uint32_t value)
    {
        if (value <= 1u)
        {
            return 1u;
        }

        uint32_t rounded = 1u;
        while (rounded < value)
        {
            rounded <<= 1u;
        }

        return rounded;
    }

    uint32_t ComputeExpandedCapacity(uint32_t currentCapacity, uint32_t requestedCount, uint32_t defaultCapacity)
    {
        if (currentCapacity == 0u)
        {
            return RoundUpToPowerOfTwo((std::max)(defaultCapacity, requestedCount));
        }

        uint64_t newCapacity = currentCapacity;
        while ((newCapacity - currentCapacity) < requestedCount)
        {
            newCapacity *= 2u;
        }

        return static_cast<uint32_t>(newCapacity);
    }

    std::vector<ChunkBatchArenaAllocation> BuildNormalizedFreeRanges(
        uint32_t                                     currentCapacity,
        const std::vector<ChunkBatchArenaAllocation>& freeRanges)
    {
        std::vector<ChunkBatchArenaAllocation> normalizedRanges;
        normalizedRanges.reserve(freeRanges.size());

        for (const ChunkBatchArenaAllocation& freeRange : freeRanges)
        {
            if (!freeRange.IsValid() || freeRange.startElement >= currentCapacity)
            {
                continue;
            }

            const uint32_t clampedEnd = (std::min)(currentCapacity, freeRange.startElement + freeRange.elementCount);
            if (clampedEnd <= freeRange.startElement)
            {
                continue;
            }

            normalizedRanges.push_back(ChunkBatchArenaAllocation{
                freeRange.startElement,
                clampedEnd - freeRange.startElement
            });
        }

        std::sort(
            normalizedRanges.begin(),
            normalizedRanges.end(),
            [](const ChunkBatchArenaAllocation& lhs, const ChunkBatchArenaAllocation& rhs)
            {
                return lhs.startElement < rhs.startElement;
            });

        std::vector<ChunkBatchArenaAllocation> mergedRanges;
        mergedRanges.reserve(normalizedRanges.size());

        for (const ChunkBatchArenaAllocation& freeRange : normalizedRanges)
        {
            if (mergedRanges.empty())
            {
                mergedRanges.push_back(freeRange);
                continue;
            }

            ChunkBatchArenaAllocation& mergedRange = mergedRanges.back();
            const uint32_t mergedRangeEnd = mergedRange.startElement + mergedRange.elementCount;
            if (mergedRangeEnd >= freeRange.startElement)
            {
                const uint32_t freeRangeEnd = freeRange.startElement + freeRange.elementCount;
                mergedRange.elementCount = (std::max)(mergedRangeEnd, freeRangeEnd) - mergedRange.startElement;
                continue;
            }

            mergedRanges.push_back(freeRange);
        }

        return mergedRanges;
    }
}

namespace enigma::voxel
{
    ChunkBatchArenaRelocationPlan ChunkBatchArenaRelocationPlanner::BuildPlanFromFreeRanges(
        ChunkBatchArenaKind                          arenaKind,
        uint32_t                                     currentCapacity,
        uint32_t                                     minimumRequestedCapacity,
        uint32_t                                     defaultCapacity,
        const std::vector<ChunkBatchArenaAllocation>& freeRanges)
    {
        ChunkBatchArenaRelocationPlan relocationPlan;
        relocationPlan.arenaKind = arenaKind;
        relocationPlan.oldCapacity = currentCapacity;
        relocationPlan.newCapacity = ComputeExpandedCapacity(currentCapacity, minimumRequestedCapacity, defaultCapacity);
        relocationPlan.minimumRequestedCapacity = minimumRequestedCapacity;
        relocationPlan.copySpans = BuildCopySpans(currentCapacity, freeRanges);

        for (const ChunkBatchArenaCopySpan& copySpan : relocationPlan.copySpans)
        {
            relocationPlan.relocatedElementCount += copySpan.destinationAllocation.elementCount;
        }

        return relocationPlan;
    }

    std::vector<ChunkBatchArenaCopySpan> ChunkBatchArenaRelocationPlanner::BuildCopySpans(
        uint32_t                                     currentCapacity,
        const std::vector<ChunkBatchArenaAllocation>& freeRanges)
    {
        std::vector<ChunkBatchArenaCopySpan> copySpans;
        if (currentCapacity == 0u)
        {
            return copySpans;
        }

        const std::vector<ChunkBatchArenaAllocation> normalizedFreeRanges = BuildNormalizedFreeRanges(currentCapacity, freeRanges);
        uint32_t sourceCursor = 0u;
        uint32_t destinationCursor = 0u;

        for (const ChunkBatchArenaAllocation& freeRange : normalizedFreeRanges)
        {
            if (freeRange.startElement > sourceCursor)
            {
                const uint32_t usedCount = freeRange.startElement - sourceCursor;
                copySpans.push_back(ChunkBatchArenaCopySpan{
                    ChunkBatchArenaAllocation{sourceCursor, usedCount},
                    ChunkBatchArenaAllocation{destinationCursor, usedCount}
                });
                destinationCursor += usedCount;
            }

            sourceCursor = (std::max)(sourceCursor, freeRange.startElement + freeRange.elementCount);
        }

        if (sourceCursor < currentCapacity)
        {
            const uint32_t usedCount = currentCapacity - sourceCursor;
            copySpans.push_back(ChunkBatchArenaCopySpan{
                ChunkBatchArenaAllocation{sourceCursor, usedCount},
                ChunkBatchArenaAllocation{destinationCursor, usedCount}
            });
        }

        return copySpans;
    }

    ChunkBatchArenaAllocation ChunkBatchArenaRelocationPlanner::ComputeRelocatedAllocation(
        const ChunkBatchArenaRelocationPlan& relocationPlan,
        const ChunkBatchArenaAllocation&     allocation)
    {
        if (!allocation.IsValid())
        {
            return {};
        }

        const uint32_t allocationEnd = allocation.startElement + allocation.elementCount;
        for (const ChunkBatchArenaCopySpan& copySpan : relocationPlan.copySpans)
        {
            if (!copySpan.IsValid())
            {
                continue;
            }

            const uint32_t sourceStart = copySpan.sourceAllocation.startElement;
            const uint32_t sourceEnd = sourceStart + copySpan.sourceAllocation.elementCount;
            if (allocation.startElement < sourceStart || allocationEnd > sourceEnd)
            {
                continue;
            }

            return ChunkBatchArenaAllocation{
                copySpan.destinationAllocation.startElement + (allocation.startElement - sourceStart),
                allocation.elementCount
            };
        }

        ERROR_AND_DIE(Stringf(
            "ChunkBatchArenaRelocationPlanner: Allocation [%u, %u) is not covered by the relocation plan",
            allocation.startElement,
            allocationEnd));
    }
}
