#pragma once

#include "Engine/Voxel/Chunk/MeshBuild/ChunkMeshingDispatchContext.hpp"
#include "Engine/Voxel/Chunk/MeshBuild/ChunkMeshingScratch.hpp"
#include "Engine/Voxel/Chunk/MeshBuild/ChunkMeshingSnapshot.hpp"

namespace enigma::voxel
{
    enum class ChunkMeshingMaterializationStatus
    {
        Materialized,
        RetryLater,
        InvalidDispatchContext,
        InvalidScratchBinding,
        ChunkCoordsMismatch,
        ChunkInstanceMismatch,
        MissingWorld,
        ChunkInactive,
        MissingHorizontalNeighbors,
    };

    inline const char* GetChunkMeshingMaterializationStatusName(ChunkMeshingMaterializationStatus status) noexcept
    {
        switch (status)
        {
        case ChunkMeshingMaterializationStatus::Materialized: return "Materialized";
        case ChunkMeshingMaterializationStatus::RetryLater: return "RetryLater";
        case ChunkMeshingMaterializationStatus::InvalidDispatchContext: return "InvalidDispatchContext";
        case ChunkMeshingMaterializationStatus::InvalidScratchBinding: return "InvalidScratchBinding";
        case ChunkMeshingMaterializationStatus::ChunkCoordsMismatch: return "ChunkCoordsMismatch";
        case ChunkMeshingMaterializationStatus::ChunkInstanceMismatch: return "ChunkInstanceMismatch";
        case ChunkMeshingMaterializationStatus::MissingWorld: return "MissingWorld";
        case ChunkMeshingMaterializationStatus::ChunkInactive: return "ChunkInactive";
        case ChunkMeshingMaterializationStatus::MissingHorizontalNeighbors: return "MissingHorizontalNeighbors";
        default: return "Unknown";
        }
    }

    struct ChunkMeshingMaterializationResult
    {
        ChunkMeshingMaterializationStatus status = ChunkMeshingMaterializationStatus::RetryLater;
        const char*                      detail = "Uninitialized";

        bool Succeeded() const noexcept
        {
            return status == ChunkMeshingMaterializationStatus::Materialized;
        }
    };

    class Chunk;

    class ChunkMeshingMaterializer
    {
    public:
        static ChunkMeshingMaterializationResult TryMaterialize(const Chunk& chunk,
                                                               const ChunkMeshingDispatchContext& context,
                                                               ChunkMeshingScratch& scratch,
                                                               ChunkMeshingSnapshot& outSnapshot);
    };
}
