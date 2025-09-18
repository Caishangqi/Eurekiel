#pragma once
#include "../Block/BlockState.hpp"
#include "../Block/BlockPos.hpp"
#include "../Chunk/ChunkManager.hpp"
#include "../Generation/Generator.hpp"
#include "../../Math/Vec3.hpp"
#include <memory>
#include <unordered_map>

namespace enigma::voxel::world
{
    using namespace enigma::voxel::block;
    using namespace enigma::voxel::chunk;

    /**
     * @brief Main world class - manages the voxel world and its chunks
     * 
     * TODO: This class needs to be implemented with the following functionality:
     * 
     * CORE FUNCTIONALITY:
     * - Block management: SetBlockState(), GetBlockState(), GetBlock()
     * - Coordinate system: World coordinates to chunk coordinates conversion
     * - Chunk loading/unloading: Dynamic chunk loading based on player position
     * - World bounds: Define world limits (y-level limits, world border)
     * 
     * REQUIRED MEMBER VARIABLES:
     * - std::unique_ptr<ChunkManager> m_chunkManager;  // Manages all chunks
     * - int32_t m_worldHeight = 256;                   // World height in blocks
     * - int32_t m_minY = -64;                          // Minimum Y coordinate
     * - int32_t m_maxY = 320;                          // Maximum Y coordinate
     * - std::string m_worldName;                       // World identifier
     * - uint64_t m_worldSeed = 0;                      // World generation seed
     * 
     * REQUIRED METHODS:
     * 
     * Block Operations:
     * - BlockState* GetBlockState(const BlockPos& pos)
     * - void SetBlockState(const BlockPos& pos, BlockState* state)
     * - bool IsValidPosition(const BlockPos& pos)
     * - bool IsBlockLoaded(const BlockPos& pos)
     * 
     * Chunk Operations:
     * - Chunk* GetChunk(int32_t chunkX, int32_t chunkZ)
     * - Chunk* GetChunkAt(const BlockPos& pos)
     * - bool IsChunkLoaded(int32_t chunkX, int32_t chunkZ)
     * - void LoadChunk(int32_t chunkX, int32_t chunkZ)
     * - void UnloadChunk(int32_t chunkX, int32_t chunkZ)
     * 
     * Update and Management:
     * - void Update(float deltaTime)  // Update world systems
     * - void SetLoadDistance(int32_t chunks)  // How many chunks to keep loaded around player
     * - void SetPlayerPosition(const Vec3& pos)  // Update player position for chunk loading
     * 
     * World Generation Integration:
     * - void GenerateChunk(int32_t chunkX, int32_t chunkZ)  // Generate new chunk
     * - void PopulateChunk(Chunk* chunk)  // Add decorations, structures
     * 
     * Persistence (Save/Load):
     * - bool SaveChunk(Chunk* chunk)
     * - bool LoadChunkFromDisk(int32_t chunkX, int32_t chunkZ)
     * 
     * INTEGRATION NOTES:
     * - Should work with ChunkManager for chunk lifecycle management
     * - Needs integration with world generation system (when implemented)
     * - Should handle block updates and neighbor notifications
     * - Needs thread safety for async chunk loading/generation
     * - Should integrate with rendering system for frustum culling
     */
    class World
    {
        // TODO: Implement according to comments above
    public:
        explicit World(std::string worldName, uint64_t worldSeed, std::unique_ptr<ChunkManager> chunkManager);
        World() = default;
        ~World();

        // Block Operations:
        BlockState* GetBlockState(const BlockPos& pos);
        void        SetBlockState(const BlockPos& pos, BlockState* state) const;
        bool        IsValidPosition(const BlockPos& pos);
        bool        IsBlockLoaded(const BlockPos& pos);

        // Chunk Operations:
        Chunk* GetChunk(int32_t chunkX, int32_t chunkY);
        Chunk* GetChunkAt(const BlockPos& pos);
        bool   IsChunkLoaded(int32_t chunkX, int32_t chunkY);
        void   LoadChunk(int32_t chunkX, int32_t chunkY);
        void   UnloadChunk(int32_t chunkX, int32_t chunkY);

        // Update and Management:
        void Update(float deltaTime); // Update world systems
        void SetPlayerPosition(const Vec3& position); // Update player position for chunk loading
        void SetChunkActivationRange(int chunkDistance); // Set activation range in chunks

        // Rendering:
        void Render(IRenderer* renderer); // Render world
        bool SetEnableChunkDebug(bool enable = true);

        // Utility
        std::unique_ptr<ChunkManager>& GetChunkManager();

        // World generation integration
        void SetWorldGenerator(std::unique_ptr<enigma::voxel::generation::Generator> generator);

    private:
        std::unique_ptr<ChunkManager> m_chunkManager; // Manages all chunks
        int32_t                       m_worldHeight = 128; // World height in blocks
        int32_t                       m_minY        = -64; // Minimum Y coordinate
        int32_t                       m_maxY        = 320; // Maximum Y coordinate
        std::string                   m_worldName; // World identifier
        uint64_t                      m_worldSeed = 0; // World generation seed

        // Player position and chunk management
        Vec3    m_playerPosition{0.0f, 0.0f, 128.0f}; // Current player position
        int32_t m_chunkActivationRange = 12; // Activation range in chunks (from settings.yml)

        // World generation
        std::unique_ptr<enigma::voxel::generation::Generator> m_worldGenerator;
    };
}
