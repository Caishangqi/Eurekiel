#include "ChunkMeshBuildTask.hpp"

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Voxel/Chunk/ChunkMeshBuilder.hpp"
#include "Engine/Voxel/Chunk/MeshBuild/ChunkMeshingMaterializer.hpp"
#include "Engine/Voxel/Chunk/MeshBuild/ChunkMeshingSnapshot.hpp"
#include "Engine/Voxel/World/World.hpp"

using namespace enigma::voxel;

namespace
{
    std::shared_ptr<ChunkMeshingScratch> GetThreadLocalMeshingScratch()
    {
        thread_local std::shared_ptr<ChunkMeshingScratch> scratch = std::make_shared<ChunkMeshingScratch>();
        return scratch;
    }

    AsyncChunkMeshMaterializationStatus ToAsyncMaterializationStatus(ChunkMeshingMaterializationStatus status) noexcept
    {
        switch (status)
        {
        case ChunkMeshingMaterializationStatus::Materialized:
            return AsyncChunkMeshMaterializationStatus::Materialized;
        case ChunkMeshingMaterializationStatus::InvalidDispatchContext:
            return AsyncChunkMeshMaterializationStatus::InvalidDispatchContext;
        case ChunkMeshingMaterializationStatus::InvalidScratchBinding:
            return AsyncChunkMeshMaterializationStatus::InvalidScratchBinding;
        case ChunkMeshingMaterializationStatus::ChunkCoordsMismatch:
            return AsyncChunkMeshMaterializationStatus::ChunkCoordsMismatch;
        case ChunkMeshingMaterializationStatus::ChunkInstanceMismatch:
            return AsyncChunkMeshMaterializationStatus::ChunkInstanceMismatch;
        case ChunkMeshingMaterializationStatus::MissingWorld:
            return AsyncChunkMeshMaterializationStatus::MissingWorld;
        case ChunkMeshingMaterializationStatus::ChunkInactive:
            return AsyncChunkMeshMaterializationStatus::ChunkInactive;
        case ChunkMeshingMaterializationStatus::MissingHorizontalNeighbors:
            return AsyncChunkMeshMaterializationStatus::MissingHorizontalNeighbors;
        case ChunkMeshingMaterializationStatus::RetryLater:
            return AsyncChunkMeshMaterializationStatus::ResolveChunkFailed;
        }

        return AsyncChunkMeshMaterializationStatus::ResolveChunkFailed;
    }
}

void ChunkMeshBuildTask::Execute()
{
    ChunkMeshBuildResult result;
    result.chunkCoords     = m_input.GetChunkCoords();
    result.chunkInstanceId = m_input.GetChunkInstanceId();
    result.buildVersion    = m_input.GetBuildVersion();
    result.reloadGeneration = m_input.GetReloadGeneration();

    if (IsCancellationRequested())
    {
        result.status = ChunkMeshBuildResultStatus::Cancelled;
        result.detail = "CancelledBeforeBuild";
        SetResult(std::move(result));
        core::LogDebug("ChunkMeshBuildTask",
                       "Cancelled before build for chunk (%d, %d) version=%llu",
                       m_input.GetChunkCoords().x,
                       m_input.GetChunkCoords().y,
                       m_input.GetBuildVersion());
        return;
    }

    ChunkMeshBuildInput buildInput = m_input;
    if (buildInput.RequiresWorkerMaterialization())
    {
        if (m_world == nullptr)
        {
            result.status = ChunkMeshBuildResultStatus::RetryLater;
            result.detail = "MissingWorldForMaterialization";
            SetResult(std::move(result));
            return;
        }

        m_world->OnWorkerMaterializationStarted();

        Chunk* chunk = m_world->ResolveChunkForMeshing(buildInput.dispatchContext);
        if (chunk == nullptr)
        {
            m_world->OnWorkerMaterializationFinished(AsyncChunkMeshMaterializationStatus::ResolveChunkFailed);
            result.status = ChunkMeshBuildResultStatus::RetryLater;
            result.detail = "ResolveChunkForMaterializationFailed";
            SetResult(std::move(result));
            return;
        }

        std::shared_ptr<ChunkMeshingScratch> scratch = GetThreadLocalMeshingScratch();
        std::shared_ptr<ChunkMeshingSnapshot> snapshot = std::make_shared<ChunkMeshingSnapshot>(scratch);
        const ChunkMeshingMaterializationResult materializeResult =
            ChunkMeshingMaterializer::TryMaterialize(*chunk, buildInput.dispatchContext, *scratch, *snapshot);
        if (!materializeResult.Succeeded())
        {
            m_world->OnWorkerMaterializationFinished(ToAsyncMaterializationStatus(materializeResult.status));
            result.status = ChunkMeshBuildResultStatus::RetryLater;
            result.detail = GetChunkMeshingMaterializationStatusName(materializeResult.status);
            SetResult(std::move(result));
            return;
        }

        m_world->OnWorkerMaterializationFinished(AsyncChunkMeshMaterializationStatus::Materialized);
        buildInput.snapshot = std::move(snapshot);
    }

    ChunkMeshBuilder builder;
    result = builder.Build(buildInput);
    if (buildInput.snapshot)
    {
        result.missingHorizontalNeighborMask = buildInput.snapshot->missingHorizontalNeighborMask;
        result.usedRelaxedNeighborAccess     = buildInput.snapshot->UsesRelaxedNeighborAccess();
        result.partialMeshBuilt              = buildInput.snapshot->IsPartialHorizontalSnapshot();
        result.requiresNeighborRefinement    = buildInput.snapshot->RequiresNeighborRefinement();

        if (result.status == ChunkMeshBuildResultStatus::Built && result.IsPartialMeshResult())
        {
            result.detail = "BuiltPartial";
        }
    }

    if (IsCancellationRequested())
    {
        result.mesh.reset();
        result.status = ChunkMeshBuildResultStatus::Cancelled;
        result.detail = "CancelledAfterBuild";
        SetResult(std::move(result));
        core::LogDebug("ChunkMeshBuildTask",
                       "Cancelled after build for chunk (%d, %d) version=%llu",
                       m_input.GetChunkCoords().x,
                       m_input.GetChunkCoords().y,
                       m_input.GetBuildVersion());
        return;
    }

    SetResult(std::move(result));
}
