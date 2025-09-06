#pragma once
#include "Chunk.hpp"
#include "../../Math/Vec3.hpp"
#include <unordered_map>
#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

#include "Engine/Renderer/Texture.hpp"

namespace enigma::voxel::chunk
{
    /**
     * @brief Manages loading, unloading, and lifecycle of world chunks
     * 
     * TODO: This class needs to be implemented with the following functionality:
     * 
     * CORE RESPONSIBILITIES:
     * - Chunk storage and retrieval
     * - Dynamic loading/unloading based on player position
     * - Thread management for async chunk generation
     * - Memory management to prevent unbounded chunk growth
     * 
     * REQUIRED MEMBER VARIABLES:
     * - std::unordered_map<int64_t, std::unique_ptr<Chunk>> m_loadedChunks;  // Loaded chunks by packed coordinates
     * - Vec3 m_playerPosition;                             // Current player position
     * - int32_t m_loadDistance = 8;                        // Chunks to keep loaded around player
     * - int32_t m_unloadDistance = 12;                     // Distance to start unloading chunks
     * 
     * Threading for Async Operations:
     * - std::thread m_generationThread;                    // Background thread for generation
     * - std::queue<std::pair<int32_t, int32_t>> m_generationQueue;  // Chunks waiting to be generated
     * - std::mutex m_queueMutex;                           // Protect generation queue
     * - std::condition_variable m_queueCondition;          // Signal new work
     * - std::atomic<bool> m_shouldStop{false};             // Signal thread shutdown
     * 
     * REQUIRED METHODS:
     * 
     * Chunk Access:
     * - Chunk* GetChunk(int32_t chunkX, int32_t chunkZ)
     * - bool IsChunkLoaded(int32_t chunkX, int32_t chunkZ)
     * - void LoadChunk(int32_t chunkX, int32_t chunkZ)     // Immediate load
     * - void LoadChunkAsync(int32_t chunkX, int32_t chunkZ) // Queue for async load
     * - void UnloadChunk(int32_t chunkX, int32_t chunkZ)
     * 
     * Player Position Management:
     * - void SetPlayerPosition(const Vec3& position)
     * - void UpdateChunksAroundPlayer()                    // Load/unload based on distance
     * - std::vector<std::pair<int32_t, int32_t>> GetChunksInRadius(int32_t radius)
     * 
     * Threading Management:
     * - void StartGenerationThread()
     * - void StopGenerationThread()
     * - void GenerationThreadMain()                       // Main loop for generation thread
     * - void QueueChunkGeneration(int32_t chunkX, int32_t chunkZ)
     * 
     * Memory Management:
     * - void SetLoadDistance(int32_t chunks)
     * - void SetUnloadDistance(int32_t chunks)
     * - void UnloadDistantChunks()                        // Unload chunks beyond unload distance
     * - size_t GetLoadedChunkCount() const
     * - void ForceUnloadAll()                             // Emergency unload all chunks
     * 
     * Utility:
     * - static int64_t PackCoordinates(int32_t x, int32_t z)  // Pack coords into single key
     * - static void UnpackCoordinates(int64_t packed, int32_t& x, int32_t& z)
     * - float GetDistanceToPlayer(int32_t chunkX, int32_t chunkZ)
     * 
     * Update Loop:
     * - void Update(float deltaTime)                      // Called each frame
     * - void ProcessPendingOperations()                  // Handle completed async operations
     * 
     * INTEGRATION WITH WORLD GENERATION:
     * - Should call world generator for new chunks
     * - Handle chunk population (structures, decorations)
     * - Manage dependencies between chunks (lighting, structures)
     * 
     * PERFORMANCE CONSIDERATIONS:
     * - Use spatial data structures for efficient distance queries
     * - Batch chunk operations to reduce threading overhead
     * - Consider priority queues for loading (closer chunks first)
     * - Implement chunk caching to disk for faster reloading
     * 
     * MEMORY SAFETY:
     * - Ensure thread-safe access to chunk data
     * - Prevent access to chunks during unloading
     * - Handle edge cases (player teleportation, world borders)
     */
    class ChunkManager
    {
        // TODO: Implement according to comments above
    public:
        ChunkManager();
        ~ChunkManager();

        void Initialize();

        // Chunk Access:
        Chunk* GetChunk(int32_t chunkX, int32_t chunkY);
        void   LoadChunk(int32_t chunkX, int32_t chunkY); // Immediate load

        // Utility:
        static int64_t PackCoordinates(int32_t x, int32_t y); // Pack coords into single key
        static void    UnpackCoordinates(int64_t packed, int32_t& x, int32_t& y);

        // Update Loop:
        void Update(float deltaTime); // Called each frame

        // Rendering:
        void Render(IRenderer* renderer); // Render all loadedChunks
        bool SetEnableChunkDebug(bool enable = true);

        // Resource management
        ::Texture* GetBlocksAtlasTexture() const { return m_cachedBlocksAtlasTexture; }

    private:
        std::unordered_map<int64_t, std::unique_ptr<Chunk>> m_loadedChunks; // Loaded chunks by packed coordinates
        bool                                                m_enableChunkDebug = false; // Enable debug drawing for chunks

        // Cached rendering resources (following NeoForge pattern)
        ::Texture* m_cachedBlocksAtlasTexture = nullptr; // Cached blocks atlas texture
    };
}
