#pragma once
#include "../Block/BlockState.hpp"
#include "../Block/BlockPos.hpp"
#include "../Block/BlockIterator.hpp"

#include "../Chunk/ChunkSerializationInterfaces.hpp"
#include "../Chunk/ChunkJob.hpp"
#include "../Chunk/GenerateChunkJob.hpp"
#include "../Chunk/LoadChunkJob.hpp"
#include "../Chunk/SaveChunkJob.hpp"
#include "../Generation/TerrainGenerator.hpp"
#include "ESFWorldStorage.hpp"
#include "VoxelRaycastResult3D.hpp"
#include "../../Math/Vec3.hpp"
#include "../../Math/IntVec2.hpp"
#include "../../Core/Schedule/ScheduleSubsystem.hpp"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <deque>
#include <atomic>

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
    class World
    {
    public:
        World(const std::string& worldName, uint64_t worldSeed, std::unique_ptr<enigma::voxel::TerrainGenerator> generator);

        World() = default;
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

        // [NEW] PlaceBlock with placement context (for slab/stairs)
        void PlaceBlock(const BlockIterator&        blockIter, enigma::registry::block::Block* blockType,
                        const VoxelRaycastResult3D& raycast, const Vec3&                       playerLookDir);

        // Light Data Access (delegate to Chunk via global coordinates)
        // These methods provide global coordinate access, consistent with GetBlockState()
        uint8_t GetOutdoorLight(int32_t globalX, int32_t globalY, int32_t globalZ) const;
        uint8_t GetIndoorLight(int32_t globalX, int32_t globalY, int32_t globalZ) const;
        bool    GetIsSky(int32_t globalX, int32_t globalY, int32_t globalZ) const;

        // Chunk Operations:
        Chunk* GetChunk(int32_t chunkX, int32_t chunkY); // 改为直接访问 m_loadedChunks
        Chunk* GetChunk(int32_t chunkX, int32_t chunkY) const; // const version for const methods
        void   UnloadChunkDirect(int32_t chunkX, int32_t chunkY); // 新增直接卸载方法
        Chunk* GetChunkAt(const BlockPos& pos) const;
        bool   IsChunkLoaded(int32_t chunkX, int32_t chunkY);

        void                                     UpdateNearbyChunks(); // Update nearby blocks according to player location
        std::vector<std::pair<int32_t, int32_t>> CalculateNeededChunks() const; // Calculate the required blocks

        // Update and Management:
        void Update(float deltaTime); // Update world systems
        void SetPlayerPosition(const Vec3& position); // Update player position for chunk loading
        void SetChunkActivationRange(int chunkDistance); // Set activation range in chunks

        // Rendering:
        void Render(IRenderer* renderer); // Render world
        bool SetEnableChunkDebug(bool enable = true);

        // Utility

        bool                                                 IsChunkLoadedDirect(int32_t chunkX, int32_t chunkY) const;
        size_t                                               GetLoadedChunkCount() const;
        std::unordered_map<int64_t, std::unique_ptr<Chunk>>& GetLoadedChunks();

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

        //-------------------------------------------------------------------------------------------
        // Phase 3: Async Task Management (Multi-threaded Chunk Loading)
        //-------------------------------------------------------------------------------------------

        // Process completed tasks from ScheduleSubsystem (uses global g_theSchedule)
        // Called automatically in Update() loop on main thread
        void ProcessCompletedChunkTasks();

        //-------------------------------------------------------------------------------------------
        // Phase 2: Cross-Chunk Hidden Face Culling Support
        //-------------------------------------------------------------------------------------------

        // Mark a chunk as needing mesh rebuild and add to queue
        // Called by Chunk::NotifyNeighborsDirty() when a chunk becomes active
        void ScheduleChunkMeshRebuild(Chunk* chunk);

        //-------------------------------------------------------------------------------------------
        // Phase 7: Lighting System Support
        //-------------------------------------------------------------------------------------------

        // Get the size of the dirty light queue (for debugging and monitoring)
        size_t GetDirtyLightQueueSize() const { return m_dirtyLightQueue.size(); }

        // Mark a block as needing light recalculation and add to dirty light queue
        void MarkLightingDirty(const BlockIterator& iter);

        // Mark a block as dirty only if it is not fully opaque (convenience method)
        void MarkLightingDirtyIfNotOpaque(const BlockIterator& iter);

        // Process one dirty light block from the queue (BFS-style light propagation)
        void ProcessNextDirtyLightBlock();

        // Process all dirty lighting in a single frame (full queue processing)
        void ProcessDirtyLighting();

        // Clean dirty light queue for a specific chunk (called during chunk unload)
        void UndirtyAllBlocksInChunk(Chunk* chunk);

        // [Phase 7] Lighting Calculation - Compute Correct Light Values
        uint8_t ComputeCorrectOutdoorLight(const BlockIterator& iter) const;
        uint8_t ComputeCorrectIndoorLight(const BlockIterator& iter) const;

    private:
        //-------------------------------------------------------------------------------------------
        // Phase 3: Async Task Management Methods
        //-------------------------------------------------------------------------------------------

        // Activate a chunk (decide whether to load from disk or generate)
        void ActivateChunk(IntVec2 chunkCoords);

        // Submit chunk generation job to ScheduleSubsystem
        void SubmitGenerateChunkJob(IntVec2 chunkCoords, Chunk* chunk);

        // Submit chunk load job to ScheduleSubsystem
        void SubmitLoadChunkJob(IntVec2 chunkCoords, Chunk* chunk);

        // Submit chunk save job to ScheduleSubsystem
        void SubmitSaveChunkJob(IntVec2 chunkCoords, const Chunk* chunk);

        // Handle completed generation job
        void HandleGenerateChunkCompleted(GenerateChunkJob* job);

        // Handle completed load job
        void HandleLoadChunkCompleted(LoadChunkJob* job);

        // Handle completed save job
        void HandleSaveChunkCompleted(SaveChunkJob* job);

        //-------------------------------------------------------------------------------------------
        // Phase 1: Main Thread Mesh Building (Assignment 03 Requirements)
        //-------------------------------------------------------------------------------------------

        // Update chunk meshes on main thread (called from Update())
        void UpdateChunkMeshes();

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

        // Job limits (Assignment 03 spec: "100s of generate, only a few load/save")
        // These prevent overwhelming the thread pool and ensure responsive chunk loading
        int m_maxGenerateJobs = 512; // Allow many generation jobs (CPU-bound, parallelizable)
        int m_maxLoadJobs     = 64; // Increased for ESFS format (no file lock contention, SSD-friendly)
        int m_maxSaveJobs     = 32; // Increased for better save throughput (lower priority than load)

        //-------------------------------------------------------------------------------------------
        // Phase 1: Main Thread Mesh Building Queue (Assignment 03 Requirements)
        //-------------------------------------------------------------------------------------------
        std::deque<Chunk*> m_pendingMeshRebuildQueue; // Chunks waiting for mesh rebuild on main thread
        int                m_maxMeshRebuildsPerFrame = 2; // Maximum mesh rebuilds per frame (Assignment 03 spec)

        //-------------------------------------------------------------------------------------------
        // Phase 5: Graceful Shutdown State
        //-------------------------------------------------------------------------------------------
        std::atomic<bool> m_isShuttingDown{false}; // Shutdown flag (thread-safe)

        //-------------------------------------------------------------------------------------------
        // Phase 7: Lighting System State
        //-------------------------------------------------------------------------------------------
        std::deque<BlockIterator> m_dirtyLightQueue; // Queue of blocks that need light recalculation
    };
}
