#include "ChunkManager.hpp"
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Engine/Resource/Atlas/TextureAtlas.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Renderer/IRenderer.hpp"
#include "Engine/Renderer/Texture.hpp" // Add explicit include for Texture class
using namespace enigma::voxel::chunk;


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
                core::LogInfo("chunk", "ChunkManager: Cached blocks atlas texture successfully");
            }
            else
            {
                core::LogWarn("chunk", "ChunkManager: Blocks atlas texture is null, will render without texture");
            }
        }
        else
        {
            core::LogWarn("chunk", "ChunkManager: No 'blocks' atlas found");
        }
    }
    else
    {
        core::LogError("chunk", "ChunkManager: ResourceSubsystem not available during initialization");
    }
}


ChunkManager::ChunkManager(IChunkGenerationCallback* callback) :
    m_generationCallback(callback)
{
}

ChunkManager::~ChunkManager()
{
    m_loadedChunks.clear();
}

Chunk* ChunkManager::GetChunk(int32_t chunkX, int32_t chunkY)
{
    int64_t chunkPackID = PackCoordinates(chunkX, chunkY);
    return m_loadedChunks[chunkPackID].get();
}

void ChunkManager::LoadChunk(int32_t chunkX, int32_t chunkY)
{
    int64_t chunkPackID = PackCoordinates(chunkX, chunkY);
    if (m_loadedChunks.find(chunkPackID) == m_loadedChunks.end())
    {
        m_loadedChunks[chunkPackID] = std::make_unique<Chunk>(IntVec2(chunkX, chunkY));

        // Use the callback interface to generate block content
        if (m_generationCallback)
        {
            m_generationCallback->GenerateChunk(m_loadedChunks[chunkPackID].get(), chunkX, chunkY);
        }

        core::LogDebug("chunk", "Loaded chunk (%d, %d)", chunkX, chunkY);
    }
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

    // Perform frame-limited chunk activation/deactivation (Assignment 02 requirement)
    UpdateChunkActivation();
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
        // Call the chunk's Render method which handles coordinate transformation
        // Note: Texture binding is now handled by ChunkManager, so we can optimize Chunk::Render
        renderer->BindTexture(m_cachedBlocksAtlasTexture);
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
    if (it != m_loadedChunks.end())
    {
        core::LogInfo("chunk", "Unloading chunk: %d, %d", chunkX, chunkY);
        m_loadedChunks[chunkPackID] = nullptr;
        // TODO: Save chunk if modified (m_needsSaving)
        // TODO: Cleanup VBO resources to prevent GPU leaks
        m_loadedChunks.erase(it);
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
    core::LogInfo("chunk", "Set activation range to %d chunks, deactivation range to %d chunks",
                  m_activationRange, m_deactivationRange);
}

void ChunkManager::SetDeactivationRange(int32_t chunkDistance)
{
    m_deactivationRange = chunkDistance;
}

void ChunkManager::UpdateChunkActivation()
{
    // Assignment 02 requirement: Only ONE operation per frame

    // 1. Priority: Check for dirty chunks and regenerate the nearest one
    Chunk* dirtyChunk = FindNearestDirtyChunk();
    if (dirtyChunk)
    {
        m_lastFrameOperation = ChunkOperationType::CheckDirtyChunks;
        // Rebuild mesh for dirty chunk (Assignment 02 requirement)
        dirtyChunk->RebuildMesh();
        core::LogDebug("chunk", "Regenerated mesh for dirty chunk");
        return;
    }

    // Calculate max allowed chunks based on activation range
    int32_t maxChunks = (2 * m_activationRange + 1) * (2 * m_activationRange + 1);

    // 2. If we have fewer chunks than max, activate nearest missing chunk
    if (static_cast<int32_t>(m_loadedChunks.size()) < maxChunks)
    {
        auto missingChunk = FindNearestMissingChunk();
        if (missingChunk.first != INT32_MAX && missingChunk.second != INT32_MAX)
        {
            m_lastFrameOperation = ChunkOperationType::ActivateChunk;
            LoadChunk(missingChunk.first, missingChunk.second);
            core::LogDebug("chunk", "Activated chunk (%d, %d)", missingChunk.first, missingChunk.second);
            return;
        }
    }

    // 3. Otherwise, deactivate the farthest chunk if it's outside deactivation range
    auto farthestChunk = FindFarthestChunk();
    if (farthestChunk.first != INT32_MAX && farthestChunk.second != INT32_MAX)
    {
        float distance = GetChunkDistanceToPlayer(farthestChunk.first, farthestChunk.second);
        if (distance > static_cast<float>(m_deactivationRange))
        {
            m_lastFrameOperation = ChunkOperationType::DeactivateChunk;
            UnloadChunk(farthestChunk.first, farthestChunk.second);
            core::LogDebug("chunk", "Deactivated chunk (%d, %d) at distance %.1f",
                           farthestChunk.first, farthestChunk.second, distance);
            return;
        }
    }

    m_lastFrameOperation = ChunkOperationType::None;
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

    // Calculate the block coordinates of the player's position
    int32_t playerChunkX = static_cast<int32_t>(std::floor(playerPos.x / Chunk::CHUNK_SIZE_X));
    int32_t playerChunkY = static_cast<int32_t>(std::floor(playerPos.z / Chunk::CHUNK_SIZE_Y));

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
        core::LogDebug("chunk", "Unloaded %zu distant chunks", chunksToUnload.size());
    }
}

// Reserved: Serialization component setting method (empty implementation)
void ChunkManager::SetChunkSerializer(std::unique_ptr<IChunkSerializer> serializer)
{
    m_chunkSerializer = std::move(serializer);
    core::LogInfo("chunk", "ChunkManager: Chunk serializer configured");
}

void ChunkManager::SetChunkStorage(std::unique_ptr<IChunkStorage> storage)
{
    m_chunkStorage = std::move(storage);
    core::LogInfo("chunk", "ChunkManager: Chunk storage configured");
}

bool ChunkManager::SaveChunkToDisk(const Chunk* chunk)
{
    // Reserved interface: currently returns false, and the actual saving logic will be implemented in the future
    UNUSED(chunk)
    return false;
}

std::unique_ptr<Chunk> ChunkManager::LoadChunkFromDisk(int32_t chunkX, int32_t chunkY)
{
    // Reserved interface: currently returns nullptr, and the actual loading logic will be implemented in the future
    UNUSED(chunkX)
    UNUSED(chunkY)
    return nullptr;
}
