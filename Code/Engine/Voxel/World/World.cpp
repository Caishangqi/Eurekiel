#include "World.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

using namespace enigma::voxel::world;

World::World(std::string worldName, uint64_t worldSeed, std::unique_ptr<ChunkManager> chunkManager) :
    m_worldName(worldName), m_worldSeed(worldSeed), m_chunkManager(std::move(chunkManager))
{
    // 设置ChunkManager的回调接口为当前World实例
    if (m_chunkManager)
    {
        m_chunkManager->SetGenerationCallback(this);
    }

    core::LogInfo("world", "World created: %s", m_worldName.c_str());
}

World::~World()
{
}

BlockState* World::GetBlockState(const BlockPos& pos)
{
    Chunk* chunk = m_chunkManager->GetChunk(pos.GetBlockXInChunk(), pos.GetBlockYInChunk());
    if (chunk)
    {
        return chunk->GetBlock(pos);
    }
    return nullptr;
}

void World::SetBlockState(const BlockPos& pos, BlockState* state) const
{
    Chunk* chunk = m_chunkManager->GetChunk(pos.GetBlockXInChunk(), pos.GetBlockYInChunk());
    if (chunk)
    {
        return chunk->SetBlockWorld(pos, state);
    }
}

bool World::IsValidPosition(const BlockPos& pos)
{
    UNUSED(pos)
    ERROR_RECOVERABLE("World::IsValidPosition is not implemented")
    return false;
}

bool World::IsBlockLoaded(const BlockPos& pos)
{
    UNUSED(pos)
    ERROR_RECOVERABLE("World::IsBlockLoaded is not implemented")
    return false;
}

Chunk* World::GetChunk(int32_t chunkX, int32_t chunkY)
{
    return m_chunkManager->GetChunk(chunkX, chunkY);
}

Chunk* World::GetChunkAt(const BlockPos& pos)
{
    return m_chunkManager->GetChunk(pos.GetBlockXInChunk(), pos.GetBlockYInChunk());
}

bool World::IsChunkLoaded(int32_t chunkX, int32_t chunkY)
{
    UNUSED(chunkX)
    UNUSED(chunkY)
    ERROR_RECOVERABLE("World::IsChunkLoaded is not implemented")
    return false;
}

// IChunkGenerationCallback 实现
void World::GenerateChunk(Chunk* chunk, int32_t chunkX, int32_t chunkY)
{
    if (chunk && m_worldGenerator && !chunk->IsGenerated())
    {
        core::LogDebug("world", "Generating content for chunk (%d, %d)", chunkX, chunkY);
        m_worldGenerator->GenerateChunk(chunk, chunkX, chunkY, static_cast<uint32_t>(m_worldSeed));
        chunk->SetGenerated(true);
    }
}

bool World::ShouldUnloadChunk(int32_t chunkX, int32_t chunkY)
{
    // 根据玩家位置计算是否应该卸载区块
    float chunkWorldX = chunkX * 16.0f + 8.0f; // 区块中心X
    float chunkWorldY = chunkY * 16.0f + 8.0f; // 区块中心Y

    float deltaX   = chunkWorldX - m_playerPosition.x;
    float deltaY   = chunkWorldY - m_playerPosition.z; // 注意：世界的Y是高度，Z是深度
    float distance = sqrt(deltaX * deltaX + deltaY * deltaY);

    // 如果距离超过激活范围 + 2，则应该卸载
    float maxDistance = (m_chunkActivationRange + 2) * 16.0f;
    return distance > maxDistance;
}

// 新的区块管理方法
void World::UpdateNearbyChunks()
{
    auto neededChunks = CalculateNeededChunks();
    if (m_chunkManager)
    {
        m_chunkManager->EnsureChunksLoaded(neededChunks);
        m_chunkManager->UnloadDistantChunks(m_playerPosition, m_chunkActivationRange + 2);
    }
}

std::vector<std::pair<int32_t, int32_t>> World::CalculateNeededChunks() const
{
    std::vector<std::pair<int32_t, int32_t>> neededChunks;

    // 计算玩家所在的区块坐标
    int32_t playerChunkX = static_cast<int32_t>(floor(m_playerPosition.x / 16.0f));
    int32_t playerChunkY = static_cast<int32_t>(floor(m_playerPosition.z / 16.0f));

    // 在激活范围内添加所有区块
    for (int32_t dx = -m_chunkActivationRange; dx <= m_chunkActivationRange; dx++)
    {
        for (int32_t dy = -m_chunkActivationRange; dy <= m_chunkActivationRange; dy++)
        {
            int32_t chunkX = playerChunkX + dx;
            int32_t chunkY = playerChunkY + dy;

            // 使用圆形加载区域而不是正方形
            float distance = (float)sqrt(dx * dx + dy * dy);
            if (distance <= m_chunkActivationRange)
            {
                neededChunks.emplace_back(chunkX, chunkY);
            }
        }
    }

    return neededChunks;
}

void World::Update(float deltaTime)
{
    m_chunkManager->Update(deltaTime);
}

void World::Render(IRenderer* renderer)
{
    m_chunkManager->Render(renderer);
}

bool World::SetEnableChunkDebug(bool enable)
{
    return m_chunkManager->SetEnableChunkDebug(enable);
}

std::unique_ptr<ChunkManager>& World::GetChunkManager()
{
    return m_chunkManager;
}

void World::SetPlayerPosition(const Vec3& position)
{
    m_playerPosition = position;
    // Sync player position to ChunkManager for distance-based chunk loading
    if (m_chunkManager)
    {
        m_chunkManager->SetPlayerPosition(position);
    }
}

void World::SetChunkActivationRange(int chunkDistance)
{
    m_chunkActivationRange = chunkDistance;
    // Sync activation range to ChunkManager
    if (m_chunkManager)
    {
        m_chunkManager->SetActivationRange(chunkDistance);
    }
    core::LogInfo("world", "Set chunk activation range to %d chunks", chunkDistance);
}

void World::SetWorldGenerator(std::unique_ptr<enigma::voxel::generation::Generator> generator)
{
    m_worldGenerator = std::move(generator);
    if (m_worldGenerator)
    {
        // Initialize the generator with world seed
        m_worldGenerator->Initialize(static_cast<uint32_t>(m_worldSeed));
        core::LogInfo("world", "World generator set and initialized for world '%s'", m_worldName.c_str());
    }
}

// 预留：序列化配置接口（空实现）
void World::SetChunkSerializer(std::unique_ptr<IChunkSerializer> serializer)
{
    m_chunkSerializer = std::move(serializer);
    if (m_chunkManager && m_chunkSerializer)
    {
        // 为ChunkManager创建一个新的序列化器实例（这里只是示例，实际需要克隆机制）
        // TODO: 实现序列化器的克隆或共享机制
        core::LogInfo("world", "Chunk serializer passed to ChunkManager for world '%s'", m_worldName.c_str());
    }
    core::LogInfo("world", "Chunk serializer configured for world '%s'", m_worldName.c_str());
}

void World::SetChunkStorage(std::unique_ptr<IChunkStorage> storage)
{
    m_chunkStorage = std::move(storage);
    if (m_chunkManager && m_chunkStorage)
    {
        // 为ChunkManager创建一个新的存储实例（这里只是示例，实际需要克隆机制）
        // TODO: 实现存储的克隆或共享机制
        core::LogInfo("world", "Chunk storage passed to ChunkManager for world '%s'", m_worldName.c_str());
    }
    core::LogInfo("world", "Chunk storage configured for world '%s'", m_worldName.c_str());
}
