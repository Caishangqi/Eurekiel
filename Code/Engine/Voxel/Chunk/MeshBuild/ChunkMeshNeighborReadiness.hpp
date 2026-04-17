#pragma once

#include "Engine/Math/IntVec2.hpp"

#include <cstdint>

namespace enigma::voxel
{
    enum class ChunkMeshHorizontalNeighbor : uint8_t
    {
        None  = 0u,
        North = 1u << 0u,
        South = 1u << 1u,
        East  = 1u << 2u,
        West  = 1u << 3u,
    };

    using ChunkMeshNeighborDependencyMask = uint8_t;

    inline constexpr ChunkMeshNeighborDependencyMask ToChunkMeshNeighborDependencyMask(ChunkMeshHorizontalNeighbor neighbor) noexcept
    {
        return static_cast<ChunkMeshNeighborDependencyMask>(neighbor);
    }

    inline constexpr ChunkMeshNeighborDependencyMask kChunkMeshNeighborDependencyMaskNone =
        ToChunkMeshNeighborDependencyMask(ChunkMeshHorizontalNeighbor::None);

    inline constexpr ChunkMeshNeighborDependencyMask kChunkMeshNeighborDependencyMaskAllHorizontal =
        ToChunkMeshNeighborDependencyMask(ChunkMeshHorizontalNeighbor::North) |
        ToChunkMeshNeighborDependencyMask(ChunkMeshHorizontalNeighbor::South) |
        ToChunkMeshNeighborDependencyMask(ChunkMeshHorizontalNeighbor::East) |
        ToChunkMeshNeighborDependencyMask(ChunkMeshHorizontalNeighbor::West);

    inline constexpr bool HasChunkMeshNeighborDependency(ChunkMeshNeighborDependencyMask mask,
                                                         ChunkMeshHorizontalNeighbor neighbor) noexcept
    {
        return (mask & ToChunkMeshNeighborDependencyMask(neighbor)) != 0u;
    }

    inline constexpr bool HasAnyChunkMeshNeighborDependencies(ChunkMeshNeighborDependencyMask mask) noexcept
    {
        return mask != kChunkMeshNeighborDependencyMaskNone;
    }

    enum class ChunkMeshNeighborPolicyKind : uint8_t
    {
        StrictNeighborGate = 0,
        RelaxedNeighborAccess,
    };

    enum class ChunkMeshBuildActivationKind : uint8_t
    {
        RebuildRequest = 0,
        NeighborReadableWake,
    };

    struct ChunkMeshNeighborReadinessAssessment
    {
        ChunkMeshNeighborPolicyKind     policyKind                    = ChunkMeshNeighborPolicyKind::StrictNeighborGate;
        ChunkMeshBuildActivationKind    activationKind                = ChunkMeshBuildActivationKind::RebuildRequest;
        ChunkMeshNeighborDependencyMask missingHorizontalMask         = kChunkMeshNeighborDependencyMaskNone;
        bool                            shouldWaitForNeighbors       = false;
        bool                            shouldUseAvailableNeighborData = false;
        bool                            requiresRefinement           = false;

        bool HasMissingHorizontalNeighbors() const noexcept
        {
            return HasAnyChunkMeshNeighborDependencies(missingHorizontalMask);
        }

        bool UsesRelaxedNeighborAccess() const noexcept
        {
            return shouldUseAvailableNeighborData;
        }
    };

    struct ChunkMeshNeighborWaitEntry
    {
        IntVec2                         targetChunkCoords     = IntVec2(0, 0);
        uint64_t                        targetChunkInstanceId = 0;
        uint64_t                        buildVersion          = 0;
        ChunkMeshNeighborDependencyMask waitingNeighborMask   = kChunkMeshNeighborDependencyMaskNone;
        bool                            important             = false;
        bool                            awaitingRefinement    = false;

        bool IsValid() const noexcept
        {
            return targetChunkInstanceId != 0;
        }

        bool HasWaitingNeighbors() const noexcept
        {
            return HasAnyChunkMeshNeighborDependencies(waitingNeighborMask);
        }
    };
}
