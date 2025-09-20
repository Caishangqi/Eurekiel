#pragma once
#include "../Block/BlockState.hpp"
#include "../Block/BlockPos.hpp"
#include "../Chunk/ChunkManager.hpp"
#include "../Chunk/IChunkGenerationCallback.hpp"
#include "../Chunk/ChunkSerializationInterfaces.hpp"
#include "../Generation/Generator.hpp"
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
     * 重构后的World类职责：
     * - 世界级别的逻辑管理和协调
     * - 方块状态的获取和设置（通过区块代理）
     * - 世界级别的配置管理（世界名、种子、高度限制）
     * - 玩家位置跟踪和区块需求计算
     * - 世界生成器的集成和调度
     * - 渲染和更新的统一入口
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

        // 预留：序列化配置接口（空实现）
        void SetChunkSerializer(std::unique_ptr<IChunkSerializer> serializer);
        void SetChunkStorage(std::unique_ptr<IChunkStorage> storage);

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

        // 预留：序列化组件（可选，暂时为nullptr）
        std::unique_ptr<IChunkSerializer> m_chunkSerializer;
        std::unique_ptr<IChunkStorage>    m_chunkStorage;
    };
}
