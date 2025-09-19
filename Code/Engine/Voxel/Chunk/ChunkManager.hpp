#pragma once
#include "Chunk.hpp"
#include "IChunkGenerationCallback.hpp"
#include "ChunkSerializationInterfaces.hpp"
#include "../../Math/Vec3.hpp"
#include <unordered_map>
#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>

#include "Engine/Renderer/Texture.hpp"

namespace enigma::voxel::chunk
{
    /**
     * @brief Manages loading, unloading, and lifecycle of world chunks
     *
     * 重构后的ChunkManager职责：
     * - 区块的存储、生命周期和内存管理
     * - 基于距离的自动区块管理
     * - 区块内存池和缓存管理
     * - 异步区块操作的调度
     * - 区块状态查询和统计
     * - 可选的区块序列化和持久化支持
     */
    class ChunkManager
    {
        friend class enigma::voxel::world::World;
        // TODO: Implement according to comments above
    public:
        // 新的构造函数：接受回调接口
        explicit ChunkManager(IChunkGenerationCallback* callback = nullptr);
        ~ChunkManager();

        void Initialize();

        // Chunk Access and Management:
        Chunk* GetChunk(int32_t chunkX, int32_t chunkY);
        bool   IsChunkLoaded(int32_t chunkX, int32_t chunkY) const;
        void   LoadChunk(int32_t chunkX, int32_t chunkY); // Immediate load
        void   UnloadChunk(int32_t chunkX, int32_t chunkY); // Unload chunk

        // 新增：批量区块管理
        void EnsureChunksLoaded(const std::vector<std::pair<int32_t, int32_t>>& chunks);
        void UnloadDistantChunks(const Vec3& playerPos, int32_t maxDistance);

        // 设置生成回调接口
        void SetGenerationCallback(IChunkGenerationCallback* callback) { m_generationCallback = callback; }

        // Player position and distance-based management
        void SetPlayerPosition(const Vec3& playerPosition);
        void SetActivationRange(int32_t chunkDistance); // Distance in chunks
        void SetDeactivationRange(int32_t chunkDistance); // Distance in chunks
        void UpdateChunkActivation(); // Called each frame to manage chunks

        // 预留：序列化组件设置（可选功能）
        void                   SetChunkSerializer(std::unique_ptr<IChunkSerializer> serializer);
        void                   SetChunkStorage(std::unique_ptr<IChunkStorage> storage);
        bool                   SaveChunkToDisk(const Chunk* chunk); // 暂时返回false
        std::unique_ptr<Chunk> LoadChunkFromDisk(int32_t chunkX, int32_t chunkY); // 暂时返回nullptr

        // Utility:
        static int64_t                                       PackCoordinates(int32_t x, int32_t y); // Pack coords into single key
        static void                                          UnpackCoordinates(int64_t packed, int32_t& x, int32_t& y);
        std::unordered_map<int64_t, std::unique_ptr<Chunk>>& GetLoadedChunks() { return m_loadedChunks; }

        // Update Loop:
        void Update(float deltaTime); // Called each frame

        // Rendering:
        void Render(IRenderer* renderer); // Render all loadedChunks
        bool SetEnableChunkDebug(bool enable = true);

        // Resource management
        ::Texture* GetBlocksAtlasTexture() const { return m_cachedBlocksAtlasTexture; }

        // Statistics and debugging
        size_t GetLoadedChunkCount() const { return m_loadedChunks.size(); }

    private:
        std::unordered_map<int64_t, std::unique_ptr<Chunk>> m_loadedChunks; // Loaded chunks by packed coordinates
        bool                                                m_enableChunkDebug = false; // Enable debug drawing for chunks

        // Player position and chunk management (using BlockPos for consistency)
        Vec3    m_playerPosition{0.0f, 0.0f, 128.0f}; // Current player position
        int32_t m_activationRange   = 12; // Activation range in chunks (from renderDistance)
        int32_t m_deactivationRange = 14; // Deactivation range in chunks

        // Callback interface (replace m_ownerWorld)
        IChunkGenerationCallback* m_generationCallback = nullptr;

        // Reserved: Serialized component (optional, temporarily nullptr)
        std::unique_ptr<IChunkSerializer> m_chunkSerializer;
        std::unique_ptr<IChunkStorage>    m_chunkStorage;

        // Frame-limited operations tracking
        enum class ChunkOperationType
        {
            None,
            CheckDirtyChunks,
            ActivateChunk,
            DeactivateChunk
        };

        ChunkOperationType m_lastFrameOperation = ChunkOperationType::None;

        // Cached rendering resources (following NeoForge pattern)
        ::Texture* m_cachedBlocksAtlasTexture = nullptr; // Cached blocks atlas texture

        // Helper methods for distance-based management
        float                                    GetChunkDistanceToPlayer(int32_t chunkX, int32_t chunkY) const;
        std::vector<std::pair<int32_t, int32_t>> GetChunksInActivationRange() const;
        std::pair<int32_t, int32_t>              FindFarthestChunk() const;
        std::pair<int32_t, int32_t>              FindNearestMissingChunk() const;
        Chunk*                                   FindNearestDirtyChunk() const;
    };
}
