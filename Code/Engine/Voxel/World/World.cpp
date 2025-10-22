#include "World.hpp"
#include "ESFWorldStorage.hpp"
#include "ESFSWorldStorage.hpp"
#include "../Chunk/ChunkStorageConfig.hpp"
#include "../Chunk/ESFSChunkSerializer.hpp"
#include "../Chunk/BuildMeshJob.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

using namespace enigma::voxel;
DEFINE_LOG_CATEGORY(LogWorld)

World::~World()
{
}

World::World(const std::string& worldName, uint64_t worldSeed, std::unique_ptr<Generator> generator) : m_worldName(worldName), m_worldSeed(worldSeed)
{
    // Create and initialize ChunkManager (follow NeoForge mode)
    m_chunkManager = std::make_unique<ChunkManager>(this);
    m_chunkManager->Initialize();

    // Initialize the ESF storage system
    if (!InitializeWorldStorage(WORLD_SAVE_PATH))
    {
        LogError(LogWorld, "Failed to initialize world storage system");
        ERROR_AND_DIE("Failed to initialize world storage system")
    }
    LogInfo(LogWorld, "World storage system initialized in: %s", WORLD_SAVE_PATH);

    // Set up the world generator
    SetWorldGenerator(std::move(generator));
    LogInfo(LogWorld, "World fully initialized with name: %s", m_worldName.c_str());
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
        chunk->SetBlockWorldByPlayer(pos, state);

        // Submit high-priority mesh rebuild for player interaction
        // This ensures instant visual feedback when player places/breaks blocks
        const_cast<World*>(this)->SubmitBuildMeshJob(chunk, TaskPriority::High);
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
        LogDebug(LogWorld, "Generating content for chunk (%d, %d)", chunkX, chunkY);
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
    if (!m_chunkManager)
    {
        return;
    }

    // Phase 3 Optimization: Process up to 5 chunks per frame (vs original 1)
    // This dramatically improves initial loading speed for new maps
    constexpr int MAX_ACTIVATIONS_PER_FRAME = 5;
    int           activatedThisFrame        = 0;

    // Phase 3: Activate chunks asynchronously (distance-sorted, nearest first)
    for (const auto& [chunkX, chunkY] : neededChunks)
    {
        // Limit activations per frame to avoid FPS drops
        if (activatedThisFrame >= MAX_ACTIVATIONS_PER_FRAME)
        {
            break; // Stop processing, continue next frame
        }

        // Check if chunk is already loaded/active
        if (m_chunkManager->IsChunkLoaded(chunkX, chunkY))
        {
            continue; // Already loaded, skip
        }

        // Check if chunk is already being processed (pending load/generate/save)
        int64_t packedCoords = ChunkManager::PackCoordinates(chunkX, chunkY);
        bool    isPending    = m_chunksWithPendingLoad.count(packedCoords) > 0 ||
            m_chunksWithPendingGenerate.count(packedCoords) > 0 ||
            m_chunksWithPendingSave.count(packedCoords) > 0;

        if (isPending)
        {
            continue; // Already queued, skip
        }

        // Activate chunk asynchronously (will check disk and submit job)
        ActivateChunk(IntVec2(chunkX, chunkY));
        ++activatedThisFrame;
    }

    if (activatedThisFrame > 0)
    {
        LogDebug(LogWorld, "Activated %d chunks this frame", activatedThisFrame);
    }

    // Deactivate distant chunks (still synchronous for now - safe on main thread)
    m_chunkManager->UnloadDistantChunks(m_playerPosition, m_chunkActivationRange + 2);
}

std::vector<std::pair<int32_t, int32_t>> World::CalculateNeededChunks() const
{
    // Calculate the chunk coordinates of the player
    // Chunk coordinates use (X, Y) horizontal plane, Z is vertical height
    int32_t playerChunkX = static_cast<int32_t>(floor(m_playerPosition.x / 16.0f));
    int32_t playerChunkY = static_cast<int32_t>(floor(m_playerPosition.y / 16.0f));

    // Temporary storage with distance information
    struct ChunkWithDistance
    {
        int32_t chunkX;
        int32_t chunkY;
        float   distance;
    };

    std::vector<ChunkWithDistance> chunksWithDistance;

    // Add all blocks within the activation range
    for (int32_t dx = -m_chunkActivationRange; dx <= m_chunkActivationRange; dx++)
    {
        for (int32_t dy = -m_chunkActivationRange; dy <= m_chunkActivationRange; dy++)
        {
            // Use circle to load areas instead of squares
            float distance = (float)sqrt(dx * dx + dy * dy);
            if (distance <= (float)m_chunkActivationRange)
            {
                int32_t chunkX = playerChunkX + dx;
                int32_t chunkY = playerChunkY + dy;

                chunksWithDistance.push_back({chunkX, chunkY, distance});
            }
        }
    }

    // Sort by distance (nearest first) - CRITICAL for loading optimization
    std::sort(chunksWithDistance.begin(), chunksWithDistance.end(),
              [](const ChunkWithDistance& a, const ChunkWithDistance& b)
              {
                  return a.distance < b.distance;
              });

    // Convert to final output format (drop distance field)
    std::vector<std::pair<int32_t, int32_t>> neededChunks;
    neededChunks.reserve(chunksWithDistance.size());
    for (const auto& chunk : chunksWithDistance)
    {
        neededChunks.emplace_back(chunk.chunkX, chunk.chunkY);
    }

    return neededChunks;
}

void World::Update(float deltaTime)
{
    // Phase 3: Update nearby chunks (activate/deactivate based on player position)
    UpdateNearbyChunks();

    // Phase 4: Process job queues with limits (submit pending → active)
    ProcessJobQueues();

    // Phase 4: Remove distant jobs from pending queues
    RemoveDistantJobs();

    // Process completed chunk tasks from async workers (Phase 3)
    ProcessCompletedChunkTasks();

    // Update ChunkManager
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
    LogInfo(LogWorld, "Set chunk activation range to %d chunks", chunkDistance);
}

void World::SetWorldGenerator(std::unique_ptr<enigma::voxel::Generator> generator)
{
    m_worldGenerator = std::move(generator);
    if (m_worldGenerator)
    {
        // Initialize the generator with world seed
        m_worldGenerator->Initialize(static_cast<uint32_t>(m_worldSeed));
        LogInfo(LogWorld, "World generator set and initialized for world '%s'", m_worldName.c_str());
    }
}

/**
 * Sets the chunk serializer for the world.
 * Note: ChunkManager's serializer is configured separately in InitializeWorldStorage().
 *
 * @param serializer A unique pointer to the chunk serializer to be configured
 *                   for the world.
 */
void World::SetChunkSerializer(std::unique_ptr<IChunkSerializer> serializer)
{
    m_chunkSerializer = std::move(serializer);
    LogInfo(LogWorld, "Chunk serializer configured for world '%s'", m_worldName.c_str());
}

void World::SetChunkStorage(std::unique_ptr<IChunkStorage> storage)
{
    m_chunkStorage = std::move(storage);
    LogInfo(LogWorld, "Chunk storage configured for world '%s'", m_worldName.c_str());
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
            LogError(LogWorld, "Failed to create world '%s' at path '%s'", m_worldName.c_str(), m_worldPath.c_str());
            return false;
        }

        LogInfo(LogWorld, "Created new world '%s' at '%s'", m_worldName.c_str(), m_worldPath.c_str());
    }
    else
    {
        LogInfo(LogWorld, "Found existing world '%s' at '%s'", m_worldName.c_str(), m_worldPath.c_str());
    }

    // Load ChunkStorageConfig from YAML
    ChunkStorageConfig config = ChunkStorageConfig::LoadFromYaml("");
    LogInfo(LogWorld, "Loaded chunk storage config: %s", config.ToString().c_str());

    // Create chunk storage based on config format selection
    if (config.storageFormat == ChunkStorageFormat::ESFS)
    {
        // ESFS format: single-file, RLE compression, ID-only
        // Create ESFS serializer for World
        auto esfsSerializer = std::make_unique<ESFSChunkSerializer>();
        SetChunkSerializer(std::move(esfsSerializer));

        // Create ESFS storage for World (pass serializer pointer to storage)
        auto esfsStorage = std::make_unique<ESFSChunkStorage>(m_worldPath, config, m_chunkSerializer.get());
        SetChunkStorage(std::move(esfsStorage));

        // Create separate ESFS serializer and storage for ChunkManager
        auto              esfsSerializerForManager = std::make_unique<ESFSChunkSerializer>();
        IChunkSerializer* serializerPtr            = esfsSerializerForManager.get(); // Get pointer before move
        m_chunkManager->SetChunkSerializer(std::move(esfsSerializerForManager));

        auto esfsStorageForManager = std::make_unique<ESFSChunkStorage>(m_worldPath, config, serializerPtr);
        m_chunkManager->SetChunkStorage(std::move(esfsStorageForManager));

        LogInfo(LogWorld, "World storage initialized with ESFS format (RLE compression)");
    }
    else if (config.storageFormat == ChunkStorageFormat::ESF)
    {
        // ESF format: region files, BlockState serialization (uses internal BlockStateSerializer)
        // ESF does not use external serializer, no need to call SetChunkSerializer

        // Create ESF storage for World
        auto esfStorage = std::make_unique<ESFChunkStorage>(m_worldPath);
        SetChunkStorage(std::move(esfStorage));

        // Create separate ESF storage for ChunkManager
        auto esfStorageForManager = std::make_unique<ESFChunkStorage>(m_worldPath);
        m_chunkManager->SetChunkStorage(std::move(esfStorageForManager));

        LogInfo(LogWorld, "World storage initialized with ESF format (region files)");
    }
    else
    {
        LogError(LogWorld, "Unknown storage format in config");
        return false;
    }

    LogInfo(LogWorld, "World storage initialized for '%s'", m_worldName.c_str());
    return true;
}

bool World::SaveWorld()
{
    if (!m_worldManager)
    {
        LogError(LogWorld, "Cannot save world '%s' - world manager not initialized", m_worldName.c_str());
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
        LogError(LogWorld, "Failed to save world info for '%s'", m_worldName.c_str());
        return false;
    }

    // Force save all block data
    if (m_chunkManager)
    {
        size_t savedChunks = m_chunkManager->SaveAllModifiedChunks();
        LogInfo(LogWorld, "Saved %zu modified chunks for world '%s'", savedChunks, m_worldName.c_str());
        m_chunkManager->FlushStorage();
    }

    LogInfo(LogWorld, "World '%s' saved successfully", m_worldName.c_str());
    return true;
}

bool World::LoadWorld()
{
    if (!m_worldManager)
    {
        LogError(LogWorld, "Cannot load world '%s' - world manager not initialized", m_worldName.c_str());
        return false;
    }

    // Load world information
    ESFWorldManager::WorldInfo worldInfo;
    if (!m_worldManager->LoadWorldInfo(worldInfo))
    {
        LogError(LogWorld, "Failed to load world info for '%s'", m_worldName.c_str());
        return false;
    }

    // Update world parameters
    m_worldSeed        = worldInfo.worldSeed;
    m_playerPosition.x = static_cast<float>(worldInfo.spawnX);
    m_playerPosition.y = static_cast<float>(worldInfo.spawnY);
    m_playerPosition.z = static_cast<float>(worldInfo.spawnZ);

    LogInfo(LogWorld, "World '%s' loaded successfully (seed: %llu, spawn: %d,%d,%d)",
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
        LogInfo(LogWorld, "Chunk storage closed for world '%s'", m_worldName.c_str());
    }

    // Reset the manager
    m_worldManager.reset();

    LogInfo(LogWorld, "World '%s' closed", m_worldName.c_str());
}

//-------------------------------------------------------------------------------------------
// Phase 3: Async Task Management Implementation
// Uses global g_theSchedule for task submission
//-------------------------------------------------------------------------------------------

void World::ActivateChunk(IntVec2 chunkCoords)
{
    // Phase 4: ASYNC path with job queue - add to pending queue instead of immediate submission
    // ProcessJobQueues() will submit jobs when active count < limit

    Chunk* chunk = m_chunkManager->GetChunk(chunkCoords.x, chunkCoords.y);
    if (!chunk)
    {
        // Create EMPTY chunk object (no generation yet)
        int64_t packedCoords = ChunkManager::PackCoordinates(chunkCoords.x, chunkCoords.y);
        auto&   loadedChunks = m_chunkManager->GetLoadedChunks();

        auto newChunk              = std::make_unique<Chunk>(chunkCoords);
        chunk                      = newChunk.get();
        loadedChunks[packedCoords] = std::move(newChunk);

        LogDebug(LogWorld, "Created empty chunk (%d, %d) for async activation", chunkCoords.x, chunkCoords.y);
    }

    // Transition from Inactive to CheckingDisk
    ChunkState currentState = chunk->GetState();
    if (currentState != ChunkState::Inactive)
    {
        // Chunk already being processed or active
        LogDebug(LogWorld, "Chunk (%d, %d) already in state %d, skipping activation",
                 chunkCoords.x, chunkCoords.y, static_cast<int>(currentState));
        return;
    }

    if (!chunk->TrySetState(ChunkState::Inactive, ChunkState::CheckingDisk))
    {
        LogWarn(LogWorld, "Failed to transition chunk (%d, %d) to CheckingDisk state", chunkCoords.x, chunkCoords.y);
        return;
    }

    LogDebug(LogWorld, "Chunk (%d, %d) transitioned to CheckingDisk", chunkCoords.x, chunkCoords.y);

    // Check if chunk exists on disk
    bool chunkExistsOnDisk = false;
    if (m_chunkStorage)
    {
        chunkExistsOnDisk = m_chunkStorage->ChunkExists(chunkCoords.x, chunkCoords.y);
        LogDebug(LogWorld, "Chunk (%d, %d) disk check: %s",
                 chunkCoords.x, chunkCoords.y,
                 chunkExistsOnDisk ? "EXISTS" : "NOT_FOUND");
    }
    else
    {
        LogDebug(LogWorld, "Chunk (%d, %d) no storage configured, will generate", chunkCoords.x, chunkCoords.y);
    }

    if (chunkExistsOnDisk)
    {
        // Phase 4: Transition to PendingLoad and add to queue (ProcessJobQueues() will submit)
        if (chunk->TrySetState(ChunkState::CheckingDisk, ChunkState::PendingLoad))
        {
            m_pendingLoadQueue.push_back(chunkCoords);
            LogDebug(LogWorld, "Chunk (%d, %d) added to load queue (size: %zu)",
                     chunkCoords.x, chunkCoords.y, m_pendingLoadQueue.size());
        }
    }
    else
    {
        // Phase 4: Transition to PendingGenerate and add to queue (ProcessJobQueues() will submit)
        if (chunk->TrySetState(ChunkState::CheckingDisk, ChunkState::PendingGenerate))
        {
            m_pendingGenerateQueue.push_back(chunkCoords);
            LogDebug(LogWorld, "Chunk (%d, %d) added to generate queue (size: %zu)",
                     chunkCoords.x, chunkCoords.y, m_pendingGenerateQueue.size());
        }
    }
}

void World::DeactivateChunk(IntVec2 chunkCoords)
{
    Chunk* chunk = m_chunkManager->GetChunk(chunkCoords.x, chunkCoords.y);
    if (!chunk)
    {
        return; // Chunk already unloaded
    }

    ChunkState currentState = chunk->GetState();

    // Only deactivate Active chunks
    if (currentState != ChunkState::Active)
    {
        return; // Chunk not in Active state, skip deactivation
    }

    // Check if chunk needs saving
    if (chunk->IsModified())
    {
        // Phase 4: Transition to PendingSave and add to queue (ProcessJobQueues() will submit)
        if (chunk->TrySetState(ChunkState::Active, ChunkState::PendingSave))
        {
            m_pendingSaveQueue.push_back(chunkCoords);
            LogDebug(LogWorld, "Chunk (%d, %d) added to save queue (size: %zu)",
                     chunkCoords.x, chunkCoords.y, m_pendingSaveQueue.size());
        }
    }
    else
    {
        // No need to save, directly unload
        if (chunk->TrySetState(ChunkState::Active, ChunkState::PendingUnload))
        {
            if (chunk->TrySetState(ChunkState::PendingUnload, ChunkState::Unloading))
            {
                // Perform immediate unload on main thread
                chunk->TrySetState(ChunkState::Unloading, ChunkState::Inactive);
                m_chunkManager->UnloadChunk(chunkCoords.x, chunkCoords.y);
                LogDebug(LogWorld, "Unloaded clean chunk (%d, %d)", chunkCoords.x, chunkCoords.y);
            }
        }
    }
}

void World::SubmitGenerateChunkJob(IntVec2 chunkCoords, Chunk* chunk)
{
    if (!g_theSchedule)
    {
        LogError(LogWorld, "Cannot submit GenerateChunkJob - g_theSchedule not initialized");
        return;
    }

    if (!m_worldGenerator)
    {
        LogError(LogWorld, "Cannot submit GenerateChunkJob - WorldGenerator not set");
        return;
    }

    // Create GenerateChunkJob and transfer ownership to ScheduleSubsystem
    GenerateChunkJob* job = new GenerateChunkJob(
        chunkCoords,
        chunk,
        m_worldGenerator.get(),
        static_cast<uint32_t>(m_worldSeed)
    );

    // Submit to global ScheduleSubsystem (transfers ownership)
    g_theSchedule->AddTask(job);

    // Track pending job
    int64_t packedCoords = ChunkManager::PackCoordinates(chunkCoords.x, chunkCoords.y);
    m_chunksWithPendingGenerate.insert(packedCoords);

    LogDebug(LogWorld, "Submitted GenerateChunkJob for chunk (%d, %d)", chunkCoords.x, chunkCoords.y);
}

void World::SubmitLoadChunkJob(IntVec2 chunkCoords, Chunk* chunk)
{
    if (!g_theSchedule)
    {
        LogError(LogWorld, "Cannot submit LoadChunkJob - g_theSchedule not initialized");
        return;
    }

    if (!m_chunkStorage)
    {
        LogError(LogWorld, "Cannot submit LoadChunkJob - ChunkStorage not set");
        return;
    }

    // Try casting to ESFChunkStorage first
    ESFChunkStorage* esfStorage = dynamic_cast<ESFChunkStorage*>(m_chunkStorage.get());
    if (esfStorage)
    {
        // Create LoadChunkJob for ESF format and transfer ownership to ScheduleSubsystem
        LoadChunkJob* job = new LoadChunkJob(chunkCoords, chunk, esfStorage);
        g_theSchedule->AddTask(job);

        // Track pending job
        int64_t packedCoords = ChunkManager::PackCoordinates(chunkCoords.x, chunkCoords.y);
        m_chunksWithPendingLoad.insert(packedCoords);

        LogDebug(LogWorld, "Submitted LoadChunkJob (ESF) for chunk (%d, %d)", chunkCoords.x, chunkCoords.y);
        return;
    }

    // Try casting to ESFSChunkStorage
    ESFSChunkStorage* esfsStorage = dynamic_cast<ESFSChunkStorage*>(m_chunkStorage.get());
    if (esfsStorage)
    {
        // Create LoadChunkJob for ESFS format and transfer ownership to ScheduleSubsystem
        LoadChunkJob* job = new LoadChunkJob(chunkCoords, chunk, esfsStorage);
        g_theSchedule->AddTask(job);

        // Track pending job
        int64_t packedCoords = ChunkManager::PackCoordinates(chunkCoords.x, chunkCoords.y);
        m_chunksWithPendingLoad.insert(packedCoords);

        LogDebug(LogWorld, "Submitted LoadChunkJob (ESFS) for chunk (%d, %d)", chunkCoords.x, chunkCoords.y);
        return;
    }

    LogError(LogWorld, "ChunkStorage is neither ESFChunkStorage nor ESFSChunkStorage type");
}

void World::SubmitSaveChunkJob(IntVec2 chunkCoords, const Chunk* chunk)
{
    if (!g_theSchedule)
    {
        LogError(LogWorld, "Cannot submit SaveChunkJob - g_theSchedule not initialized");
        return;
    }

    if (!m_chunkStorage)
    {
        LogError(LogWorld, "Cannot submit SaveChunkJob - ChunkStorage not set");
        return;
    }

    // Try casting to ESFChunkStorage first
    ESFChunkStorage* esfStorage = dynamic_cast<ESFChunkStorage*>(m_chunkStorage.get());
    if (esfStorage)
    {
        // Create SaveChunkJob for ESF format (deep copy performed in constructor) and transfer ownership
        SaveChunkJob* job = new SaveChunkJob(chunkCoords, chunk, esfStorage);
        g_theSchedule->AddTask(job);

        // Track pending job
        int64_t packedCoords = ChunkManager::PackCoordinates(chunkCoords.x, chunkCoords.y);
        m_chunksWithPendingSave.insert(packedCoords);

        LogDebug(LogWorld, "Submitted SaveChunkJob (ESF) for chunk (%d, %d)", chunkCoords.x, chunkCoords.y);
        return;
    }

    // Try casting to ESFSChunkStorage
    ESFSChunkStorage* esfsStorage = dynamic_cast<ESFSChunkStorage*>(m_chunkStorage.get());
    if (esfsStorage)
    {
        // Create SaveChunkJob for ESFS format (deep copy performed in constructor) and transfer ownership
        SaveChunkJob* job = new SaveChunkJob(chunkCoords, chunk, esfsStorage);
        g_theSchedule->AddTask(job);

        // Track pending job
        int64_t packedCoords = ChunkManager::PackCoordinates(chunkCoords.x, chunkCoords.y);
        m_chunksWithPendingSave.insert(packedCoords);

        LogDebug(LogWorld, "Submitted SaveChunkJob (ESFS) for chunk (%d, %d)", chunkCoords.x, chunkCoords.y);
        return;
    }

    LogError(LogWorld, "ChunkStorage is neither ESFChunkStorage nor ESFSChunkStorage type");
}

void World::HandleGenerateChunkCompleted(GenerateChunkJob* job)
{
    IntVec2 chunkCoords  = job->GetChunkCoords();
    int64_t packedCoords = ChunkManager::PackCoordinates(chunkCoords.x, chunkCoords.y);

    // Phase 4: Decrement active generate job counter
    m_activeGenerateJobs--;
    LogDebug(LogWorld, "Generate job completed for chunk (%d, %d) - Active: %d/%d",
             chunkCoords.x, chunkCoords.y,
             m_activeGenerateJobs.load(), m_maxGenerateJobs);

    // Remove from pending tracking
    m_chunksWithPendingGenerate.erase(packedCoords);

    // Get chunk from ChunkManager
    Chunk* chunk = m_chunkManager->GetChunk(chunkCoords.x, chunkCoords.y);
    if (!chunk)
    {
        LogWarn(LogWorld, "Chunk (%d, %d) no longer exists after generation", chunkCoords.x, chunkCoords.y);
        return;
    }

    // Check if job was cancelled
    if (job->IsCancelled())
    {
        LogDebug(LogWorld, "GenerateChunkJob for chunk (%d, %d) was cancelled", chunkCoords.x, chunkCoords.y);
        // Transition back to Inactive
        chunk->TrySetState(ChunkState::Generating, ChunkState::Inactive);
        return;
    }

    // Transition from Generating to Active
    if (chunk->TrySetState(ChunkState::Generating, ChunkState::Active))
    {
        chunk->SetGenerated(true);
        chunk->SetModified(true); // Newly generated chunks should be saved

        // Submit normal-priority async mesh build job for generated chunk
        // No need to mark dirty - async system handles mesh building
        SubmitBuildMeshJob(chunk, TaskPriority::Normal);

        LogDebug(LogWorld, "Chunk (%d, %d) generation completed, now Active (async mesh build submitted)", chunkCoords.x, chunkCoords.y);
    }
    else
    {
        LogWarn(LogWorld, "Failed to transition chunk (%d, %d) to Active after generation", chunkCoords.x, chunkCoords.y);
    }
}

void World::HandleLoadChunkCompleted(LoadChunkJob* job)
{
    IntVec2 chunkCoords  = job->GetChunkCoords();
    int64_t packedCoords = ChunkManager::PackCoordinates(chunkCoords.x, chunkCoords.y);

    // Phase 4: Decrement active load job counter
    m_activeLoadJobs--;
    LogDebug(LogWorld, "Load job completed for chunk (%d, %d) - Active: %d/%d",
             chunkCoords.x, chunkCoords.y,
             m_activeLoadJobs.load(), m_maxLoadJobs);

    // Remove from pending tracking
    m_chunksWithPendingLoad.erase(packedCoords);

    // Get chunk from ChunkManager
    Chunk* chunk = m_chunkManager->GetChunk(chunkCoords.x, chunkCoords.y);
    if (!chunk)
    {
        LogWarn(LogWorld, "Chunk (%d, %d) no longer exists after load attempt", chunkCoords.x, chunkCoords.y);
        return;
    }

    // Check if job was cancelled
    if (job->IsCancelled())
    {
        LogDebug(LogWorld, "LoadChunkJob for chunk (%d, %d) was cancelled", chunkCoords.x, chunkCoords.y);
        chunk->TrySetState(ChunkState::Loading, ChunkState::Inactive);
        return;
    }

    // Check load success
    if (job->WasSuccessful())
    {
        // Load succeeded, transition to Active
        if (chunk->TrySetState(ChunkState::Loading, ChunkState::Active))
        {
            chunk->SetGenerated(true);

            // Submit normal-priority async mesh build job for loaded chunk
            SubmitBuildMeshJob(chunk, TaskPriority::Normal);

            LogDebug(LogWorld, "Chunk (%d, %d) loaded successfully, now Active (async mesh build submitted)", chunkCoords.x, chunkCoords.y);
        }
        else
        {
            LogWarn(LogWorld, "Failed to transition chunk (%d, %d) to Active after load", chunkCoords.x, chunkCoords.y);
        }
    }
    else
    {
        // Phase 4: Load failed, add to generate queue instead of immediate submission
        LogDebug(LogWorld, "Chunk (%d, %d) load failed, adding to generate queue", chunkCoords.x, chunkCoords.y);

        if (chunk->TrySetState(ChunkState::Loading, ChunkState::PendingGenerate))
        {
            m_pendingGenerateQueue.push_back(chunkCoords);
            LogDebug(LogWorld, "Chunk (%d, %d) added to generate queue after load failure (size: %zu)",
                     chunkCoords.x, chunkCoords.y, m_pendingGenerateQueue.size());
        }
    }
}

void World::HandleSaveChunkCompleted(SaveChunkJob* job)
{
    IntVec2 chunkCoords  = job->GetChunkCoords();
    int64_t packedCoords = ChunkManager::PackCoordinates(chunkCoords.x, chunkCoords.y);

    // Phase 4: Decrement active save job counter
    m_activeSaveJobs--;
    LogDebug(LogWorld, "Save job completed for chunk (%d, %d) - Active: %d/%d",
             chunkCoords.x, chunkCoords.y,
             m_activeSaveJobs.load(), m_maxSaveJobs);

    // Remove from pending tracking
    m_chunksWithPendingSave.erase(packedCoords);

    // Get chunk from ChunkManager
    Chunk* chunk = m_chunkManager->GetChunk(chunkCoords.x, chunkCoords.y);
    if (!chunk)
    {
        LogDebug(LogWorld, "Chunk (%d, %d) no longer exists after save", chunkCoords.x, chunkCoords.y);
        return;
    }

    // Check if job was cancelled
    if (job->IsCancelled())
    {
        LogDebug(LogWorld, "SaveChunkJob for chunk (%d, %d) was cancelled", chunkCoords.x, chunkCoords.y);
        // Don't transition state if cancelled
        return;
    }

    // Save completed successfully
    chunk->SetModified(false); // Clear modified flag
    LogDebug(LogWorld, "Chunk (%d, %d) saved successfully", chunkCoords.x, chunkCoords.y);

    // Transition from Saving to PendingUnload -> Unloading -> Inactive
    if (chunk->TrySetState(ChunkState::Saving, ChunkState::PendingUnload))
    {
        if (chunk->TrySetState(ChunkState::PendingUnload, ChunkState::Unloading))
        {
            chunk->TrySetState(ChunkState::Unloading, ChunkState::Inactive);
            m_chunkManager->UnloadChunk(chunkCoords.x, chunkCoords.y);
            LogDebug(LogWorld, "Chunk (%d, %d) unloaded after save", chunkCoords.x, chunkCoords.y);
        }
    }
}

//-------------------------------------------------------------------------------------------
// Phase 4: Job Queue Management Implementation
//-------------------------------------------------------------------------------------------

void World::ProcessJobQueues()
{
    // Process Generate queue (highest priority: fill player's surroundings)
    while (m_activeGenerateJobs < m_maxGenerateJobs && !m_pendingGenerateQueue.empty())
    {
        IntVec2 chunkCoords = m_pendingGenerateQueue.front();
        m_pendingGenerateQueue.pop_front();

        // Check if chunk still exists and needs generation
        Chunk* chunk = m_chunkManager->GetChunk(chunkCoords.x, chunkCoords.y);
        if (!chunk)
        {
            continue; // Chunk was unloaded, skip
        }

        ChunkState state = chunk->GetState();
        if (state != ChunkState::PendingGenerate && state != ChunkState::Generating)
        {
            continue; // Chunk no longer needs generation
        }

        // Transition to Generating and submit job
        if (chunk->TrySetState(ChunkState::PendingGenerate, ChunkState::Generating))
        {
            SubmitGenerateChunkJob(chunkCoords, chunk);
            m_activeGenerateJobs++; // Increment active counter
            LogDebug(LogWorld, "Submitted generate job for chunk (%d, %d) - Active: %d/%d",
                     chunkCoords.x, chunkCoords.y,
                     m_activeGenerateJobs.load(), m_maxGenerateJobs);
        }
    }

    // Process Load queue (medium priority: load from disk before generating)
    while (m_activeLoadJobs < m_maxLoadJobs && !m_pendingLoadQueue.empty())
    {
        IntVec2 chunkCoords = m_pendingLoadQueue.front();
        m_pendingLoadQueue.pop_front();

        // Check if chunk still exists and needs loading
        Chunk* chunk = m_chunkManager->GetChunk(chunkCoords.x, chunkCoords.y);
        if (!chunk)
        {
            continue; // Chunk was unloaded, skip
        }

        ChunkState state = chunk->GetState();
        if (state != ChunkState::PendingLoad && state != ChunkState::Loading)
        {
            continue; // Chunk no longer needs loading
        }

        // Transition to Loading and submit job
        if (chunk->TrySetState(ChunkState::PendingLoad, ChunkState::Loading))
        {
            SubmitLoadChunkJob(chunkCoords, chunk);
            m_activeLoadJobs++; // Increment active counter
            LogDebug(LogWorld, "Submitted load job for chunk (%d, %d) - Active: %d/%d",
                     chunkCoords.x, chunkCoords.y,
                     m_activeLoadJobs.load(), m_maxLoadJobs);
        }
    }

    // Process Save queue (lowest priority: saving can wait)
    while (m_activeSaveJobs < m_maxSaveJobs && !m_pendingSaveQueue.empty())
    {
        IntVec2 chunkCoords = m_pendingSaveQueue.front();
        m_pendingSaveQueue.pop_front();

        // Check if chunk still exists and needs saving
        Chunk* chunk = m_chunkManager->GetChunk(chunkCoords.x, chunkCoords.y);
        if (!chunk)
        {
            continue; // Chunk was unloaded, skip
        }

        ChunkState state = chunk->GetState();
        if (state != ChunkState::PendingSave && state != ChunkState::Saving)
        {
            continue; // Chunk no longer needs saving
        }

        // Transition to Saving and submit job
        if (chunk->TrySetState(ChunkState::PendingSave, ChunkState::Saving))
        {
            SubmitSaveChunkJob(chunkCoords, chunk);
            m_activeSaveJobs++; // Increment active counter
            LogDebug(LogWorld, "Submitted save job for chunk (%d, %d) - Active: %d/%d",
                     chunkCoords.x, chunkCoords.y,
                     m_activeSaveJobs.load(), m_maxSaveJobs);
        }
    }
}

void World::RemoveDistantJobs()
{
    // Calculate player's current chunk position
    // Chunk coordinates use (X, Y) horizontal plane, Z is vertical height
    int32_t playerChunkX = static_cast<int32_t>(floor(m_playerPosition.x / 16.0f));
    int32_t playerChunkY = static_cast<int32_t>(floor(m_playerPosition.y / 16.0f));

    // Distance threshold for job cancellation (activation range + buffer)
    float maxDistanceSq = static_cast<float>((m_chunkActivationRange + 2) * (m_chunkActivationRange + 2));

    // Helper lambda to check distance
    auto isDistant = [&](const IntVec2& coords) -> bool
    {
        int32_t dx         = coords.x - playerChunkX;
        int32_t dy         = coords.y - playerChunkY;
        float   distanceSq = static_cast<float>(dx * dx + dy * dy);
        return distanceSq > maxDistanceSq;
    };

    // Remove distant jobs from Generate queue
    auto   it              = std::remove_if(m_pendingGenerateQueue.begin(), m_pendingGenerateQueue.end(), isDistant);
    size_t removedGenerate = std::distance(it, m_pendingGenerateQueue.end());
    m_pendingGenerateQueue.erase(it, m_pendingGenerateQueue.end());

    // Remove distant jobs from Load queue
    it                 = std::remove_if(m_pendingLoadQueue.begin(), m_pendingLoadQueue.end(), isDistant);
    size_t removedLoad = std::distance(it, m_pendingLoadQueue.end());
    m_pendingLoadQueue.erase(it, m_pendingLoadQueue.end());

    // Remove distant jobs from Save queue
    it                 = std::remove_if(m_pendingSaveQueue.begin(), m_pendingSaveQueue.end(), isDistant);
    size_t removedSave = std::distance(it, m_pendingSaveQueue.end());
    m_pendingSaveQueue.erase(it, m_pendingSaveQueue.end());

    // Log cancellations (only if any were removed)
    if (removedGenerate > 0 || removedLoad > 0 || removedSave > 0)
    {
        LogDebug(LogWorld, "Removed distant jobs: %zu generate, %zu load, %zu save",
                 removedGenerate, removedLoad, removedSave);
    }
}

void World::ProcessCompletedChunkTasks()
{
    if (!g_theSchedule)
    {
        return; // No global ScheduleSubsystem, cannot process tasks
    }

    // Retrieve all completed tasks from global ScheduleSubsystem (returns vector)
    std::vector<RunnableTask*> completedTasks = g_theSchedule->RetrieveCompletedTasks();

    // Process each completed task
    for (RunnableTask* task : completedTasks)
    {
        // Attempt to downcast to ChunkJob types
        if (auto* genJob = dynamic_cast<GenerateChunkJob*>(task))
        {
            HandleGenerateChunkCompleted(genJob);
        }
        else if (auto* loadJob = dynamic_cast<LoadChunkJob*>(task))
        {
            HandleLoadChunkCompleted(loadJob);
        }
        else if (auto* saveJob = dynamic_cast<SaveChunkJob*>(task))
        {
            HandleSaveChunkCompleted(saveJob);
        }
        else if (auto* meshJob = dynamic_cast<BuildMeshJob*>(task))
        {
            HandleBuildMeshCompleted(meshJob);
        }
        // Other task types can be handled here in the future

        // Delete the task (caller is responsible for cleanup)
        delete task;
    }
}

//-----------------------------------------------------------------------------------------------
// SubmitBuildMeshJob: Submit async mesh building job to ScheduleSubsystem
//-----------------------------------------------------------------------------------------------
void World::SubmitBuildMeshJob(Chunk* chunk, TaskPriority priority)
{
    if (!chunk)
    {
        LogError(LogWorld, "SubmitBuildMeshJob called with null chunk");
        return;
    }

    IntVec2 chunkCoords = chunk->GetChunkCoords();

    // Check job limit
    if (m_activeMeshBuildJobs >= m_maxMeshBuildJobs)
    {
        // Only log when limit reached (rare event, important to know)
        /*static int lastLoggedFrame = -1;
        int currentFrame = g_theApp->GetFrameNumber();
        if (currentFrame != lastLoggedFrame)
        {
            LogDebug(LogWorld, "Mesh build job limit reached (%d/%d), skipping new jobs this frame",
                     m_activeMeshBuildJobs.load(), m_maxMeshBuildJobs);
            lastLoggedFrame = currentFrame;
        }*/
        return;
    }

    // PERFORMANCE: Remove per-job debug logging (causes 60% of AddTask time due to console I/O)
    // Only log high-priority jobs (player interaction) for debugging
    // const char* priorityStr = (priority == TaskPriority::High) ? "High" : "Normal";
    // LogDebug(LogWorld, "Submitting BuildMeshJob for chunk (%d, %d) priority=%s - Active: %d/%d",
    //          chunkCoords.x, chunkCoords.y, priorityStr,
    //          m_activeMeshBuildJobs.load(), m_maxMeshBuildJobs);

    // Create and submit job
    auto* job = new BuildMeshJob(chunkCoords, chunk, priority);
    g_theSchedule->AddTask(job, priority);

    // Increment counter
    m_activeMeshBuildJobs++;
}

//-----------------------------------------------------------------------------------------------
// HandleBuildMeshCompleted: Process completed mesh build job on main thread
// CRITICAL: CompileToGPU() must run on main thread (DirectX 11 limitation)
//-----------------------------------------------------------------------------------------------
void World::HandleBuildMeshCompleted(BuildMeshJob* job)
{
    IntVec2 chunkCoords = job->GetChunkCoords();

    // Decrement active mesh job counter
    m_activeMeshBuildJobs--;

    // PERFORMANCE: Remove per-job debug logging (causes frame drops during mesh rebuild storms)
    // LogDebug(LogWorld, "Mesh build job completed for chunk (%d, %d) - Active: %d/%d",
    //          chunkCoords.x, chunkCoords.y,
    //          m_activeMeshBuildJobs.load(), m_maxMeshBuildJobs);

    // Get chunk from ChunkManager
    Chunk* chunk = job->GetChunk();
    if (!chunk)
    {
        LogWarn(LogWorld, "BuildMeshJob chunk pointer is null for (%d, %d)", chunkCoords.x, chunkCoords.y);
        return;
    }

    // Double-check chunk still exists in manager
    if (chunk != m_chunkManager->GetChunk(chunkCoords.x, chunkCoords.y))
    {
        LogWarn(LogWorld, "Chunk (%d, %d) no longer matches after mesh build", chunkCoords.x, chunkCoords.y);
        return;
    }

    // Check if job was cancelled
    if (job->IsCancelled())
    {
        // PERFORMANCE: Only log cancelled jobs (rare event)
        LogDebug(LogWorld, "BuildMeshJob for chunk (%d, %d) was cancelled", chunkCoords.x, chunkCoords.y);
        return;
    }

    // Take ownership of built mesh
    std::unique_ptr<ChunkMesh> newMesh = job->TakeMesh();
    if (!newMesh)
    {
        LogWarn(LogWorld, "BuildMeshJob produced null mesh for chunk (%d, %d)", chunkCoords.x, chunkCoords.y);
        return;
    }

    // MAIN THREAD ONLY: Compile mesh to GPU (DirectX 11 buffer creation)
    newMesh->CompileToGPU();

    // Set mesh on chunk and clear dirty flag
    chunk->SetMesh(std::move(newMesh));

    // PERFORMANCE: Remove per-job vertex count logging (use debug panel instead)
    // LogDebug(LogWorld, "Mesh compiled and set for chunk (%d, %d) vertices=%d",
    //          chunkCoords.x, chunkCoords.y,
    //          chunk->GetMesh() ? (int)chunk->GetMesh()->GetOpaqueVertexCount() + chunk->GetMesh()->GetTransparentVertexCount() : 0);
}
