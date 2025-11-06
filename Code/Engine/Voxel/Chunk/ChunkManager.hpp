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

#include "Engine/Core/LogCategory/LogCategory.hpp"
#include "Engine/Renderer/Texture.hpp"

DECLARE_LOG_CATEGORY_EXTERN(LogChunk)

namespace enigma::voxel
{
    /**
         * @brief Manages loading, unloading, and lifecycle of world chunks
         *
         * Reconstructed ChunkManager Responsibilities:
         * - Block storage, life cycle and memory management
         * - Automatic block management based on distance
         * - Block memory pool and cache management
         * - Scheduling of asynchronous block operations
         * - Block status query and statistics
         * - Optional block serialization and persistence support
         */
    class ChunkManager
    {
        friend class enigma::voxel::World;
        // TODO: Implement according to comments above
    public:
        explicit ChunkManager(IChunkGenerationCallback* callback = nullptr);
        ~ChunkManager();

        void Initialize();

        // Chunk Access and Management:
        Chunk* GetChunk(int32_t chunkX, int32_t chunkY);
        bool   IsChunkLoaded(int32_t chunkX, int32_t chunkY) const;
        void   LoadChunk(int32_t chunkX, int32_t chunkY); // Immediate load
        void   UnloadChunk(int32_t chunkX, int32_t chunkY); // Unload chunk

        void EnsureChunksLoaded(const std::vector<std::pair<int32_t, int32_t>>& chunks);
        void UnloadDistantChunks(const Vec3& playerPos, int32_t maxDistance);

        // Set up the callback interface
        void SetGenerationCallback(IChunkGenerationCallback* callback) { m_generationCallback = callback; }

        // Player position and distance-based management
        void SetPlayerPosition(const Vec3& playerPosition);
        void SetActivationRange(int32_t chunkDistance); // Distance in chunks
        void SetDeactivationRange(int32_t chunkDistance); // Distance in chunks

        void                   SetChunkSerializer(std::unique_ptr<IChunkSerializer> serializer);
        void                   SetChunkStorage(std::unique_ptr<IChunkStorage> storage);
        bool                   SaveChunkToDisk(const Chunk* chunk);
        std::unique_ptr<Chunk> LoadChunkFromDisk(int32_t chunkX, int32_t chunkY);
        size_t                 SaveAllModifiedChunks(); // Save all modified chunks to disk
        void                   FlushStorage(); // Flush storage buffers

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
        Texture* GetBlocksAtlasTexture() const { return m_cachedBlocksAtlasTexture; }

        // Statistics and debugging
        size_t GetLoadedChunkCount() const { return m_loadedChunks.size(); }

        // Delayed deletion management
        void   MarkChunkForDeletion(Chunk* chunk); // Mark chunk for delayed deletion
        void   ProcessPendingDeletions(); // Process delayed deletion queue
        size_t GetPendingDeletionCount() const; // Get pending deletion count (debugging)

    private:
        std::unordered_map<int64_t, std::unique_ptr<Chunk>> m_loadedChunks; // Loaded chunks by packed coordinates
        std::vector<Chunk*>                                 m_pendingDeleteChunks; // Chunks pending delayed deletion
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

        // Mesh rebuild limits (performance tuning)
        int m_maxMeshRebuildsPerFrame = 1; // Max mesh rebuilds per frame (increase for better initial loading)

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
