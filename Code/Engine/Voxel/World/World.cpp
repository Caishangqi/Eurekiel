#include "World.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

using namespace enigma::voxel::world;

World::World(std::string worldName, uint64_t worldSeed, std::unique_ptr<ChunkManager> chunkManager) :
    m_worldName(worldName), m_worldSeed(worldSeed), m_chunkManager(std::move(chunkManager))
{
    // Set the callback interface of ChunkManager to the current World instance
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
    Chunk* chunk = GetChunkAt(pos);
    if (chunk)
    {
        return chunk->GetBlock(pos);
    }
    return nullptr;
}

void World::SetBlockState(const BlockPos& pos, BlockState* state) const
{
    Chunk* chunk = GetChunkAt(pos);
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

BlockState* World::GetTopBlock(const BlockPos& pos)
{
    Chunk* chunk = m_chunkManager->GetChunk(pos.GetBlockXInChunk(), pos.GetBlockYInChunk());
    if (chunk)
    {
        return chunk->GetTopBlock(pos);
    }
    return nullptr;
}

BlockState* World::GetTopBlock(int32_t x, int32_t y)
{
    UNUSED(x)
    UNUSED(y)
    ERROR_RECOVERABLE("World::GetTopBlock is not implemented")
    return nullptr;
}

int World::GetTopBlockZ(const BlockPos& pos)
{
    Chunk* chunk = GetChunkAt(pos);
    if (chunk)
    {
        return chunk->GetTopBlockZ(pos);
    }
    return -1;
}

Chunk* World::GetChunk(int32_t chunkX, int32_t chunkY)
{
    return m_chunkManager->GetChunk(chunkX, chunkY);
}

Chunk* World::GetChunkAt(const BlockPos& pos) const
{
    return m_chunkManager->GetChunk(pos.GetChunkX(), pos.GetChunkY());
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
    // Calculate whether the block should be uninstalled based on the player's location
    float chunkWorldX = chunkX * 16.0f + 8.0f; // Block Center X
    float chunkWorldY = chunkY * 16.0f + 8.0f; // Block Center Y

    float deltaX   = chunkWorldX - m_playerPosition.x;
    float deltaY   = chunkWorldY - m_playerPosition.y;
    float distance = sqrt(deltaX * deltaX + deltaY * deltaY);

    // If the distance exceeds the activation range + 2, it should be uninstalled
    float maxDistance = (float)(m_chunkActivationRange + 2) * 16.0f;
    return distance > maxDistance;
}

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

    // Calculate the block coordinates of the player
    int32_t playerChunkX = static_cast<int32_t>(floor(m_playerPosition.x / 16.0f));
    int32_t playerChunkY = static_cast<int32_t>(floor(m_playerPosition.z / 16.0f));

    // Add all blocks within the activation range
    for (int32_t dx = -m_chunkActivationRange; dx <= m_chunkActivationRange; dx++)
    {
        for (int32_t dy = -m_chunkActivationRange; dy <= m_chunkActivationRange; dy++)
        {
            int32_t chunkX = playerChunkX + dx;
            int32_t chunkY = playerChunkY + dy;

            // Use circle to load areas instead of squares
            float distance = (float)sqrt(dx * dx + dy * dy);
            if (distance <= (float)m_chunkActivationRange)
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

// Reserved: Serialized configuration interface (empty implementation)
void World::SetChunkSerializer(std::unique_ptr<IChunkSerializer> serializer)
{
    m_chunkSerializer = std::move(serializer);
    if (m_chunkManager && m_chunkSerializer)
    {
        // Create a new serializer instance for ChunkManager (just an example here, you actually need a cloning mechanism)
        // TODO: Implementing the cloning or sharing mechanism of the serializer
        core::LogInfo("world", "Chunk serializer passed to ChunkManager for world '%s'", m_worldName.c_str());
    }
    core::LogInfo("world", "Chunk serializer configured for world '%s'", m_worldName.c_str());
}

void World::SetChunkStorage(std::unique_ptr<IChunkStorage> storage)
{
    m_chunkStorage = std::move(storage);
    if (m_chunkManager && m_chunkStorage)
    {
        // Create a new storage instance for ChunkManager (just an example here, you actually need a cloning mechanism)
        // TODO: Implement storage cloning or sharing mechanism
        core::LogInfo("world", "Chunk storage passed to ChunkManager for world '%s'", m_worldName.c_str());
    }
    core::LogInfo("world", "Chunk storage configured for world '%s'", m_worldName.c_str());
}
