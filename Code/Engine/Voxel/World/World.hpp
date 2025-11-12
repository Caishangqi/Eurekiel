#pragma once
#include "../Block/BlockState.hpp"
#include "../Block/BlockPos.hpp"

#include "../Chunk/ChunkSerializationInterfaces.hpp"
#include "../Chunk/ChunkJob.hpp"
#include "../Chunk/GenerateChunkJob.hpp"
#include "../Chunk/LoadChunkJob.hpp"
#include "../Chunk/SaveChunkJob.hpp"
#include "../Generation/TerrainGenerator.hpp"
#include "ESFWorldStorage.hpp"
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
    class BuildMeshJob;
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

        // Chunk Operations:
        Chunk* GetChunk(int32_t chunkX, int32_t chunkY); // 改为直接访问 m_loadedChunks
        Chunk* GetChunk(int32_t chunkX, int32_t chunkY) const; // const version for const methods
        void   LoadChunkDirect(int32_t chunkX, int32_t chunkY); // 新增直接加载方法
        void   UnloadChunkDirect(int32_t chunkX, int32_t chunkY); // 新增直接卸载方法
        Chunk* GetChunkAt(const BlockPos& pos) const;
        bool   IsChunkLoaded(int32_t chunkX, int32_t chunkY);

        void                                     UpdateNearbyChunks(); // Update nearby blocks according to player location
        std::vector<std::pair<int32_t, int32_t>> CalculateNeededChunks() const; // Calculate the required blocks

        // 区块生成和管理方法
        void GenerateChunk(Chunk* chunk, int32_t chunkX, int32_t chunkY);
        bool ShouldUnloadChunk(int32_t chunkX, int32_t chunkY);

        // Update and Management:
        void Update(float deltaTime); // Update world systems
        void SetPlayerPosition(const Vec3& position); // Update player position for chunk loading
        void SetChunkActivationRange(int chunkDistance); // Set activation range in chunks

        // Rendering:
        void Render(IRenderer* renderer); // Render world
        bool SetEnableChunkDebug(bool enable = true);

        // Utility

        // 区块访问和管理（Direct 版本，直接访问 World 数据）
        Chunk*                                               GetChunkDirect(int32_t chunkX, int32_t chunkY);
        bool                                                 IsChunkLoadedDirect(int32_t chunkX, int32_t chunkY) const;
        size_t                                               GetLoadedChunkCount() const;
        std::unordered_map<int64_t, std::unique_ptr<Chunk>>& GetLoadedChunks();

        // 调试功能
        bool SetEnableChunkDebugDirect(bool enable = true);

        // 渲染资源
        Texture* GetBlocksAtlasTexture() const;

        // 延迟删除管理
        void   MarkChunkForDeletion(Chunk* chunk);
        void   ProcessPendingDeletions();
        size_t GetPendingDeletionCount() const;

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

    private:
        //-------------------------------------------------------------------------------------------
        // Phase 3: Async Task Management Methods
        //-------------------------------------------------------------------------------------------

        // Activate a chunk (decide whether to load from disk or generate)
        void ActivateChunk(IntVec2 chunkCoords);

        // Deactivate and save a chunk (if modified)
        void DeactivateChunk(IntVec2 chunkCoords);

        // Submit chunk generation job to ScheduleSubsystem
        void SubmitGenerateChunkJob(IntVec2 chunkCoords, Chunk* chunk);

        // Submit chunk load job to ScheduleSubsystem
        void SubmitLoadChunkJob(IntVec2 chunkCoords, Chunk* chunk);

        // Submit chunk save job to ScheduleSubsystem
        void SubmitSaveChunkJob(IntVec2 chunkCoords, const Chunk* chunk);

        // Submit mesh build job to ScheduleSubsystem
        void SubmitBuildMeshJob(Chunk* chunk, TaskPriority priority = TaskPriority::Normal);

        // Handle completed generation job
        void HandleGenerateChunkCompleted(GenerateChunkJob* job);

        // Handle completed load job
        void HandleLoadChunkCompleted(LoadChunkJob* job);

        // Handle completed save job
        void HandleSaveChunkCompleted(SaveChunkJob* job);

        // Handle completed mesh build job
        void HandleBuildMeshCompleted(BuildMeshJob* job);

        //-------------------------------------------------------------------------------------------
        // Phase 4: Job Queue Management Methods
        //-------------------------------------------------------------------------------------------

        // Process pending job queues and submit jobs when under limit
        // Called from Update() every frame to enforce job limits
        void ProcessJobQueues();

        // Remove jobs from pending queues that are beyond activation range
        // Called from Update() to cancel jobs for chunks that moved out of range
        void RemoveDistantJobs();

        //-------------------------------------------------------------------------------------------
        // 区块距离和管理辅助方法（从 ChunkManager 迁移）
        //-------------------------------------------------------------------------------------------

        // 计算区块到玩家的距离
        float GetChunkDistanceToPlayer(int32_t chunkX, int32_t chunkY) const;

        // 获取激活范围内的所有区块坐标
        std::vector<std::pair<int32_t, int32_t>> GetChunksInActivationRange() const;

        // 查找距离玩家最远的已加载区块
        std::pair<int32_t, int32_t> FindFarthestChunk() const;

        // 查找距离玩家最近的未加载区块（在激活范围内）
        std::pair<int32_t, int32_t> FindNearestMissingChunk() const;

        // 查找距离玩家最近的需要重建网格的区块
        Chunk* FindNearestDirtyChunk() const;

    private:
        // 从 ChunkManager 迁移的成员
        std::unordered_map<int64_t, std::unique_ptr<Chunk>> m_loadedChunks;
        std::vector<Chunk*>                                 m_pendingDeleteChunks;
        bool                                                m_enableChunkDebug         = false;
        int32_t                                             m_activationRange          = 12;
        int32_t                                             m_deactivationRange        = 14;
        int                                                 m_maxMeshRebuildsPerFrame  = 1;
        Texture*                                            m_cachedBlocksAtlasTexture = nullptr;

        std::string m_worldName; // World identifier
        std::string m_worldPath; // World storage path
        uint64_t    m_worldSeed = 0; // World generation seed

        // Player position and chunk management
        Vec3    m_playerPosition{0.0f, 0.0f, 256.0f}; // Current player position
        int32_t m_chunkActivationRange = 16; // Activation range in chunks (from settings.yml)

        // World generation
        std::unique_ptr<enigma::voxel::TerrainGenerator> m_worldGenerator;

        // Serialize components
        std::unique_ptr<IChunkSerializer> m_chunkSerializer;
        std::unique_ptr<IChunkStorage>    m_chunkStorage;

        // World File Manager
        std::unique_ptr<ESFWorldManager> m_worldManager;

        //-------------------------------------------------------------------------------------------
        // Phase 3: Async Task Management State
        //-------------------------------------------------------------------------------------------

        // Track chunks in different async states (for debugging and management)
        // These sets track chunk coordinates that have pending async operations
        std::unordered_set<int64_t> m_chunksWithPendingLoad; // Chunks with pending Load jobs
        std::unordered_set<int64_t> m_chunksWithPendingGenerate; // Chunks with pending Generate jobs
        std::unordered_set<int64_t> m_chunksWithPendingSave; // Chunks with pending Save jobs

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
        std::atomic<int> m_activeMeshBuildJobs{0}; // Currently processing mesh build jobs

        // Job limits (Assignment 03 spec: "100s of generate, only a few load/save")
        // These prevent overwhelming the thread pool and ensure responsive chunk loading
        int m_maxGenerateJobs  = 1024; // Allow many generation jobs (CPU-bound, parallelizable)
        int m_maxLoadJobs      = 512; // Increased for ESFS format (no file lock contention, SSD-friendly)
        int m_maxSaveJobs      = 512; // Increased for better save throughput (lower priority than load)
        int m_maxMeshBuildJobs = 1024; // Async mesh building jobs (CPU-bound, player interaction needs fast response)

        //-------------------------------------------------------------------------------------------
        // Phase 5: Graceful Shutdown State
        //-------------------------------------------------------------------------------------------
        std::atomic<bool> m_isShuttingDown{false}; // Shutdown flag (thread-safe)
    };
}
