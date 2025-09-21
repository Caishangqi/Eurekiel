#pragma once
#include "../Block/BlockState.hpp"
#include "../Block/BlockPos.hpp"
#include "../Chunk/ChunkManager.hpp"
#include "../Chunk/IChunkGenerationCallback.hpp"
#include "../Chunk/ChunkSerializationInterfaces.hpp"
#include "../Generation/Generator.hpp"
#include "ESFWorldStorage.hpp"
#include "../../Math/Vec3.hpp"
#include <memory>
#include <unordered_map>
#include <vector>

namespace enigma::voxel::world
{
    // ========================================================================
    // Core Configuration
    // ========================================================================
    constexpr char WORLD_SAVE_PATH[] = ".enigma/saves";

    using namespace enigma::voxel::block;
    using namespace enigma::voxel::chunk;

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
    class World : public IChunkGenerationCallback
    {
    public:
        World(const std::string& worldName, uint64_t worldSeed, std::unique_ptr<enigma::voxel::generation::Generator> generator);

        World() = default;
        ~World() override;

        // Block Operations:
        BlockState* GetBlockState(const BlockPos& pos);
        void        SetBlockState(const BlockPos& pos, BlockState* state) const;
        bool        IsValidPosition(const BlockPos& pos);
        bool        IsBlockLoaded(const BlockPos& pos);
        BlockState* GetTopBlock(const BlockPos& pos);
        BlockState* GetTopBlock(int32_t x, int32_t y);
        int         GetTopBlockZ(const BlockPos& pos);

        // Chunk Operations:
        Chunk* GetChunk(int32_t chunkX, int32_t chunkY);
        Chunk* GetChunkAt(const BlockPos& pos) const;
        bool   IsChunkLoaded(int32_t chunkX, int32_t chunkY);

        void                                     UpdateNearbyChunks(); // Update nearby blocks according to player location
        std::vector<std::pair<int32_t, int32_t>> CalculateNeededChunks() const; // Calculate the required blocks

        // IChunkGenerationCallback 实现
        void GenerateChunk(Chunk* chunk, int32_t chunkX, int32_t chunkY) override;
        bool ShouldUnloadChunk(int32_t chunkX, int32_t chunkY) override;

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

        // Serialize the configuration interface
        void SetChunkSerializer(std::unique_ptr<IChunkSerializer> serializer);
        void SetChunkStorage(std::unique_ptr<IChunkStorage> storage);

        // World file management interface
        bool InitializeWorldStorage(const std::string& savesPath);
        bool SaveWorld();
        bool LoadWorld();
        void CloseWorld();

        // Get world information
        const std::string& GetWorldName() const { return m_worldName; }
        uint64_t           GetWorldSeed() const { return m_worldSeed; }
        const std::string& GetWorldPath() const { return m_worldPath; }

    private:
        std::unique_ptr<ChunkManager> m_chunkManager; // Manages all chunks
        std::string                   m_worldName; // World identifier
        std::string                   m_worldPath; // World storage path
        uint64_t                      m_worldSeed = 0; // World generation seed

        // Player position and chunk management
        Vec3    m_playerPosition{0.0f, 0.0f, 128.0f}; // Current player position
        int32_t m_chunkActivationRange = 12; // Activation range in chunks (from settings.yml)

        // World generation
        std::unique_ptr<enigma::voxel::generation::Generator> m_worldGenerator;

        // Serialize components
        std::unique_ptr<IChunkSerializer> m_chunkSerializer;
        std::unique_ptr<IChunkStorage>    m_chunkStorage;

        // World File Manager
        std::unique_ptr<ESFWorldManager> m_worldManager;
    };
}
