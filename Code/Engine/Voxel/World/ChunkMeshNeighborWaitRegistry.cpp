#include "ChunkMeshNeighborWaitRegistry.hpp"

#include "../Chunk/ChunkHelper.hpp"

using namespace enigma::voxel;

namespace
{
    bool MatchesWaitIdentity(const ChunkMeshNeighborWaitEntry& entry,
                             uint64_t targetChunkInstanceId,
                             uint64_t buildVersion) noexcept
    {
        return entry.targetChunkInstanceId == targetChunkInstanceId &&
               entry.buildVersion == buildVersion;
    }
}

ChunkMeshNeighborWaitRegistry::PackedChunkCoords ChunkMeshNeighborWaitRegistry::PackChunkCoords(IntVec2 chunkCoords)
{
    return ChunkHelper::PackCoordinates(chunkCoords.x, chunkCoords.y);
}

IntVec2 ChunkMeshNeighborWaitRegistry::GetNeighborChunkCoords(IntVec2 chunkCoords, ChunkMeshHorizontalNeighbor neighbor)
{
    switch (neighbor)
    {
    case ChunkMeshHorizontalNeighbor::North:
        return IntVec2(chunkCoords.x, chunkCoords.y + 1);
    case ChunkMeshHorizontalNeighbor::South:
        return IntVec2(chunkCoords.x, chunkCoords.y - 1);
    case ChunkMeshHorizontalNeighbor::East:
        return IntVec2(chunkCoords.x + 1, chunkCoords.y);
    case ChunkMeshHorizontalNeighbor::West:
        return IntVec2(chunkCoords.x - 1, chunkCoords.y);
    case ChunkMeshHorizontalNeighbor::None:
        break;
    }

    return chunkCoords;
}

void ChunkMeshNeighborWaitRegistry::AddReverseDependencies(const ChunkMeshNeighborWaitEntry& entry)
{
    if (!entry.HasWaitingNeighbors())
    {
        return;
    }

    const PackedChunkCoords packedTargetCoords = PackChunkCoords(entry.targetChunkCoords);

    if (HasChunkMeshNeighborDependency(entry.waitingNeighborMask, ChunkMeshHorizontalNeighbor::North))
    {
        m_targetsByNeighbor[PackChunkCoords(GetNeighborChunkCoords(entry.targetChunkCoords, ChunkMeshHorizontalNeighbor::North))]
            .insert(packedTargetCoords);
    }

    if (HasChunkMeshNeighborDependency(entry.waitingNeighborMask, ChunkMeshHorizontalNeighbor::South))
    {
        m_targetsByNeighbor[PackChunkCoords(GetNeighborChunkCoords(entry.targetChunkCoords, ChunkMeshHorizontalNeighbor::South))]
            .insert(packedTargetCoords);
    }

    if (HasChunkMeshNeighborDependency(entry.waitingNeighborMask, ChunkMeshHorizontalNeighbor::East))
    {
        m_targetsByNeighbor[PackChunkCoords(GetNeighborChunkCoords(entry.targetChunkCoords, ChunkMeshHorizontalNeighbor::East))]
            .insert(packedTargetCoords);
    }

    if (HasChunkMeshNeighborDependency(entry.waitingNeighborMask, ChunkMeshHorizontalNeighbor::West))
    {
        m_targetsByNeighbor[PackChunkCoords(GetNeighborChunkCoords(entry.targetChunkCoords, ChunkMeshHorizontalNeighbor::West))]
            .insert(packedTargetCoords);
    }
}

void ChunkMeshNeighborWaitRegistry::RemoveReverseDependencies(const ChunkMeshNeighborWaitEntry& entry)
{
    if (!entry.HasWaitingNeighbors())
    {
        return;
    }

    const PackedChunkCoords packedTargetCoords = PackChunkCoords(entry.targetChunkCoords);

    const auto removeTargetFromNeighbor = [this, packedTargetCoords](IntVec2 neighborCoords)
    {
        const PackedChunkCoords packedNeighborCoords = PackChunkCoords(neighborCoords);
        auto reverseIt = m_targetsByNeighbor.find(packedNeighborCoords);
        if (reverseIt == m_targetsByNeighbor.end())
        {
            return;
        }

        reverseIt->second.erase(packedTargetCoords);
        if (reverseIt->second.empty())
        {
            m_targetsByNeighbor.erase(reverseIt);
        }
    };

    if (HasChunkMeshNeighborDependency(entry.waitingNeighborMask, ChunkMeshHorizontalNeighbor::North))
    {
        removeTargetFromNeighbor(GetNeighborChunkCoords(entry.targetChunkCoords, ChunkMeshHorizontalNeighbor::North));
    }

    if (HasChunkMeshNeighborDependency(entry.waitingNeighborMask, ChunkMeshHorizontalNeighbor::South))
    {
        removeTargetFromNeighbor(GetNeighborChunkCoords(entry.targetChunkCoords, ChunkMeshHorizontalNeighbor::South));
    }

    if (HasChunkMeshNeighborDependency(entry.waitingNeighborMask, ChunkMeshHorizontalNeighbor::East))
    {
        removeTargetFromNeighbor(GetNeighborChunkCoords(entry.targetChunkCoords, ChunkMeshHorizontalNeighbor::East));
    }

    if (HasChunkMeshNeighborDependency(entry.waitingNeighborMask, ChunkMeshHorizontalNeighbor::West))
    {
        removeTargetFromNeighbor(GetNeighborChunkCoords(entry.targetChunkCoords, ChunkMeshHorizontalNeighbor::West));
    }
}

bool ChunkMeshNeighborWaitRegistry::ErasePackedEntry(PackedChunkCoords packedTargetCoords)
{
    const auto entryIt = m_entriesByTarget.find(packedTargetCoords);
    if (entryIt == m_entriesByTarget.end())
    {
        return false;
    }

    RemoveReverseDependencies(entryIt->second);
    m_entriesByTarget.erase(entryIt);
    return true;
}

void ChunkMeshNeighborWaitRegistry::RegisterWait(const ChunkMeshNeighborWaitEntry& entry)
{
    const PackedChunkCoords packedTargetCoords = PackChunkCoords(entry.targetChunkCoords);
    ErasePackedEntry(packedTargetCoords);

    if (!entry.IsValid() || !entry.HasWaitingNeighbors())
    {
        return;
    }

    m_entriesByTarget[packedTargetCoords] = entry;
    AddReverseDependencies(entry);
}

bool ChunkMeshNeighborWaitRegistry::CancelWait(IntVec2 targetChunkCoords,
                                               uint64_t targetChunkInstanceId,
                                               uint64_t buildVersion)
{
    const PackedChunkCoords packedTargetCoords = PackChunkCoords(targetChunkCoords);
    const auto entryIt = m_entriesByTarget.find(packedTargetCoords);
    if (entryIt == m_entriesByTarget.end())
    {
        return false;
    }

    if (!MatchesWaitIdentity(entryIt->second, targetChunkInstanceId, buildVersion))
    {
        return false;
    }

    RemoveReverseDependencies(entryIt->second);
    m_entriesByTarget.erase(entryIt);
    return true;
}

bool ChunkMeshNeighborWaitRegistry::CancelWaitForTarget(IntVec2 targetChunkCoords)
{
    return ErasePackedEntry(PackChunkCoords(targetChunkCoords));
}

const ChunkMeshNeighborWaitEntry* ChunkMeshNeighborWaitRegistry::FindWaitEntry(IntVec2 targetChunkCoords) const
{
    const PackedChunkCoords packedTargetCoords = PackChunkCoords(targetChunkCoords);
    const auto entryIt = m_entriesByTarget.find(packedTargetCoords);
    return entryIt != m_entriesByTarget.end() ? &entryIt->second : nullptr;
}

bool ChunkMeshNeighborWaitRegistry::HasWaitEntry(IntVec2 targetChunkCoords) const
{
    return FindWaitEntry(targetChunkCoords) != nullptr;
}

std::vector<ChunkMeshNeighborWaitEntry> ChunkMeshNeighborWaitRegistry::CollectWakeEntries(IntVec2 readableNeighborCoords) const
{
    std::vector<ChunkMeshNeighborWaitEntry> wakeEntries;

    const auto reverseIt = m_targetsByNeighbor.find(PackChunkCoords(readableNeighborCoords));
    if (reverseIt == m_targetsByNeighbor.end())
    {
        return wakeEntries;
    }

    wakeEntries.reserve(reverseIt->second.size());
    for (const PackedChunkCoords packedTargetCoords : reverseIt->second)
    {
        const auto entryIt = m_entriesByTarget.find(packedTargetCoords);
        if (entryIt != m_entriesByTarget.end())
        {
            wakeEntries.push_back(entryIt->second);
        }
    }

    return wakeEntries;
}

void ChunkMeshNeighborWaitRegistry::PruneInvalidEntries(const std::function<bool(const ChunkMeshNeighborWaitEntry&)>& shouldPrune)
{
    if (!shouldPrune)
    {
        return;
    }

    std::vector<PackedChunkCoords> packedTargetsToErase;
    packedTargetsToErase.reserve(m_entriesByTarget.size());

    for (const auto& [packedTargetCoords, entry] : m_entriesByTarget)
    {
        if (shouldPrune(entry))
        {
            packedTargetsToErase.push_back(packedTargetCoords);
        }
    }

    for (const PackedChunkCoords packedTargetCoords : packedTargetsToErase)
    {
        ErasePackedEntry(packedTargetCoords);
    }
}

void ChunkMeshNeighborWaitRegistry::Clear()
{
    m_entriesByTarget.clear();
    m_targetsByNeighbor.clear();
}
