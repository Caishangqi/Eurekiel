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
        BlockState* GetTopBlock(const BlockPos& pos);
        BlockState* GetTopBlock(int32_t x, int32_t y);
        int         GetTopBlockZ(const BlockPos& pos);

        // Chunk Operations:
        Chunk* GetChunk(int32_t chunkX, int32_t chunkY);
        Chunk* GetChunkAt(const BlockPos& pos) const;
        bool   IsChunkLoaded(int32_t chunkX, int32_t chunkY);

        // 新的区块管理方法（取代LoadChunk/UnloadChunk）
        void                                     UpdateNearbyChunks(); // 根据玩家位置更新附近区块
        std::vector<std::pair<int32_t, int32_t>> CalculateNeededChunks() const; // 计算需要的区块

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

        // 预留：序列化配置接口
        void SetChunkSerializer(std::unique_ptr<IChunkSerializer> serializer);
        void SetChunkStorage(std::unique_ptr<IChunkStorage> storage);

        // 世界文件管理接口
        bool InitializeWorldStorage(const std::string& savesPath);
        bool SaveWorld();
        bool LoadWorld();
        void CloseWorld();

        // 获取世界信息
        const std::string& GetWorldName() const { return m_worldName; }
        uint64_t           GetWorldSeed() const { return m_worldSeed; }
        const std::string& GetWorldPath() const { return m_worldPath; }

    private:
        std::unique_ptr<ChunkManager> m_chunkManager; // Manages all chunks
        int32_t                       m_worldHeight = 128; // World height in blocks
        int32_t                       m_minY        = -64; // Minimum Y coordinate
        int32_t                       m_maxY        = 320; // Maximum Y coordinate
        std::string                   m_worldName; // World identifier
        std::string                   m_worldPath; // World storage path
        uint64_t                      m_worldSeed = 0; // World generation seed

        // Player position and chunk management
        Vec3    m_playerPosition{0.0f, 0.0f, 128.0f}; // Current player position
        int32_t m_chunkActivationRange = 12; // Activation range in chunks (from settings.yml)

        // World generation
        std::unique_ptr<enigma::voxel::generation::Generator> m_worldGenerator;

        // 预留：序列化组件（可选，暂时为nullptr）
        std::unique_ptr<IChunkSerializer> m_chunkSerializer;
        std::unique_ptr<IChunkStorage>    m_chunkStorage;

        // 世界文件管理器
        std::unique_ptr<ESFWorldManager> m_worldManager;
    };
}
