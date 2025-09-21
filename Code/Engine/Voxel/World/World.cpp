#include "World.hpp"
#include "ESFWorldStorage.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

using namespace enigma::voxel::world;

World::~World()
{
}

World::World(const std::string& worldName, uint64_t worldSeed, std::unique_ptr<enigma::voxel::generation::Generator> generator) : m_worldName(worldName), m_worldSeed(worldSeed)
{
    using namespace enigma::voxel::chunk;

    // Create and initialize ChunkManager (follow NeoForge mode)
    m_chunkManager = std::make_unique<ChunkManager>(this);
    m_chunkManager->Initialize();

    // Initialize the ESF storage system
    if (!InitializeWorldStorage(WORLD_SAVE_PATH))
    {
        core::LogError("world", "Failed to initialize world storage system");
        ERROR_AND_DIE("Failed to initialize world storage system")
    }
    core::LogInfo("world", "World storage system initialized in: %s", WORLD_SAVE_PATH);

    // Set up the world generator
    SetWorldGenerator(std::move(generator));
    core::LogInfo("world", "World fully initialized with name: %s", m_worldName.c_str());
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
        return chunk->SetBlockWorldByPlayer(pos, state);
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

// IChunkGenerationCallback implementation
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

/**
 * Sets the chunk serializer for the world. This serializer is responsible for
 * handling the serialization and deserialization of chunks. If both the chunk
 * manager and the provided serializer are valid, the serializer is passed to
 * the chunk manager as well.
 *
 * @param serializer A unique pointer to the chunk serializer to be configured
 *                   for the world.
 */
void World::SetChunkSerializer(std::unique_ptr<IChunkSerializer> serializer)
{
    m_chunkSerializer = std::move(serializer);
    if (m_chunkManager && m_chunkSerializer)
    {
        // Pass the serializer to ChunkManager
        std::unique_ptr<IChunkSerializer> serializerCopy = std::make_unique<ESFChunkSerializer>();
        m_chunkManager->SetChunkSerializer(std::move(serializerCopy));
        core::LogInfo("world", "Chunk serializer passed to ChunkManager for world '%s'", m_worldName.c_str());
    }
    core::LogInfo("world", "Chunk serializer configured for world '%s'", m_worldName.c_str());
}

void World::SetChunkStorage(std::unique_ptr<IChunkStorage> storage)
{
    m_chunkStorage = std::move(storage);
    if (m_chunkManager && m_chunkStorage)
    {
        // Pass memory to ChunkManager
        std::unique_ptr<IChunkStorage> storageCopy = std::make_unique<ESFChunkStorage>(m_worldPath);
        m_chunkManager->SetChunkStorage(std::move(storageCopy));
        core::LogInfo("world", "Chunk storage passed to ChunkManager for world '%s'", m_worldName.c_str());
    }
    core::LogInfo("world", "Chunk storage configured for world '%s'", m_worldName.c_str());
}

/**
 * Initializes the world storage system by either creating a new world or
 * loading an existing one. This involves setting up the world path, configuring
 * the world manager, and initializing the chunk storage and serializer.
 *
 * @param savesPath The base directory where world data should be saved or
 *                  accessed.
 * @return True if the world storage initialization was successful, false
 *         otherwise.
 */
bool World::InitializeWorldStorage(const std::string& savesPath)
{
    // Build the world path
    m_worldPath = savesPath + "/" + m_worldName;

    // Create a world manager
    m_worldManager = std::make_unique<ESFWorldManager>(m_worldPath);

    // If the world does not exist, create a new world
    if (!m_worldManager->WorldExists())
    {
        ESFWorldManager::WorldInfo worldInfo;
        worldInfo.worldName = m_worldName;
        worldInfo.worldSeed = m_worldSeed;
        worldInfo.spawnX    = 0;
        worldInfo.spawnY    = 0;
        worldInfo.spawnZ    = 128;

        if (!m_worldManager->CreateWorld(worldInfo))
        {
            core::LogError("world", "Failed to create world '%s' at path '%s'", m_worldName.c_str(), m_worldPath.c_str());
            return false;
        }

        core::LogInfo("world", "Created new world '%s' at '%s'", m_worldName.c_str(), m_worldPath.c_str());
    }
    else
    {
        core::LogInfo("world", "Found existing world '%s' at '%s'", m_worldName.c_str(), m_worldPath.c_str());
    }

    // Automatically set up the ESF storage system
    auto esfStorage    = std::make_unique<ESFChunkStorage>(m_worldPath);
    auto esfSerializer = std::make_unique<ESFChunkSerializer>();

    SetChunkStorage(std::move(esfStorage));
    SetChunkSerializer(std::move(esfSerializer));

    core::LogInfo("world", "World storage initialized for '%s'", m_worldName.c_str());
    return true;
}

bool World::SaveWorld()
{
    if (!m_worldManager)
    {
        core::LogError("world", "Cannot save world '%s' - world manager not initialized", m_worldName.c_str());
        return false;
    }

    // Save world information
    ESFWorldManager::WorldInfo worldInfo;
    worldInfo.worldName = m_worldName;
    worldInfo.worldSeed = m_worldSeed;
    worldInfo.spawnX    = static_cast<int32_t>(m_playerPosition.x);
    worldInfo.spawnY    = static_cast<int32_t>(m_playerPosition.y);
    worldInfo.spawnZ    = static_cast<int32_t>(m_playerPosition.z);

    if (!m_worldManager->SaveWorldInfo(worldInfo))
    {
        core::LogError("world", "Failed to save world info for '%s'", m_worldName.c_str());
        return false;
    }

    // Force save all block data
    if (m_chunkManager)
    {
        size_t savedChunks = m_chunkManager->SaveAllModifiedChunks();
        core::LogInfo("world", "Saved %zu modified chunks for world '%s'", savedChunks, m_worldName.c_str());
        m_chunkManager->FlushStorage();
    }

    core::LogInfo("world", "World '%s' saved successfully", m_worldName.c_str());
    return true;
}

bool World::LoadWorld()
{
    if (!m_worldManager)
    {
        core::LogError("world", "Cannot load world '%s' - world manager not initialized", m_worldName.c_str());
        return false;
    }

    // Load world information
    ESFWorldManager::WorldInfo worldInfo;
    if (!m_worldManager->LoadWorldInfo(worldInfo))
    {
        core::LogError("world", "Failed to load world info for '%s'", m_worldName.c_str());
        return false;
    }

    // Update world parameters
    m_worldSeed        = worldInfo.worldSeed;
    m_playerPosition.x = static_cast<float>(worldInfo.spawnX);
    m_playerPosition.y = static_cast<float>(worldInfo.spawnY);
    m_playerPosition.z = static_cast<float>(worldInfo.spawnZ);

    core::LogInfo("world", "World '%s' loaded successfully (seed: %llu, spawn: %d,%d,%d)",
                  m_worldName.c_str(), m_worldSeed, worldInfo.spawnX, worldInfo.spawnY, worldInfo.spawnZ);
    return true;
}

void World::CloseWorld()
{
    // Save world data
    SaveWorld();

    // Turn off the storage system
    if (m_chunkStorage)
    {
        m_chunkStorage->Close();
        core::LogInfo("world", "Chunk storage closed for world '%s'", m_worldName.c_str());
    }

    // Reset the manager
    m_worldManager.reset();

    core::LogInfo("world", "World '%s' closed", m_worldName.c_str());
}
