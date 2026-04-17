#pragma once

#include "Engine/Math/IntVec2.hpp"
#include "Engine/Voxel/Chunk/MeshBuild/ChunkMeshNeighborReadiness.hpp"
#include "Engine/Voxel/Chunk/ChunkMesh.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace enigma::voxel
{
    enum class ChunkMeshBuildResultStatus : uint8_t
    {
        Built = 0,
        RetryLater,
        Cancelled,
        Failed
    };

    inline const char* GetChunkMeshBuildResultStatusName(ChunkMeshBuildResultStatus status) noexcept
    {
        switch (status)
        {
        case ChunkMeshBuildResultStatus::Built:
            return "Built";
        case ChunkMeshBuildResultStatus::RetryLater:
            return "RetryLater";
        case ChunkMeshBuildResultStatus::Cancelled:
            return "Cancelled";
        case ChunkMeshBuildResultStatus::Failed:
            return "Failed";
        }

        return "Unknown";
    }

    struct ChunkMeshBuildMetrics
    {
        uint64_t opaqueVertexCount      = 0;
        uint64_t cutoutVertexCount      = 0;
        uint64_t translucentVertexCount = 0;
        uint64_t opaqueIndexCount       = 0;
        uint64_t cutoutIndexCount       = 0;
        uint64_t translucentIndexCount  = 0;
    };

    struct ChunkMeshBuildResult
    {
        IntVec2                     chunkCoords     = IntVec2(0, 0);
        uint64_t                    chunkInstanceId = 0;
        uint64_t                    buildVersion    = 0;
        ChunkMeshBuildResultStatus  status          = ChunkMeshBuildResultStatus::Failed;
        ChunkMeshNeighborDependencyMask missingHorizontalNeighborMask = kChunkMeshNeighborDependencyMaskNone;
        bool                        usedRelaxedNeighborAccess = false;
        bool                        partialMeshBuilt = false;
        bool                        requiresNeighborRefinement = false;
        std::unique_ptr<ChunkMesh>  mesh;
        ChunkMeshBuildMetrics       metrics;
        std::string                 detail;

        bool HasMesh() const noexcept
        {
            return mesh != nullptr;
        }

        bool RequiresRetry() const noexcept
        {
            return status == ChunkMeshBuildResultStatus::RetryLater;
        }

        bool HasMissingHorizontalNeighbors() const noexcept
        {
            return HasAnyChunkMeshNeighborDependencies(missingHorizontalNeighborMask);
        }

        bool IsPartialMeshResult() const noexcept
        {
            return partialMeshBuilt;
        }
    };
}
