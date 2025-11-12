#include "ChunkManager.hpp"

#include "ChunkStorageConfig.hpp"
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Engine/Resource/Atlas/TextureAtlas.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Renderer/IRenderer.hpp"
#include "Engine/Renderer/Texture.hpp" // Add explicit include for Texture class
using namespace enigma::voxel;

DEFINE_LOG_CATEGORY(LogChunk);

void ChunkManager::Initialize()
{
    // Cache the blocks atlas texture once during initialization (NeoForge pattern)
    auto* resourceSubsystem = GEngine->GetSubsystem<enigma::resource::ResourceSubsystem>();
    if (resourceSubsystem)
    {
        const auto* blocksAtlas = resourceSubsystem->GetAtlas("blocks");
        if (blocksAtlas)
        {
            m_cachedBlocksAtlasTexture = blocksAtlas->GetAtlasTexture();
            if (m_cachedBlocksAtlasTexture)
            {
                LogInfo(LogChunk, "ChunkManager: Cached blocks atlas texture successfully");
            }
            else
            {
                LogWarn(LogChunk, "ChunkManager: Blocks atlas texture is null, will render without texture");
            }
        }
        else
        {
            LogWarn(LogChunk, "ChunkManager: No 'blocks' atlas found");
        }
    }
    else
    {
        LogError(LogChunk, "ChunkManager: ResourceSubsystem not available during initialization");
    }
}


ChunkManager::ChunkManager(IChunkGenerationCallback* callback) :
    m_generationCallback(callback)
{
}

ChunkManager::~ChunkManager()
{
    // Save all modified chunks before destruction
    if (m_chunkStorage && m_chunkSerializer)
    {
        size_t savedCount = SaveAllModifiedChunks();
        if (savedCount > 0)
        {
            LogInfo(LogChunk, "ChunkManager: Saved %zu modified chunks during shutdown", savedCount);
        }
        FlushStorage();
    }

    m_loadedChunks.clear();
}

Chunk* ChunkManager::GetChunk(int32_t chunkX, int32_t chunkY)
{
    int64_t chunkPackID = PackCoordinates(chunkX, chunkY);
    return m_loadedChunks[chunkPackID].get();
}

// DEPRECATED: Synchronous loading - Use World::ActivateChunk() for async loading!
// Only kept for legacy EnsureChunksLoaded() support
void ChunkManager::LoadChunk(int32_t chunkX, int32_t chunkY)
{
    int64_t chunkPackID = PackCoordinates(chunkX, chunkY);
    if (m_loadedChunks.find(chunkPackID) != m_loadedChunks.end())
    {
        return; // Already loaded
    }

    // Simplified synchronous loading for emergency/legacy use
    auto chunk = std::make_unique<Chunk>(IntVec2(chunkX, chunkY));
    chunk->SetChunkManager(this); // [A05] Set ChunkManager for neighbor access
    if (m_generationCallback)
    {
        m_generationCallback->GenerateChunk(chunk.get(), chunkX, chunkY);
    }
    m_loadedChunks[chunkPackID] = std::move(chunk);
}

int64_t ChunkManager::PackCoordinates(int32_t x, int32_t y)
{
    // Convert signed 32-bit integers to unsigned for proper bit manipulation
    uint64_t ux = static_cast<uint64_t>(static_cast<uint32_t>(x));
    uint64_t uy = static_cast<uint64_t>(static_cast<uint32_t>(y));

    // Pack: high 32 bits = y, low 32 bits = x
    return static_cast<int64_t>((uy << 32) | ux);
}

void ChunkManager::UnpackCoordinates(int64_t packed, int32_t& x, int32_t& y)
{
    uint64_t upacked = static_cast<uint64_t>(packed);

    // Unpack: extract high and low 32 bits, then convert back to signed
    x = static_cast<int32_t>(static_cast<uint32_t>(upacked & 0xFFFFFFFF));
    y = static_cast<int32_t>(static_cast<uint32_t>(upacked >> 32));
}

void ChunkManager::Update(float deltaTime)
{
    // Update all loaded chunks
    for (const std::pair<const long long, std::unique_ptr<Chunk>>& loaded_chunk : m_loadedChunks)
    {
        loaded_chunk.second->Update(deltaTime);
    }

    // REMOVED: Synchronous mesh rebuild loop (now handled by World's async BuildMeshJob system)
    // Async mesh building provides:
    // - No main thread blocking (BuildMesh runs on worker threads)
    // - Priority system (High for player interaction, Normal for world generation)
    // - Better performance (parallel mesh building, O(1) priority queue)
}

void ChunkManager::Render(IRenderer* renderer)
{
    // Bind the cached blocks atlas texture once for all chunks (NeoForge pattern)
    if (m_cachedBlocksAtlasTexture)
    {
        renderer->BindTexture(m_cachedBlocksAtlasTexture);
    }

    for (const std::pair<const long long, std::unique_ptr<Chunk>>& loaded_chunk : m_loadedChunks)
    {
        if (m_enableChunkDebug)
        {
            renderer->BindTexture(nullptr);
            loaded_chunk.second->DebugDraw(renderer);
        }
        if (loaded_chunk.second)
            loaded_chunk.second->Render(renderer);
    }
}

bool ChunkManager::SetEnableChunkDebug(bool enable)
{
    m_enableChunkDebug = enable;
    return m_enableChunkDebug;
}

void ChunkManager::UnloadChunk(int32_t chunkX, int32_t chunkY)
{
    int64_t chunkPackID = PackCoordinates(chunkX, chunkY);
    auto    it          = m_loadedChunks.find(chunkPackID);
    if (it == m_loadedChunks.end())
    {
        return; // Chunk not loaded
    }

    Chunk* chunk = it->second.get();

    // ===== Phase 4: 指针有效性检查（崩溃防护点1） =====
    // 防止访问已被 worker 线程删除的 chunk
    if (!chunk)
    {
        LogWarn(LogChunk,
                "Chunk (%d, %d) pointer is null during unload, cleanup map entry",
                chunkX, chunkY);
        m_loadedChunks.erase(it);
        return;
    }

    // ===== Phase 4: 状态安全读取（崩溃防护点2） =====
    // 在访问 chunk 状态前再次验证指针（双重保险）
    ChunkState currentState = chunk->GetState();

    // Core logic: distinguish between Generating and other states
    if (currentState == ChunkState::Generating)
    {
        // Currently generating: use delayed deletion
        LogDebug(LogChunk, "Chunk (%d, %d) is generating, marking for delayed deletion", chunkX, chunkY);
        chunk->TrySetState(ChunkState::Generating, ChunkState::PendingUnload);

        // Transfer ownership to raw pointer for delayed deletion
        Chunk* rawChunk = it->second.release();
        m_loadedChunks.erase(it);
        MarkChunkForDeletion(rawChunk);
    }
    else
    {
        // Other states: immediate deletion (preserve original behavior)
        LogDebug(LogChunk, "Chunk (%d, %d) safe to unload immediately (state: %d)", chunkX, chunkY, (int)currentState);

        // Save chunk if modified and storage is configured
        if (chunk && chunk->NeedsMeshRebuild() && m_chunkStorage && m_chunkSerializer)
        {
            if (SaveChunkToDisk(chunk))
            {
                LogDebug(LogChunk, "Saved modified chunk (%d, %d) to disk", chunkX, chunkY);
            }
            else
            {
                LogWarn(LogChunk, "Failed to save modified chunk (%d, %d)", chunkX, chunkY);
            }
        }

        // Cleanup VBO resources to prevent GPU leaks
        if (chunk && chunk->GetMesh())
        {
            chunk->SetMesh(nullptr);
        }

        chunk->TrySetState(currentState, ChunkState::Inactive);
        m_loadedChunks.erase(it);
        // unique_ptr will automatically delete the chunk
    }
}

bool ChunkManager::IsChunkLoaded(int32_t chunkX, int32_t chunkY) const
{
    int64_t chunkPackID = PackCoordinates(chunkX, chunkY);
    return m_loadedChunks.find(chunkPackID) != m_loadedChunks.end();
}

void ChunkManager::SetPlayerPosition(const Vec3& playerPosition)
{
    m_playerPosition = playerPosition;
}

void ChunkManager::SetActivationRange(int32_t chunkDistance)
{
    m_activationRange   = chunkDistance;
    m_deactivationRange = chunkDistance + 2; // Add buffer for deactivation
    LogInfo(LogChunk, "Set activation range to %d chunks, deactivation range to %d chunks",
            m_activationRange, m_deactivationRange);
}

void ChunkManager::SetDeactivationRange(int32_t chunkDistance)
{
    m_deactivationRange = chunkDistance;
}

float ChunkManager::GetChunkDistanceToPlayer(int32_t chunkX, int32_t chunkY) const
{
    // Convert player position to chunk coordinates
    int32_t playerChunkX = static_cast<int32_t>(std::floor(m_playerPosition.x / Chunk::CHUNK_SIZE_X));
    int32_t playerChunkY = static_cast<int32_t>(std::floor(m_playerPosition.y / Chunk::CHUNK_SIZE_Y));

    // Calculate distance in chunk units
    float dx = static_cast<float>(chunkX - playerChunkX);
    float dy = static_cast<float>(chunkY - playerChunkY);
    return std::sqrt(dx * dx + dy * dy);
}

std::vector<std::pair<int32_t, int32_t>> ChunkManager::GetChunksInActivationRange() const
{
    std::vector<std::pair<int32_t, int32_t>> chunks;

    // Convert player position to chunk coordinates
    int32_t playerChunkX = static_cast<int32_t>(std::floor(m_playerPosition.x / Chunk::CHUNK_SIZE_X));
    int32_t playerChunkY = static_cast<int32_t>(std::floor(m_playerPosition.y / Chunk::CHUNK_SIZE_Y));

    // Find all chunks within activation range
    for (int32_t chunkX = playerChunkX - m_activationRange; chunkX <= playerChunkX + m_activationRange; ++chunkX)
    {
        for (int32_t chunkY = playerChunkY - m_activationRange; chunkY <= playerChunkY + m_activationRange; ++chunkY)
        {
            float distance = GetChunkDistanceToPlayer(chunkX, chunkY);
            if (distance <= static_cast<float>(m_activationRange))
            {
                chunks.emplace_back(chunkX, chunkY);
            }
        }
    }

    return chunks;
}

std::pair<int32_t, int32_t> ChunkManager::FindFarthestChunk() const
{
    std::pair<int32_t, int32_t> farthest    = {INT32_MAX, INT32_MAX};
    float                       maxDistance = -1.0f;

    for (const auto& [packedCoords, chunk] : m_loadedChunks)
    {
        int32_t chunkX, chunkY;
        UnpackCoordinates(packedCoords, chunkX, chunkY);

        float distance = GetChunkDistanceToPlayer(chunkX, chunkY);
        if (distance > maxDistance)
        {
            maxDistance = distance;
            farthest    = {chunkX, chunkY};
        }
    }

    return farthest;
}

std::pair<int32_t, int32_t> ChunkManager::FindNearestMissingChunk() const
{
    std::pair<int32_t, int32_t> nearest     = {INT32_MAX, INT32_MAX};
    float                       minDistance = FLT_MAX;

    auto chunksInRange = GetChunksInActivationRange();
    for (const auto& [chunkX, chunkY] : chunksInRange)
    {
        if (!IsChunkLoaded(chunkX, chunkY))
        {
            float distance = GetChunkDistanceToPlayer(chunkX, chunkY);
            if (distance < minDistance)
            {
                minDistance = distance;
                nearest     = {chunkX, chunkY};
            }
        }
    }

    return nearest;
}

Chunk* ChunkManager::FindNearestDirtyChunk() const
{
    Chunk* nearestDirty = nullptr;
    float  minDistance  = FLT_MAX;

    for (const auto& [packedCoords, chunk] : m_loadedChunks)
    {
        if (chunk && chunk->NeedsMeshRebuild())
        {
            int32_t chunkX, chunkY;
            UnpackCoordinates(packedCoords, chunkX, chunkY);

            float distance = GetChunkDistanceToPlayer(chunkX, chunkY);
            if (distance < minDistance)
            {
                minDistance  = distance;
                nearestDirty = chunk.get();
            }
        }
    }

    return nearestDirty;
}

// 新增：批量区块管理方法
void ChunkManager::EnsureChunksLoaded(const std::vector<std::pair<int32_t, int32_t>>& chunks)
{
    for (const auto& [chunkX, chunkY] : chunks)
    {
        if (!IsChunkLoaded(chunkX, chunkY))
        {
            LoadChunk(chunkX, chunkY);
        }
    }
}

void ChunkManager::UnloadDistantChunks(const Vec3& playerPos, int32_t maxDistance)
{
    std::vector<int64_t> chunksToUnload;

    // Calculate the chunk coordinates of the player's position
    // Chunk coordinates use (X, Y) horizontal plane, Z is vertical height
    int32_t playerChunkX = static_cast<int32_t>(std::floor(playerPos.x / Chunk::CHUNK_SIZE_X));
    int32_t playerChunkY = static_cast<int32_t>(std::floor(playerPos.y / Chunk::CHUNK_SIZE_Y));

    // Check all loaded blocks
    for (const auto& [packedCoords, chunk] : m_loadedChunks)
    {
        int32_t chunkX, chunkY;
        UnpackCoordinates(packedCoords, chunkX, chunkY);

        // Calculate distance
        float deltaX   = static_cast<float>(chunkX - playerChunkX);
        float deltaY   = static_cast<float>(chunkY - playerChunkY);
        float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);

        // If the distance exceeds the maximum distance, it is marked as unloading
        if (distance > static_cast<float>(maxDistance))
        {
            // If there is a callback interface, check whether it should be uninstalled
            bool shouldUnload = true;
            if (m_generationCallback)
            {
                shouldUnload = m_generationCallback->ShouldUnloadChunk(chunkX, chunkY);
            }

            if (shouldUnload)
            {
                chunksToUnload.push_back(packedCoords);
            }
        }
    }

    // Perform uninstall
    for (int64_t packedCoords : chunksToUnload)
    {
        int32_t chunkX, chunkY;
        UnpackCoordinates(packedCoords, chunkX, chunkY);
        UnloadChunk(chunkX, chunkY);
    }

    if (!chunksToUnload.empty())
    {
        LogDebug(LogChunk, "Unloaded %zu distant chunks", chunksToUnload.size());
    }
}

// Reserved: Serialization component setting method (empty implementation)
void ChunkManager::SetChunkSerializer(std::unique_ptr<IChunkSerializer> serializer)
{
    m_chunkSerializer = std::move(serializer);
    LogInfo(LogChunk, "ChunkManager: Chunk serializer configured");
}

void ChunkManager::SetChunkStorage(std::unique_ptr<IChunkStorage> storage)
{
    m_chunkStorage = std::move(storage);
    LogInfo(LogChunk, "ChunkManager: Chunk storage configured");
}

bool ChunkManager::SaveChunkToDisk(const Chunk* chunk)
{
    if (!chunk || !m_chunkStorage || !m_chunkSerializer)
    {
        LogWarn(LogChunk, "SaveChunkToDisk: Storage or serializer not configured");
        return false;
    }

    try
    {
        // Use GetChunkX() and GetChunkZ() instead of GetPosition()
        int32_t chunkX = chunk->GetChunkX();
        int32_t chunkZ = chunk->GetChunkY();
        return m_chunkStorage->SaveChunk(chunkX, chunkZ, chunk);
    }
    catch (const std::exception& e)
    {
        LogError(LogChunk, "SaveChunkToDisk failed: %s", e.what());
        return false;
    }
}

std::unique_ptr<Chunk> ChunkManager::LoadChunkFromDisk(int32_t chunkX, int32_t chunkY)
{
    if (!m_chunkStorage || !m_chunkSerializer)
    {
        LogWarn(LogChunk, "LoadChunkFromDisk: Storage or serializer not configured");
        return nullptr;
    }

    if (!m_chunkStorage->ChunkExists(chunkX, chunkY))
    {
        return nullptr; // Chunk doesn't exist on disk
    }

    try
    {
        auto chunk = std::make_unique<Chunk>(IntVec2(chunkX, chunkY));
        chunk->SetChunkManager(this); // [A05] Set ChunkManager for neighbor access
        if (m_chunkStorage->LoadChunk(chunkX, chunkY, chunk.get()))
        {
            LogDebug(LogChunk, "Successfully loaded chunk (%d, %d) from disk", chunkX, chunkY);
            return chunk;
        }
        else
        {
            LogWarn(LogChunk, "Failed to load chunk (%d, %d) from disk", chunkX, chunkY);
            return nullptr;
        }
    }
    catch (const std::exception& e)
    {
        LogError(LogChunk, "LoadChunkFromDisk failed: %s", e.what());
        return nullptr;
    }
}

size_t ChunkManager::SaveAllModifiedChunks()
{
    if (!m_chunkStorage || !m_chunkSerializer)
    {
        return 0;
    }

    size_t savedCount = 0;
    for (auto& [packedId, chunk] : m_loadedChunks)
    {
        if (chunk && chunk->IsModified())
        {
            if (SaveChunkToDisk(chunk.get()))
            {
                chunk->SetModified(false); // Clear modified flag after successful save
                savedCount++;
                int32_t chunkX, chunkY;
                UnpackCoordinates(packedId, chunkX, chunkY);
                LogDebug(LogChunk, "Saved modified chunk (%d, %d)", chunkX, chunkY);
            }
        }
    }

    if (savedCount > 0)
    {
        LogInfo(LogChunkSave, "Saved %zu modified chunks to disk", savedCount);
    }
    else
    {
        LogInfo(LogChunk, "No modified chunks to save (all chunks are unmodified)");
    }

    return savedCount;
}

void ChunkManager::FlushStorage()
{
    if (m_chunkStorage)
    {
        try
        {
            m_chunkStorage->Flush();
            LogDebug(LogChunk, "Storage flushed successfully");
        }
        catch (const std::exception& e)
        {
            LogError(LogChunk, "Failed to flush storage: %s", e.what());
        }
    }
}

// ============================================================================
// Delayed Deletion Management Implementation
// ============================================================================

void ChunkManager::MarkChunkForDeletion(Chunk* chunk)
{
    if (chunk == nullptr)
    {
        LogError(LogChunk, "Attempted to mark null chunk for deletion");
        return;
    }

    // Check if chunk is already in the pending deletion queue
    auto it = std::find(m_pendingDeleteChunks.begin(), m_pendingDeleteChunks.end(), chunk);
    if (it != m_pendingDeleteChunks.end())
    {
        LogWarn(LogChunk, "Chunk (%d, %d) already in pending deletion queue",
                chunk->GetChunkX(), chunk->GetChunkY());
        return;
    }

    m_pendingDeleteChunks.push_back(chunk);
    LogDebug(LogChunk, "Marked chunk (%d, %d) for deletion, queue size: %zu",
             chunk->GetChunkX(), chunk->GetChunkY(), m_pendingDeleteChunks.size());
}

void ChunkManager::ProcessPendingDeletions()
{
    if (m_pendingDeleteChunks.empty())
    {
        return;
    }

    std::vector<Chunk*> remainingChunks;
    int                 deletedCount = 0;

    for (Chunk* chunk : m_pendingDeleteChunks)
    {
        // ===== Phase 4: 空指针检查（崩溃防护点3） =====
        // 防止处理空指针或已删除的 chunk
        if (!chunk)
        {
            LogWarn(LogChunk,
                    "Found null chunk in pending deletion queue, skipping");
            continue;
        }

        ChunkState state = chunk->GetState();

        // Safe to delete if Inactive or PendingUnload (worker thread finished)
        if (state == ChunkState::Inactive || state == ChunkState::PendingUnload)
        {
            int32_t chunkX = chunk->GetChunkX();
            int32_t chunkZ = chunk->GetChunkY();

            // Cleanup VBO resources to prevent GPU leaks
            if (chunk->GetMesh())
            {
                chunk->SetMesh(nullptr);
            }

            // Delete the chunk object
            delete chunk;
            deletedCount++;

            LogDebug(LogChunk, "Safely deleted chunk (%d, %d)", chunkX, chunkZ);
        }
        else
        {
            // Still generating: keep for next frame
            remainingChunks.push_back(chunk);
            LogWarn(LogChunk, "Chunk (%d, %d) still in state %d, defer deletion",
                    chunk->GetChunkX(), chunk->GetChunkY(), static_cast<int>(state));
        }
    }

    // Update pending deletion queue
    m_pendingDeleteChunks = std::move(remainingChunks);

    if (deletedCount > 0)
    {
        LogDebug(LogChunk, "Processed deletions: %d deleted, %zu remaining",
                 deletedCount, m_pendingDeleteChunks.size());
    }
}

size_t ChunkManager::GetPendingDeletionCount() const
{
    return m_pendingDeleteChunks.size();
}
