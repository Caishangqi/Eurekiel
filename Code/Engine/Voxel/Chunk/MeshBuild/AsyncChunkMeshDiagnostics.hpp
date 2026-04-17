#pragma once

#include <cstdint>
#include <string>

namespace enigma::voxel
{
    enum class AsyncChunkMeshMode : uint8_t
    {
        AsyncEnabled = 0,
        SyncFallback
    };

    inline const char* GetAsyncChunkMeshModeName(AsyncChunkMeshMode mode) noexcept
    {
        switch (mode)
        {
        case AsyncChunkMeshMode::AsyncEnabled:
            return "AsyncEnabled";
        case AsyncChunkMeshMode::SyncFallback:
            return "SyncFallback";
        }

        return "Unknown";
    }

    enum class AsyncChunkMeshMaterializationStatus : uint8_t
    {
        None = 0,
        Materialized,
        MissingWorldForTask,
        ResolveChunkFailed,
        InvalidDispatchContext,
        InvalidScratchBinding,
        ChunkCoordsMismatch,
        ChunkInstanceMismatch,
        MissingWorld,
        ChunkInactive,
        MissingHorizontalNeighbors,
    };

    inline const char* GetAsyncChunkMeshMaterializationStatusName(AsyncChunkMeshMaterializationStatus status) noexcept
    {
        switch (status)
        {
        case AsyncChunkMeshMaterializationStatus::None:
            return "None";
        case AsyncChunkMeshMaterializationStatus::Materialized:
            return "Materialized";
        case AsyncChunkMeshMaterializationStatus::MissingWorldForTask:
            return "MissingWorldForTask";
        case AsyncChunkMeshMaterializationStatus::ResolveChunkFailed:
            return "ResolveChunkFailed";
        case AsyncChunkMeshMaterializationStatus::InvalidDispatchContext:
            return "InvalidDispatchContext";
        case AsyncChunkMeshMaterializationStatus::InvalidScratchBinding:
            return "InvalidScratchBinding";
        case AsyncChunkMeshMaterializationStatus::ChunkCoordsMismatch:
            return "ChunkCoordsMismatch";
        case AsyncChunkMeshMaterializationStatus::ChunkInstanceMismatch:
            return "ChunkInstanceMismatch";
        case AsyncChunkMeshMaterializationStatus::MissingWorld:
            return "MissingWorld";
        case AsyncChunkMeshMaterializationStatus::ChunkInactive:
            return "ChunkInactive";
        case AsyncChunkMeshMaterializationStatus::MissingHorizontalNeighbors:
            return "MissingHorizontalNeighbors";
        }

        return "Unknown";
    }

    struct AsyncChunkMeshCounters
    {
        uint64_t queued                        = 0;
        uint64_t submitted                     = 0;
        uint64_t executing                     = 0;
        uint64_t completed                     = 0;
        uint64_t published                     = 0;
        uint64_t neighborWaitRegistered        = 0;
        uint64_t neighborWaitWoken             = 0;
        uint64_t neighborWaitCancelled         = 0;
        uint64_t partialBuildSubmitted         = 0;
        uint64_t partialBuildPublished         = 0;
        uint64_t refinementBuildQueued         = 0;
        uint64_t refinementBuildPublished      = 0;
        uint64_t mainThreadSnapshotBuildCount  = 0;
        uint64_t workerMaterializationAttempts = 0;
        uint64_t workerMaterializationSucceeded = 0;
        uint64_t workerMaterializationRetryLater = 0;
        uint64_t workerMaterializationResolveFailed = 0;
        uint64_t workerMaterializationMissingNeighbors = 0;
        uint64_t workerMaterializationValidationFailed = 0;
        uint64_t discardedStale                = 0;
        uint64_t discardedCancelled            = 0;
        uint64_t retryLater                    = 0;
        uint64_t syncFallbackCount             = 0;
        uint64_t boundedWaitAttempts           = 0;
        uint64_t boundedWaitSatisfied          = 0;
        uint64_t boundedWaitTimedOut           = 0;
        uint64_t boundedWaitYieldCount         = 0;
        uint64_t boundedWaitMicroseconds = 0;
    };

    struct AsyncChunkMeshLiveState
    {
        uint64_t queuedBacklog           = 0;
        uint64_t pendingDispatchCount    = 0;
        uint64_t activeHandleCount       = 0;
        uint64_t schedulerQueuedCount    = 0;
        uint64_t schedulerExecutingCount = 0;
        uint64_t workerMaterializationQueuedCount = 0;
        uint64_t workerMaterializationExecutingCount = 0;
        uint64_t workerMaterializationInFlight = 0;
        uint64_t waitingForNeighborsCount = 0;
        uint64_t partialMeshPublishedCount = 0;
        uint64_t refinementPendingCount = 0;
        uint64_t importantPendingCount    = 0;
        uint64_t importantActiveCount     = 0;
        uint64_t importantQueuedCount     = 0;
        uint64_t importantExecutingCount = 0;
    };

    struct AsyncChunkMeshDiagnostics
    {
        AsyncChunkMeshMode mode                         = AsyncChunkMeshMode::AsyncEnabled;
        bool               asyncEnabled                 = true;
        bool               fallbackActive               = false;
        std::string        lastFallbackReason           = "None";
        std::string        lastWorkerMaterializationStatus = "None";
        std::string        lastWorkerMaterializationFailureReason = "None";
        AsyncChunkMeshLiveState live;
        AsyncChunkMeshCounters  frame;
        AsyncChunkMeshCounters  cumulative;

        void ClearFrame() noexcept
        {
            live  = {};
            frame = {};
        }

        void ClearAll()
        {
            mode               = AsyncChunkMeshMode::AsyncEnabled;
            asyncEnabled       = true;
            fallbackActive     = false;
            lastFallbackReason = "None";
            lastWorkerMaterializationStatus = "None";
            lastWorkerMaterializationFailureReason = "None";
            live               = {};
            frame              = {};
            cumulative         = {};
        }
    };
}
