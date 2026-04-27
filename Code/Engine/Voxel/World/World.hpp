#pragma once
#pragma once
#include "../Block/BlockState.hpp"
#include "../Block/BlockPos.hpp"
#include "../Block/BlockIterator.hpp"
#include "../Light/VoxelLightEngine.hpp"
#include "../Time/ITimeProvider.hpp"

#include "../Chunk/ChunkSerializationInterfaces.hpp"
#include "../Chunk/ChunkRenderRegionStorage.hpp"
#include "../Chunk/ChunkJob.hpp"
#include "../Chunk/GenerateChunkJob.hpp"
#include "../Chunk/LoadChunkJob.hpp"
#include "../Chunk/MeshBuild/ChunkMeshNeighborReadiness.hpp"
#include "../Chunk/MeshBuild/ChunkMeshBuildInputFactory.hpp"
#include "../Chunk/MeshBuild/AsyncChunkMeshDiagnostics.hpp"
#include "../Chunk/MeshBuild/ChunkMeshBuildTask.hpp"
#include "../Chunk/SaveChunkJob.hpp"
#include "../Generation/TerrainGenerator.hpp"
#include "ChunkMeshNeighborWaitRegistry.hpp"
#include "ESFWorldStorage.hpp"
#include "VoxelRaycastResult3D.hpp"
#include "Engine/Graphic/Reload/RenderPipelineReloadTypes.hpp"
#include "../../Math/Vec3.hpp"
#include "../../Math/IntVec2.hpp"
#include "../../Core/Schedule/ScheduleSubsystem.hpp"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <deque>
#include <atomic>
#include <optional>

class Texture;

namespace enigma::voxel
{
    // ========================================================================
    // Core Configuration
    // ========================================================================
    constexpr char WORLD_SAVE_PATH[] = ".enigma/saves";

    /**
         * @brief Main world class - manages the voxel world and its chunks
         *
         * Reconstructed World class responsibilities:
         * - World-class logic management and coordination
         * - Get and set block status (through block proxy)
         * - World-class configuration management (world name, seed, height limit)
         * - Player position tracking and block requirements calculation
         * - Integration and scheduling of world generators
         * - Unified portal for rendering and updating
         */
    struct ChunkMeshBuildState
    {
        uint64_t                                   latestRequestedVersion = 0;
        uint64_t                                   latestSubmittedVersion = 0;
        uint64_t                                   chunkInstanceId        = 0;
        enigma::graphic::RenderPipelineReloadGeneration requestedReloadGeneration;
        enigma::graphic::RenderPipelineReloadGeneration submittedReloadGeneration;
        enigma::graphic::RenderPipelineReloadGeneration publishedReloadGeneration;
        std::optional<enigma::core::TaskHandle>    activeHandle;
        bool                                       pendingDispatch        = false;
        bool                                       important              = false;
        ChunkMeshNeighborReadinessAssessment       pendingNeighborReadiness;
        bool                                       waitingForNeighbors    = false;
        ChunkMeshNeighborDependencyMask            waitingNeighborMask    = kChunkMeshNeighborDependencyMaskNone;
        bool                                       partialMeshPublished   = false;
        bool                                       refinementPending      = false;
        bool                                       activeImportant        = false;
        ChunkMeshNeighborReadinessAssessment       activeNeighborReadiness;
        bool                                       activeRequiresWorkerMaterialization = false;
    };

    class World
    {
    public:
        World(const std::string& worldName, uint64_t worldSeed, std::unique_ptr<enigma::voxel::TerrainGenerator> generator);

        World();
        ~World();

        // Block Operations:
        BlockState* GetBlockState(const BlockPos& pos);
        void        SetBlockState(const BlockPos& pos, BlockState* state) const;
        bool        IsValidPosition(const BlockPos& pos);
        bool        IsBlockLoaded(const BlockPos& pos);
        BlockState* GetTopBlock(const BlockPos& pos);
        BlockState* GetTopBlock(int32_t x, int32_t y);
        int         GetTopBlockZ(const BlockPos& pos);

        // Raycast Operations:
        VoxelRaycastResult3D RaycastVsBlocks(const Vec3& rayStart, const Vec3& rayFwdNormal, float rayMaxLength = 8.0f) const;

        // Block Operations - Player digging and placing
        void DigBlock(const BlockIterator& blockIter);
        void PlaceBlock(const BlockIterator& blockIter, BlockState* newState);

        // PlaceBlock with placement context (for slab/stairs)
        void PlaceBlock(const BlockIterator&        blockIter, enigma::registry::block::Block* blockType,
                        const VoxelRaycastResult3D& raycast, const Vec3&                       playerLookDir);

        //-------------------------------------------------------------------------------------------
        // VoxelLightEngine Integration
        //-------------------------------------------------------------------------------------------
        VoxelLightEngine&       GetVoxelLightEngine() { return *m_voxelLightEngine; }
        const VoxelLightEngine& GetVoxelLightEngine() const { return *m_voxelLightEngine; }

        // Light Data Access (delegate to VoxelLightEngine)
        uint8_t GetSkyLight(int32_t globalX, int32_t globalY, int32_t globalZ) const;
        uint8_t GetBlockLight(int32_t globalX, int32_t globalY, int32_t globalZ) const;
        bool    GetIsSky(int32_t globalX, int32_t globalY, int32_t globalZ) const;

        // Sky Darken (calculated from time)
        int  GetSkyDarken() const { return m_skyDarken; }
        void UpdateSkyBrightness(const ITimeProvider& timeProvider);

        // Light Dirty Marking (convenience wrapper for Chunk usage)
        void MarkLightingDirty(const BlockIterator& iter);

        // Chunk Operations:
        Chunk* GetChunk(int32_t chunkCoordinateX, int32_t chunkCoordinateY);
        Chunk* GetChunk(int32_t chunkCoordinateX, int32_t chunkCoordinateY) const;
        Chunk* GetChunk(const BlockPos& blockPosition) const;
        Chunk* ResolveChunkForMeshing(const ChunkMeshingDispatchContext& context) const;
        void   UnloadChunk(int32_t chunkCoordinateX, int32_t chunkCoordinateY);
        bool   IsChunkLoaded(int32_t chunkCoordinateX, int32_t chunkCoordinateY);

        void                                     UpdateNearbyChunks(); // Update nearby blocks according to player location
        std::vector<std::pair<int32_t, int32_t>> CalculateNeededChunks() const; // Calculate the required blocks

        // Update and Management:
        void Update(float deltaTime); // Update world systems
        void SetPlayerPosition(const Vec3& position); // Update player position for chunk loading
        void SetChunkActivationRange(int chunkDistance); // Set activation range in chunks

        // Rendering:

        bool SetEnableChunkDebug(bool enable = true);

        // Utility

        bool                                                 IsChunkLoadedDirect(int32_t chunkX, int32_t chunkY) const;
        size_t                                               GetLoadedChunkCount() const;
        std::unordered_map<int64_t, std::unique_ptr<Chunk>>& GetLoadedChunks();
        ChunkBatchStats&                                     MutableChunkBatchStats() { return m_chunkBatchStats; }
        const ChunkBatchStats&                               GetChunkBatchStats() const { return m_chunkBatchStats; }
        ChunkRenderRegionStorage&                            GetChunkRenderRegionStorage() { return m_chunkRenderRegionStorage; }
        const ChunkRenderRegionStorage&                      GetChunkRenderRegionStorage() const { return m_chunkRenderRegionStorage; }
        const AsyncChunkMeshDiagnostics&                     GetAsyncChunkMeshDiagnostics() const { return m_asyncChunkMeshDiagnostics; }
        uint32_t                                             GetMaxChunkBatchRegionRebuildsPerFrame() const { return m_maxChunkBatchRegionRebuildsPerFrame; }

        // 调试功能
        bool SetEnableChunkDebugDirect(bool enable = true);

        // 渲染资源
        Texture* GetBlocksAtlasTexture() const;

        // World generation integration
        void SetWorldGenerator(std::unique_ptr<enigma::voxel::TerrainGenerator> generator);

        // Serialize the configuration interface
        void SetChunkSerializer(std::unique_ptr<IChunkSerializer> serializer);
        void SetChunkStorage(std::unique_ptr<IChunkStorage> storage);

        // World file management interface
        bool InitializeWorldStorage(const std::string& savesPath);
        bool SaveWorld();
        bool LoadWorld();
        void CloseWorld();

        //-------------------------------------------------------------------------------------------
        // Phase 5: Graceful Shutdown Support
        //-------------------------------------------------------------------------------------------
        void PrepareShutdown(); // Stop accepting new tasks
        void WaitForPendingTasks(); // Wait for existing tasks to complete
        bool IsShuttingDown() const { return m_isShuttingDown.load(); }

        // Get world information
        const std::string& GetWorldName() const { return m_worldName; }
        uint64_t           GetWorldSeed() const { return m_worldSeed; }
        const std::string& GetWorldPath() const { return m_worldPath; }
        uint64_t           GetWorldLifetimeToken() const { return m_worldLifetimeToken.load(); }
        void               OnWorkerMaterializationStarted();
        void               OnWorkerMaterializationFinished(AsyncChunkMeshMaterializationStatus status);

        //-------------------------------------------------------------------------------------------
        // Phase 3: Async Task Management (Multi-threaded Chunk Loading)
        //-------------------------------------------------------------------------------------------

        // Process completed tasks from ScheduleSubsystem (uses global g_theSchedule)
        // Called automatically in Update() loop on main thread
        void ProcessCompletedChunkTasks();

        //-------------------------------------------------------------------------------------------
        // Phase 2: Cross-Chunk Hidden Face Culling Support
        //-------------------------------------------------------------------------------------------

        // Coordinate readable-chunk activation at world scope so waiting wakeups
        // run before generic neighbor exposure refresh propagation.
        void HandleChunkBecameMeshReadable(Chunk& chunk);

        // Mark a chunk as needing mesh rebuild and add to queue
        // Called by lifecycle, block edits, explicit invalidation, and controlled neighbor refresh
        void ScheduleChunkMeshRebuild(Chunk* chunk, bool forceImportant = false);

        // [R5.0] Mark all loaded chunks as dirty and schedule mesh rebuild
        // Called when ShaderBundle switches (material ID mappings change)
        // Reference: Iris levelRenderer.allChanged() on shader pack switch
        void InvalidateAllChunkMeshes();

        void BeginReloadGeneration(enigma::graphic::RenderPipelineReloadGeneration generation);
        void MarkLoadedChunksForReload(enigma::graphic::RenderPipelineReloadGeneration generation);
        enigma::graphic::RenderPipelineReloadChunkGateSnapshot GetReloadChunkGateSnapshot(
            enigma::graphic::RenderPipelineReloadGeneration generation) const;
        void EndReloadGeneration(enigma::graphic::RenderPipelineReloadGeneration generation);

        //-------------------------------------------------------------------------------------------
        // [REFACTORED] Lighting System - Now delegates to VoxelLightEngine
        //-------------------------------------------------------------------------------------------

        // Clean dirty light queue for a specific chunk (called during chunk unload)
        void UndirtyAllBlocksInChunk(Chunk* chunk);

    private:
        //-------------------------------------------------------------------------------------------
        // Phase 3: Async Task Management Methods
        //-------------------------------------------------------------------------------------------

        // Activate a chunk (decide whether to load from disk or generate)
        void ActivateChunk(IntVec2 chunkCoords);

        // Submit chunk generation job to ScheduleSubsystem
        bool SubmitGenerateChunkJob(IntVec2 chunkCoords, Chunk* chunk);

        // Submit chunk load job to ScheduleSubsystem
        bool SubmitLoadChunkJob(IntVec2 chunkCoords, Chunk* chunk);

        // Submit chunk save job to ScheduleSubsystem
        bool SubmitSaveChunkJob(IntVec2 chunkCoords, const Chunk* chunk);

        void StoreActiveChunkTaskHandle(std::unordered_map<int64_t, enigma::core::TaskHandle>& handleMap, IntVec2 chunkCoords,
                                        const enigma::core::TaskHandle& handle);
        void ClearActiveChunkTaskHandle(std::unordered_map<int64_t, enigma::core::TaskHandle>& handleMap, IntVec2 chunkCoords);
        bool RequestActiveChunkTaskCancellation(std::unordered_map<int64_t, enigma::core::TaskHandle>& handleMap, IntVec2 chunkCoords,
                                                const char* jobLabel);
        bool FinalizePendingUnloadChunk(Chunk* chunk, IntVec2 chunkCoords, const char* jobLabel);
        void ProcessGenerateChunkTaskRecord(const enigma::core::TaskCompletionRecord& record, GenerateChunkJob* job);
        void ProcessLoadChunkTaskRecord(const enigma::core::TaskCompletionRecord& record, LoadChunkJob* job);
        void ProcessSaveChunkTaskRecord(const enigma::core::TaskCompletionRecord& record, SaveChunkJob* job);
        void ProcessChunkMeshBuildTaskRecord(const enigma::core::TaskCompletionRecord& record, ChunkMeshBuildTask* task);
        void LogDiscardedChunkTaskRecord(const enigma::core::TaskCompletionRecord& record, const char* jobLabel, const char* reason) const;

        // Handle completed generation job
        void HandleGenerateChunkCompleted(GenerateChunkJob* job);

        // Handle completed load job
        void HandleLoadChunkCompleted(LoadChunkJob* job);

        // Handle completed save job
        void HandleSaveChunkCompleted(SaveChunkJob* job);
        void HandleChunkMeshBuildCompleted(const enigma::core::TaskCompletionRecord& record,
                                           ChunkMeshBuildTask* task,
                                           std::optional<ChunkMeshBuildResult> result);
        bool ShouldRequeueDiscardedChunkMeshBuild(const enigma::core::TaskCompletionRecord& record,
                                                  const std::optional<ChunkMeshBuildResult>& result,
                                                  const Chunk& chunk,
                                                  const ChunkMeshBuildState& buildState) const;
        bool CanPublishChunkMeshBuildResult(const Chunk& chunk,
                                            const ChunkMeshBuildState& buildState,
                                            const ChunkMeshBuildTask& task,
                                            const ChunkMeshBuildResult& result,
                                            const char*& rejectionReason) const;
        ChunkMeshNeighborDependencyMask CollectMissingHorizontalNeighborMask(const Chunk& chunk) const;
        ChunkMeshNeighborReadinessAssessment EvaluateChunkMeshNeighborReadiness(const Chunk& chunk,
                                                                                const ChunkMeshBuildState& buildState) const;
        void ClearChunkMeshNeighborWaitState(ChunkMeshBuildState& buildState, IntVec2 chunkCoords, const char* reason);
        bool RegisterChunkMeshNeighborWait(Chunk& chunk,
                                           ChunkMeshBuildState& buildState,
                                           const ChunkMeshNeighborReadinessAssessment& readiness);
        bool ShouldPruneChunkMeshNeighborWait(const ChunkMeshNeighborWaitEntry& entry) const;
        void HandleChunkMeshNeighborReadable(IntVec2 readableChunkCoords);
        void QueueNeighborExposureRefreshesForReadableChunk(Chunk& readableChunk);
        void QueueChunkMeshBuildRetry(Chunk& chunk, ChunkMeshBuildState& buildState, IntVec2 chunkCoords);
        bool IsImportantChunkMeshBuild(const Chunk& chunk, bool forceImportant = false) const;
        bool HasImportantChunkMeshBuildInFlight() const;
        void RunImportantChunkBoundedWait();
        void RefreshAsyncChunkMeshDiagnosticsSnapshot();
        void CancelActiveChunkMeshBuilds(const char* reason);
        bool HasOutstandingChunkMeshBuildWork() const;
        void ClearAsyncChunkMeshBuildState();
        void ReleaseLoadedChunkRuntimeState();

        //-------------------------------------------------------------------------------------------
        // Async Chunk Mesh Dispatch
        //-------------------------------------------------------------------------------------------

        // Update chunk mesh requests (called from Update())
        void UpdateChunkMeshes();
        uint32_t DispatchQueuedChunkMeshBuilds(uint32_t maxDispatchCount);
        uint32_t RebuildQueuedChunkMeshes(uint32_t maxRebuildCount);
        bool     RebuildChunkMeshNow(Chunk* chunk);
        bool     SubmitChunkMeshBuildTask(Chunk* chunk, ChunkMeshBuildState& buildState);
        bool     RequestChunkMeshBuildCancellation(IntVec2 chunkCoords, const char* reason);
        bool     CanDispatchAsyncChunkMeshBuilds() const;
        void     RefreshAsyncChunkMeshMode();
        void     EnqueueChunkMeshBuildRequest(IntVec2 chunkCoords);
        void     RemovePendingChunkMeshBuildRequest(IntVec2 chunkCoords);
        ChunkMeshBuildState& GetOrCreateChunkMeshBuildState(IntVec2 chunkCoords);
        ChunkMeshBuildState* FindChunkMeshBuildState(IntVec2 chunkCoords);
        const ChunkMeshBuildState* FindChunkMeshBuildState(IntVec2 chunkCoords) const;
        void     EraseChunkMeshBuildState(IntVec2 chunkCoords);

        // Sort mesh rebuild queue by distance to player (nearest first)
        void SortMeshQueueByDistance();

        // Get distance from chunk to player (helper for sorting)
        float GetChunkDistanceToPlayer(Chunk* chunk) const;

        //-------------------------------------------------------------------------------------------
        // Phase 4: Job Queue Management Methods
        //-------------------------------------------------------------------------------------------

        // Process pending job queues and submit jobs when under limit
        // Called from Update() every frame to enforce job limits
        void ProcessJobQueues();

        // Remove jobs from pending queues that are beyond activation range
        // Called from Update() to cancel jobs for chunks that moved out of range
        void RemoveDistantJobs();

        // Phase 2.2: Cancel pending jobs for a specific chunk
        // Called when unloading a chunk to remove it from all pending queues
        void CancelPendingJobsForChunk(IntVec2 coords);

        // Phase 3: Helper method to check if coords are in queue
        bool IsInQueue(const std::deque<IntVec2>& queue, IntVec2 coords) const;

        // Phase 6: Chunk Unloading Logic
        void UnloadFarthestChunk(); // Unload the farthest chunk if beyond deactivation range

        // Calculate the distance from the block to the player
        float GetChunkDistanceToPlayer(int32_t chunkX, int32_t chunkY) const;

        // Find the loaded chunk farthest from the player
        std::pair<int32_t, int32_t> FindFarthestChunk() const;

    private:
        std::unordered_map<int64_t, std::unique_ptr<Chunk>> m_loadedChunks;
        bool                                                m_enableChunkDebug         = false;
        Texture*                                            m_cachedBlocksAtlasTexture = nullptr;
        ChunkBatchStats                                     m_chunkBatchStats;
        ChunkRenderRegionStorage                            m_chunkRenderRegionStorage;

        std::string m_worldName; // World identifier
        std::string m_worldPath; // World storage path
        uint64_t    m_worldSeed = 0; // World generation seed

        // Player position and chunk management
        Vec3    m_playerPosition{0.0f, 0.0f, 256.0f}; // Current player position
        int32_t m_chunkActivationRange   = 12; // Activation range in chunks (from settings.yml)
        int32_t m_chunkDeactivationRange = 14; // Deactivation range = activation + 2 chunks

        // World generation
        std::unique_ptr<TerrainGenerator> m_worldGenerator = nullptr;

        // Serialize components
        std::unique_ptr<IChunkSerializer> m_chunkSerializer;
        std::unique_ptr<IChunkStorage>    m_chunkStorage;

        // World File Manager
        std::unique_ptr<ESFWorldManager> m_worldManager;

        //-------------------------------------------------------------------------------------------
        // Phase 3: Async Task Management State (REMOVED tracking sets)
        //-------------------------------------------------------------------------------------------
        // [Phase 3] Removed m_chunksWithPendingLoad/Generate/Save tracking sets
        // Now using IsInQueue() helper to check pending queues directly

        //-------------------------------------------------------------------------------------------
        // Phase 4: Job Queue and Limits System (Assignment 03 Requirements)
        //-------------------------------------------------------------------------------------------

        // World-side pending job queues (sorted by distance, nearest first)
        // Jobs are added to these queues instead of immediately submitting to ScheduleSubsystem
        // ProcessJobQueues() will submit jobs when active count < limit
        std::deque<IntVec2> m_pendingGenerateQueue; // Chunks waiting for generation
        std::deque<IntVec2> m_pendingLoadQueue; // Chunks waiting for disk load
        std::deque<IntVec2> m_pendingSaveQueue; // Chunks waiting for disk save

        // Active job counters (atomic for thread-safe access)
        // Incremented when job submitted to ScheduleSubsystem
        // Decremented when job completes in HandleXxxCompleted()
        std::atomic<int> m_activeGenerateJobs{0}; // Currently processing generate jobs
        std::atomic<int> m_activeLoadJobs{0}; // Currently processing load jobs
        std::atomic<int> m_activeSaveJobs{0}; // Currently processing save jobs
        std::unordered_map<int64_t, enigma::core::TaskHandle> m_activeGenerateJobHandles;
        std::unordered_map<int64_t, enigma::core::TaskHandle> m_activeLoadJobHandles;
        std::unordered_map<int64_t, enigma::core::TaskHandle> m_activeSaveJobHandles;

        // Job limits (Assignment 03 spec: "100s of generate, only a few load/save")
        // These prevent overwhelming the thread pool and ensure responsive chunk loading
        int m_maxGenerateJobs = 512; // Allow many generation jobs (CPU-bound, parallelizable)
        int m_maxLoadJobs     = 64; // Increased for ESFS format (no file lock contention, SSD-friendly)
        int m_maxSaveJobs     = 32; // Increased for better save throughput (lower priority than load)

        //-------------------------------------------------------------------------------------------
        // Async Chunk Mesh Build State
        //-------------------------------------------------------------------------------------------
        std::unordered_map<int64_t, ChunkMeshBuildState> m_chunkMeshBuildStates;
        ChunkMeshNeighborWaitRegistry                    m_chunkMeshNeighborWaitRegistry;
        std::deque<IntVec2>                              m_pendingChunkMeshBuildQueue;
        AsyncChunkMeshDiagnostics                        m_asyncChunkMeshDiagnostics;
        enigma::graphic::RenderPipelineReloadGeneration  m_currentReloadGeneration;
        enigma::graphic::RenderPipelineReloadGeneration  m_activeReloadGeneration;
        std::unordered_set<int64_t>                      m_reloadAffectedVisibleChunkKeys;
        bool                                             m_asyncChunkMeshEnabled = true;
        int                                              m_maxMeshRebuildsPerFrame = 2; // Maximum chunk mesh dispatches/rebuilds per frame
        float                                            m_importantChunkDistanceThreshold = 2.0f;
        bool                                             m_enableImportantChunkBoundedWait = true;
        uint32_t                                         m_importantChunkWaitBudgetMicros = 750;
        uint32_t                                         m_importantChunkWaitYieldLimit   = 8;
        uint32_t           m_maxChunkBatchRegionRebuildsPerFrame = 2;

        //-------------------------------------------------------------------------------------------
        // Phase 5: Graceful Shutdown State
        //-------------------------------------------------------------------------------------------
        std::atomic<bool> m_isShuttingDown{false}; // Shutdown flag (thread-safe)
        std::atomic<uint64_t> m_worldLifetimeToken{1};

        //-------------------------------------------------------------------------------------------
        // [REFACTORED] Lighting System State - Now uses VoxelLightEngine
        //-------------------------------------------------------------------------------------------
        std::unique_ptr<VoxelLightEngine> m_voxelLightEngine; // Composite light engine
        int                               m_skyDarken = 0; // Computed from time (0 at noon, 11 at midnight)
        struct AsyncChunkMeshWorkerCounterSnapshot
        {
            uint64_t attempts          = 0;
            uint64_t succeeded         = 0;
            uint64_t retryLater        = 0;
            uint64_t resolveFailed     = 0;
            uint64_t missingNeighbors  = 0;
            uint64_t validationFailed  = 0;
        };

        std::atomic<uint64_t> m_workerMaterializationAttempts{0};
        std::atomic<uint64_t> m_workerMaterializationSucceeded{0};
        std::atomic<uint64_t> m_workerMaterializationRetryLater{0};
        std::atomic<uint64_t> m_workerMaterializationResolveFailed{0};
        std::atomic<uint64_t> m_workerMaterializationMissingNeighbors{0};
        std::atomic<uint64_t> m_workerMaterializationValidationFailed{0};
        std::atomic<uint64_t> m_workerMaterializationExecutingCount{0};
        std::atomic<uint8_t>  m_lastWorkerMaterializationStatus{
            static_cast<uint8_t>(AsyncChunkMeshMaterializationStatus::None)
        };
        std::atomic<uint8_t>  m_lastWorkerMaterializationFailure{
            static_cast<uint8_t>(AsyncChunkMeshMaterializationStatus::None)
        };
        AsyncChunkMeshWorkerCounterSnapshot m_previousWorkerMaterializationCounters;
    };
}
