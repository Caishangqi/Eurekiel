#include "World.hpp"
#include "ESFWorldStorage.hpp"
#include "ESFSWorldStorage.hpp"
#include "../Chunk/ChunkStorageConfig.hpp"
#include "../Chunk/ESFSChunkSerializer.hpp"
#include "../Chunk/ChunkHelper.hpp"
#include "../Chunk/ChunkMeshHelper.hpp"
#include "../Block/PlacementContext.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Renderer/IRenderer.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Engine/Resource/Atlas/TextureAtlas.hpp"

#include <cfloat>
#include <algorithm>

#include "Engine/Registry/Block/Block.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"
#include "Engine/Voxel/Block/VoxelShape.hpp"
#include "Game/GameCommon.hpp"

using namespace enigma::voxel;

//-----------------------------------------------------------------------------------------------
// Helper function to convert impact normal to Direction enum
// Used by RaycastVsBlocks to determine which face of a VoxelShape was hit
//-----------------------------------------------------------------------------------------------
static Direction GetDirectionFromNormal(const Vec3& normal)
{
    // Find the dominant axis in the normal vector
    float absX = std::abs(normal.x);
    float absY = std::abs(normal.y);
    float absZ = std::abs(normal.z);

    if (absX >= absY && absX >= absZ)
    {
        // X axis is dominant
        return (normal.x > 0.0f) ? Direction::EAST : Direction::WEST;
    }
    else if (absY >= absX && absY >= absZ)
    {
        // Y axis is dominant
        return (normal.y > 0.0f) ? Direction::NORTH : Direction::SOUTH;
    }
    else
    {
        // Z axis is dominant
        return (normal.z > 0.0f) ? Direction::UP : Direction::DOWN;
    }
}

World::~World()
{
}

World::World(const std::string& worldName, uint64_t worldSeed, std::unique_ptr<enigma::voxel::TerrainGenerator> generator) : m_worldName(worldName), m_worldSeed(worldSeed)
{
    using namespace enigma::voxel;

    // [NEW] Initialize VoxelLightEngine
    m_voxelLightEngine = std::make_unique<VoxelLightEngine>();

    // Create and initialize ChunkManager (follow NeoForge mode)

    // Initialize the ESF storage system
    if (!InitializeWorldStorage(WORLD_SAVE_PATH))
    {
        LogError("world", "Failed to initialize world storage system");
        ERROR_AND_DIE("Failed to initialize world storage system")
    }
    LogInfo("world", "World storage system initialized in: %s", WORLD_SAVE_PATH);

    // Set up the world generator
    SetWorldGenerator(std::move(generator));
    LogInfo("world", "World fully initialized with name: %s", m_worldName.c_str());

    m_cachedBlocksAtlasTexture = g_theResource->GetAtlas("blocks")->GetAtlasTexture();
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

        // Mark chunk for mesh rebuild on main thread
        // This ensures visual feedback when player places/breaks blocks
        const_cast<World*>(this)->ScheduleChunkMeshRebuild(chunk);
    }
}

uint8_t World::GetSkyLight(int32_t globalX, int32_t globalY, int32_t globalZ) const
{
    // [REFACTORED] Delegate to VoxelLightEngine
    return m_voxelLightEngine->GetSkyLight(BlockPos{globalX, globalY, globalZ});
}

uint8_t World::GetBlockLight(int32_t globalX, int32_t globalY, int32_t globalZ) const
{
    // [REFACTORED] Delegate to VoxelLightEngine
    return m_voxelLightEngine->GetBlockLight(BlockPos{globalX, globalY, globalZ});
}

bool World::GetIsSky(int32_t globalX, int32_t globalY, int32_t globalZ) const
{
    // Calculate chunk coordinates
    int32_t chunkX = globalX >> 4;
    int32_t chunkY = globalY >> 4;

    // Get chunk
    Chunk* chunk = GetChunk(chunkX, chunkY);
    if (!chunk) return false;

    // Calculate local coordinates
    int32_t localX = globalX & 0x0F;
    int32_t localY = globalY & 0x0F;
    int32_t localZ = globalZ;

    // Delegate to Chunk
    return chunk->GetIsSky(localX, localY, localZ);
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
    Chunk* chunk = GetChunk(pos.GetBlockXInChunk(), pos.GetBlockYInChunk());
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

//-----------------------------------------------------------------------------------------------
// RaycastVsBlocks - Fast Voxel Raycast (3D DDA Algorithm)
// Maintains BlockIterator for efficient cross-chunk navigation
// Based on 2D Fast Voxel Raycast from Libra/Map.cpp:853
//-----------------------------------------------------------------------------------------------
VoxelRaycastResult3D World::RaycastVsBlocks(const Vec3& rayStart, const Vec3& rayFwdNormal, float rayMaxLength) const
{
    VoxelRaycastResult3D result;
    result.m_rayStartPos  = rayStart;
    result.m_rayFwdNormal = rayFwdNormal;
    result.m_rayMaxLength = rayMaxLength;

    // [STEP 1] Initialize starting block iterator
    // Convert Vec3 world position to BlockPos using floor()
    BlockPos startBlockPos(
        static_cast<int32_t>(std::floor(rayStart.x)),
        static_cast<int32_t>(std::floor(rayStart.y)),
        static_cast<int32_t>(std::floor(rayStart.z))
    );

    Chunk* startChunk = GetChunkAt(startBlockPos);
    if (!startChunk)
    {
        return result; // Ray starts outside loaded world
    }

    // Convert world BlockPos to local chunk coordinates
    int32_t localX, localY, localZ;
    if (!startChunk->WorldToLocal(startBlockPos, localX, localY, localZ))
    {
        return result; // Invalid coordinates (shouldn't happen if GetChunkAt succeeded)
    }

    // Create BlockIterator using local coordinates converted to blockIndex
    int           blockIndex = static_cast<int>(Chunk::CoordsToIndex(localX, localY, localZ));
    BlockIterator currentIter(startChunk, blockIndex);
    BlockState*   startState = currentIter.GetBlock();
    if (startState)
    {
        if (startState->IsFullOpaque())
        {
            // Starting inside full opaque block - immediate hit
            result.m_didImpact    = true;
            result.m_impactPos    = rayStart;
            result.m_impactDist   = 0.0f;
            result.m_impactNormal = -rayFwdNormal; // Arbitrary normal when starting inside
            result.m_hitBlockIter = currentIter;
            result.m_hitFace      = Direction::NORTH; // Arbitrary face
            return result;
        }
        else
        {
            // Starting inside non-full block - test if inside collision shape
            registry::block::Block* block = startState->GetBlock();
            if (block)
            {
                VoxelShape collisionShape = block->GetCollisionShape(startState);
                if (!collisionShape.IsEmpty())
                {
                    // Calculate local position within block
                    Vec3 localPos(rayStart.x - std::floor(rayStart.x),
                                  rayStart.y - std::floor(rayStart.y),
                                  rayStart.z - std::floor(rayStart.z));
                    if (collisionShape.IsPointInside(localPos))
                    {
                        result.m_didImpact    = true;
                        result.m_impactPos    = rayStart;
                        result.m_impactDist   = 0.0f;
                        result.m_impactNormal = -rayFwdNormal;
                        result.m_hitBlockIter = currentIter;
                        result.m_hitFace      = Direction::NORTH;
                        return result;
                    }
                }
            }
        }
    }

    // [STEP 2] DDA Initialization - Calculate step directions and delta distances

    // X-axis DDA parameters
    float fwdDistPerXCrossing    = (rayFwdNormal.x != 0.0f) ? std::abs(1.0f / rayFwdNormal.x) : FLT_MAX;
    int   tileStepDirectionX     = (rayFwdNormal.x < 0.0f) ? -1 : 1;
    float xAtFirstXCrossing      = (tileStepDirectionX > 0) ? (std::floor(rayStart.x) + 1.0f) : std::floor(rayStart.x);
    float xDistToFirstXCrossing  = (xAtFirstXCrossing - rayStart.x) * static_cast<float>(tileStepDirectionX);
    float fwdDistAtNextXCrossing = std::abs(xDistToFirstXCrossing) * fwdDistPerXCrossing;

    // Y-axis DDA parameters
    float fwdDistPerYCrossing    = (rayFwdNormal.y != 0.0f) ? std::abs(1.0f / rayFwdNormal.y) : FLT_MAX;
    int   tileStepDirectionY     = (rayFwdNormal.y < 0.0f) ? -1 : 1;
    float yAtFirstYCrossing      = (tileStepDirectionY > 0) ? (std::floor(rayStart.y) + 1.0f) : std::floor(rayStart.y);
    float yDistToFirstYCrossing  = (yAtFirstYCrossing - rayStart.y) * static_cast<float>(tileStepDirectionY);
    float fwdDistAtNextYCrossing = std::abs(yDistToFirstYCrossing) * fwdDistPerYCrossing;

    // Z-axis DDA parameters
    float fwdDistPerZCrossing    = (rayFwdNormal.z != 0.0f) ? std::abs(1.0f / rayFwdNormal.z) : FLT_MAX;
    int   tileStepDirectionZ     = (rayFwdNormal.z < 0.0f) ? -1 : 1;
    float zAtFirstZCrossing      = (tileStepDirectionZ > 0) ? (std::floor(rayStart.z) + 1.0f) : std::floor(rayStart.z);
    float zDistToFirstZCrossing  = (zAtFirstZCrossing - rayStart.z) * static_cast<float>(tileStepDirectionZ);
    float fwdDistAtNextZCrossing = std::abs(zDistToFirstZCrossing) * fwdDistPerZCrossing;

    // [STEP 3] DDA Main Loop - Step through voxel grid until hit or max distance
    while (true)
    {
        // Determine which axis to step along (choose minimum tMax)
        if (fwdDistAtNextXCrossing < fwdDistAtNextYCrossing && fwdDistAtNextXCrossing < fwdDistAtNextZCrossing)
        {
            // Step along X-axis
            if (fwdDistAtNextXCrossing > rayMaxLength)
            {
                break; // Ray missed - exceeded max distance
            }

            // Move to next block in X direction using BlockIterator
            Direction moveDir = (tileStepDirectionX > 0) ? Direction::EAST : Direction::WEST;
            currentIter       = currentIter.GetNeighbor(moveDir);

            if (!currentIter.IsValid())
            {
                break; // Stepped outside loaded chunks
            }

            // Check for solid block or non-full block with collision shape
            BlockState* state = currentIter.GetBlock();
            if (state)
            {
                if (state->IsFullOpaque())
                {
                    // Full opaque block - immediate hit
                    result.m_didImpact    = true;
                    result.m_impactDist   = fwdDistAtNextXCrossing;
                    result.m_impactPos    = rayStart + rayFwdNormal * fwdDistAtNextXCrossing;
                    result.m_impactNormal = Vec3(static_cast<float>(-tileStepDirectionX), 0.0f, 0.0f);
                    result.m_hitBlockIter = currentIter;
                    result.m_hitFace      = (tileStepDirectionX > 0) ? Direction::WEST : Direction::EAST;
                    return result;
                }
                else
                {
                    // Non-full block (slab, stairs, etc.) - test against collision shape
                    registry::block::Block* block = state->GetBlock();
                    if (block)
                    {
                        VoxelShape collisionShape = block->GetCollisionShape(state);
                        if (!collisionShape.IsEmpty())
                        {
                            // Get block world position from iterator
                            BlockPos blockPos = currentIter.GetBlockPos();
                            Vec3     blockWorldPos(static_cast<float>(blockPos.x), static_cast<float>(blockPos.y), static_cast<float>(blockPos.z));

                            // Raycast against collision shape
                            RaycastResult3D shapeResult = collisionShape.RaycastWorld(rayStart, rayFwdNormal, rayMaxLength, blockWorldPos);
                            if (shapeResult.m_didImpact)
                            {
                                result.m_didImpact    = true;
                                result.m_impactDist   = shapeResult.m_impactDist;
                                result.m_impactPos    = shapeResult.m_impactPos;
                                result.m_impactNormal = shapeResult.m_impactNormal;
                                result.m_hitBlockIter = currentIter;
                                // Determine hit face from actual collision normal (not DDA step direction)
                                result.m_hitFace = GetDirectionFromNormal(shapeResult.m_impactNormal);
                                return result;
                            }
                        }
                    }
                }
            }

            fwdDistAtNextXCrossing += fwdDistPerXCrossing;
        }
        else if (fwdDistAtNextYCrossing < fwdDistAtNextZCrossing)
        {
            // Step along Y-axis
            if (fwdDistAtNextYCrossing > rayMaxLength)
            {
                break; // Ray missed
            }

            Direction moveDir = (tileStepDirectionY > 0) ? Direction::NORTH : Direction::SOUTH;
            currentIter       = currentIter.GetNeighbor(moveDir);

            if (!currentIter.IsValid())
            {
                break;
            }

            // Check for solid block or non-full block with collision shape
            BlockState* state = currentIter.GetBlock();
            if (state)
            {
                if (state->IsFullOpaque())
                {
                    // Full opaque block - immediate hit
                    result.m_didImpact    = true;
                    result.m_impactDist   = fwdDistAtNextYCrossing;
                    result.m_impactPos    = rayStart + rayFwdNormal * fwdDistAtNextYCrossing;
                    result.m_impactNormal = Vec3(0.0f, static_cast<float>(-tileStepDirectionY), 0.0f);
                    result.m_hitBlockIter = currentIter;
                    result.m_hitFace      = (tileStepDirectionY > 0) ? Direction::SOUTH : Direction::NORTH;
                    return result;
                }
                else
                {
                    // Non-full block (slab, stairs, etc.) - test against collision shape
                    registry::block::Block* block = state->GetBlock();
                    if (block)
                    {
                        VoxelShape collisionShape = block->GetCollisionShape(state);
                        if (!collisionShape.IsEmpty())
                        {
                            // Get block world position from iterator
                            BlockPos blockPos = currentIter.GetBlockPos();
                            Vec3     blockWorldPos(static_cast<float>(blockPos.x), static_cast<float>(blockPos.y), static_cast<float>(blockPos.z));

                            // Raycast against collision shape
                            RaycastResult3D shapeResult = collisionShape.RaycastWorld(rayStart, rayFwdNormal, rayMaxLength, blockWorldPos);
                            if (shapeResult.m_didImpact)
                            {
                                result.m_didImpact    = true;
                                result.m_impactDist   = shapeResult.m_impactDist;
                                result.m_impactPos    = shapeResult.m_impactPos;
                                result.m_impactNormal = shapeResult.m_impactNormal;
                                result.m_hitBlockIter = currentIter;
                                // Determine hit face from actual collision normal (not DDA step direction)
                                result.m_hitFace = GetDirectionFromNormal(shapeResult.m_impactNormal);
                                return result;
                            }
                        }
                    }
                }
            }

            fwdDistAtNextYCrossing += fwdDistPerYCrossing;
        }
        else
        {
            // Step along Z-axis
            if (fwdDistAtNextZCrossing > rayMaxLength)
            {
                break; // Ray missed
            }

            Direction moveDir = (tileStepDirectionZ > 0) ? Direction::UP : Direction::DOWN;
            currentIter       = currentIter.GetNeighbor(moveDir);

            if (!currentIter.IsValid())
            {
                break;
            }

            // Check for solid block or non-full block with collision shape
            BlockState* state = currentIter.GetBlock();
            if (state)
            {
                if (state->IsFullOpaque())
                {
                    // Full opaque block - immediate hit
                    result.m_didImpact    = true;
                    result.m_impactDist   = fwdDistAtNextZCrossing;
                    result.m_impactPos    = rayStart + rayFwdNormal * fwdDistAtNextZCrossing;
                    result.m_impactNormal = Vec3(0.0f, 0.0f, static_cast<float>(-tileStepDirectionZ));
                    result.m_hitBlockIter = currentIter;
                    result.m_hitFace      = (tileStepDirectionZ > 0) ? Direction::DOWN : Direction::UP;
                    return result;
                }
                else
                {
                    // Non-full block (slab, stairs, etc.) - test against collision shape
                    registry::block::Block* block = state->GetBlock();
                    if (block)
                    {
                        VoxelShape collisionShape = block->GetCollisionShape(state);
                        if (!collisionShape.IsEmpty())
                        {
                            // Get block world position from iterator
                            BlockPos blockPos = currentIter.GetBlockPos();
                            Vec3     blockWorldPos(static_cast<float>(blockPos.x), static_cast<float>(blockPos.y), static_cast<float>(blockPos.z));

                            // Raycast against collision shape
                            RaycastResult3D shapeResult = collisionShape.RaycastWorld(rayStart, rayFwdNormal, rayMaxLength, blockWorldPos);
                            if (shapeResult.m_didImpact)
                            {
                                result.m_didImpact    = true;
                                result.m_impactDist   = shapeResult.m_impactDist;
                                result.m_impactPos    = shapeResult.m_impactPos;
                                result.m_impactNormal = shapeResult.m_impactNormal;
                                result.m_hitBlockIter = currentIter;
                                // Determine hit face from actual collision normal (not DDA step direction)
                                result.m_hitFace = GetDirectionFromNormal(shapeResult.m_impactNormal);
                                return result;
                            }
                        }
                    }
                }
            }

            fwdDistAtNextZCrossing += fwdDistPerZCrossing;
        }
    }

    // [STEP 4] No hit - return miss result
    result.m_impactDist = rayMaxLength;
    result.m_impactPos  = rayStart + rayFwdNormal * rayMaxLength;
    return result;
}

Chunk* World::GetChunk(int32_t chunkX, int32_t chunkY)
{
    // 改为直接访问 m_loadedChunks（迁移自 ChunkManager）
    int64_t chunkPackID = ChunkHelper::PackCoordinates(chunkX, chunkY);
    auto    it          = m_loadedChunks.find(chunkPackID);
    if (it != m_loadedChunks.end())
    {
        return it->second.get();
    }
    return nullptr;
}

Chunk* World::GetChunk(int32_t chunkX, int32_t chunkY) const
{
    // const version - 改为直接访问 m_loadedChunks（迁移自 ChunkManager）
    int64_t chunkPackID = ChunkHelper::PackCoordinates(chunkX, chunkY);
    auto    it          = m_loadedChunks.find(chunkPackID);
    if (it != m_loadedChunks.end())
    {
        return it->second.get();
    }
    return nullptr;
}

void World::UnloadChunkDirect(int32_t chunkX, int32_t chunkY)
{
    int64_t chunkPackID = ChunkHelper::PackCoordinates(chunkX, chunkY);
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
        LogWarn("world",
                "Chunk (%d, %d) pointer is null during unload, cleanup map entry",
                chunkX, chunkY);
        m_loadedChunks.erase(it);
        return;
    }

    // Clean dirty light queue before deactivating chunk
    UndirtyAllBlocksInChunk(chunk);

    // ===== Phase 4: 状态安全读取（崩溃防护点2） =====
    // 在访问 chunk 状态前再次验证指针（双重保险）
    ChunkState currentState = chunk->GetState();

    // Core logic: distinguish between Generating and other states
    if (currentState == ChunkState::Generating)
    {
        // Currently generating: use delayed deletion
        LogDebug("world", "Chunk (%d, %d) is generating, marking for delayed deletion", chunkX, chunkY);
        chunk->TrySetState(ChunkState::Generating, ChunkState::PendingUnload);

        // [REMOVED] Delayed deletion - now using PendingUnload state
        // Chunk will be deleted in next Update() cycle
        m_loadedChunks.erase(it);
    }
    else
    {
        // Other states: immediate deletion (preserve original behavior)
        LogDebug("world", "Chunk (%d, %d) safe to unload immediately (state: %d)", chunkX, chunkY, (int)currentState);

        // Save chunk if modified and storage is configured
        if (chunk && chunk->NeedsMeshRebuild() && m_chunkStorage && m_chunkSerializer)
        {
            // 保存到磁盘的逻辑（简化版，完整版在 SaveChunkToDisk 中）
            // 这里只做基本的序列化保存
            LogDebug("world", "Saving modified chunk (%d, %d) to disk", chunkX, chunkY);
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

Chunk* World::GetChunkAt(const BlockPos& pos) const
{
    return GetChunk(pos.GetChunkX(), pos.GetChunkY());
}

bool World::IsChunkLoaded(int32_t chunkX, int32_t chunkY)
{
    Chunk* chunk = GetChunk(chunkX, chunkY);
    return chunk != nullptr && chunk->GetState() == ChunkState::Active;
}

void World::UpdateNearbyChunks()
{
    auto neededChunks = CalculateNeededChunks();

    // Phase 3 Optimization: Process up to 10 chunks per frame (vs original 1)
    // This dramatically improves initial loading speed for new maps
    constexpr int MAX_ACTIVATIONS_PER_FRAME = 4;
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
        if (IsChunkLoadedDirect(chunkX, chunkY))
        {
            continue; // Already loaded, skip
        }

        // Phase 3: Check if chunk is already being processed using IsInQueue()
        IntVec2 coords = IntVec2(
            chunkX, chunkY);
        bool isPending = IsInQueue(m_pendingLoadQueue, coords) ||
            IsInQueue(m_pendingGenerateQueue, coords) ||
            IsInQueue(m_pendingSaveQueue, coords);

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
        LogDebug("world", "Activated %d chunks this frame", activatedThisFrame);
    }

    // [NEW] Phase 6: Dynamic unload rate based on loaded chunk count
    // Assignment 02 requirement: deactivate chunks per frame if outside range
    size_t loadedCount = m_loadedChunks.size();
    size_t targetCount = CalculateNeededChunks().size();

    int unloadsThisFrame = 1; // Default: 1 chunk/frame (original design)
    if (loadedCount > targetCount * 1.5f)
    {
        unloadsThisFrame = MAX_ACTIVATIONS_PER_FRAME; // Overload 50%: fast cleanup
    }
    else if (loadedCount > targetCount * 1.2f)
    {
        unloadsThisFrame = (int)MAX_ACTIVATIONS_PER_FRAME / 2; // Overload 20%: medium cleanup
    }

    for (int i = 0; i < unloadsThisFrame; ++i)
    {
        UnloadFarthestChunk();
    }
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
    UNUSED(deltaTime)
    // Phase 3: Update nearby chunks (activate/deactivate based on player position)
    UpdateNearbyChunks();

    // Phase 4: Process job queues with limits (submit pending → active)
    ProcessJobQueues();

    // Phase 4: Remove distant jobs from pending queues
    RemoveDistantJobs();

    // Process completed chunk tasks from async workers (Phase 3)
    ProcessCompletedChunkTasks();

    // [REFACTORED] Process dirty lighting via VoxelLightEngine
    m_voxelLightEngine->RunLightUpdates();

    // Phase 1: Update chunk meshes on main thread (Assignment 03 requirement)
    UpdateChunkMeshes();
}


bool World::SetEnableChunkDebug(bool enable)
{
    return SetEnableChunkDebugDirect(enable);
}


void World::SetPlayerPosition(const Vec3& position)
{
    m_playerPosition = position;
    // Sync player position to ChunkManager for distance-based chunk loading
}

void World::SetChunkActivationRange(int chunkDistance)
{
    m_chunkActivationRange = chunkDistance;
    // [FIX] Sync deactivation range = activation range + 2 (buffer zone)
    // This prevents ping-pong effect where chunks are created and immediately removed
    m_chunkDeactivationRange = chunkDistance + 2;
    LogInfo("world", "Set chunk activation range to %d chunks (deactivation: %d)",
            chunkDistance, m_chunkDeactivationRange);
}

void World::SetWorldGenerator(std::unique_ptr<enigma::voxel::TerrainGenerator> generator)
{
    m_worldGenerator = std::move(generator);
    if (m_worldGenerator)
    {
        // Initialize the generator with world seed
        m_worldGenerator->Initialize(static_cast<uint32_t>(m_worldSeed));
        LogInfo("world", "World generator set and initialized for world '%s'", m_worldName.c_str());
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
    LogInfo("world", "Chunk serializer configured for world '%s'", m_worldName.c_str());
}

void World::SetChunkStorage(std::unique_ptr<IChunkStorage> storage)
{
    m_chunkStorage = std::move(storage);
    LogInfo("world", "Chunk storage configured for world '%s'", m_worldName.c_str());
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
            LogError("world", "Failed to create world '%s' at path '%s'", m_worldName.c_str(), m_worldPath.c_str());
            return false;
        }

        LogInfo("world", "Created new world '%s' at '%s'", m_worldName.c_str(), m_worldPath.c_str());
    }
    else
    {
        LogInfo("world", "Found existing world '%s' at '%s'", m_worldName.c_str(), m_worldPath.c_str());
    }

    // Load ChunkStorageConfig from YAML
    ChunkStorageConfig config = ChunkStorageConfig::LoadFromYaml("");
    LogInfo("world", "Loaded chunk storage config: %s", config.ToString().c_str());

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
        // [REMOVED] m_chunkManager->SetChunkSerializer(std::move(esfsSerializerForManager));

        auto esfsStorageForManager = std::make_unique<ESFSChunkStorage>(m_worldPath, config, serializerPtr);
        // [REMOVED] m_chunkManager->SetChunkStorage(std::move(esfsStorageForManager));

        LogInfo("world", "World storage initialized with ESFS format (RLE compression)");
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
        // [REMOVED] m_chunkManager->SetChunkStorage(std::move(esfStorageForManager));

        LogInfo("world", "World storage initialized with ESF format (region files)");
    }
    else
    {
        LogError("world", "Unknown storage format in config");
        return false;
    }

    LogInfo("world", "World storage initialized for '%s'", m_worldName.c_str());
    return true;
}

bool World::SaveWorld()
{
    if (!m_worldManager)
    {
        LogError("world", "Cannot save world '%s' - world manager not initialized", m_worldName.c_str());
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
        LogError("world", "Failed to save world info for '%s'", m_worldName.c_str());
        return false;
    }

    // Force save all block data

    LogInfo("world", "World '%s' saved successfully", m_worldName.c_str());
    return true;
}

bool World::LoadWorld()
{
    if (!m_worldManager)
    {
        LogError("world", "Cannot load world '%s' - world manager not initialized", m_worldName.c_str());
        return false;
    }

    // Load world information
    ESFWorldManager::WorldInfo worldInfo;
    if (!m_worldManager->LoadWorldInfo(worldInfo))
    {
        LogError("world", "Failed to load world info for '%s'", m_worldName.c_str());
        return false;
    }

    // Update world parameters
    m_worldSeed        = worldInfo.worldSeed;
    m_playerPosition.x = static_cast<float>(worldInfo.spawnX);
    m_playerPosition.y = static_cast<float>(worldInfo.spawnY);
    m_playerPosition.z = static_cast<float>(worldInfo.spawnZ);

    LogInfo("world", "World '%s' loaded successfully (seed: %llu, spawn: %d,%d,%d)",
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
        LogInfo("world", "Chunk storage closed for world '%s'", m_worldName.c_str());
    }

    // Reset the manager
    m_worldManager.reset();

    LogInfo("world", "World '%s' closed", m_worldName.c_str());
}

//-------------------------------------------------------------------------------------------
// Phase 3: Async Task Management Implementation
// Uses global g_theSchedule for task submission
//-------------------------------------------------------------------------------------------

void World::ActivateChunk(IntVec2 chunkCoords)
{
    // Phase 4: ASYNC path with job queue - add to pending queue instead of immediate submission
    // ProcessJobQueues() will submit jobs when active count < limit

    Chunk* chunk = GetChunk(chunkCoords.x, chunkCoords.y);
    if (!chunk)
    {
        // Create EMPTY chunk object (no generation yet)
        int64_t packedCoords = ChunkHelper::PackCoordinates(chunkCoords.x, chunkCoords.y);
        auto&   loadedChunks = GetLoadedChunks();

        auto newChunk = std::make_unique<Chunk>(chunkCoords);
        chunk         = newChunk.get();
        chunk->SetWorld(this); // [FIX] 设置m_world指针，使GetEastNeighbor()等方法能工作
        loadedChunks[packedCoords] = std::move(newChunk);

        LogDebug("world", "Created empty chunk (%d, %d) for async activation", chunkCoords.x, chunkCoords.y);
    }

    // Transition from Inactive to CheckingDisk
    ChunkState currentState = chunk->GetState();
    if (currentState != ChunkState::Inactive)
    {
        // Chunk already being processed or active
        LogDebug("world", "Chunk (%d, %d) already in state %d, skipping activation",
                 chunkCoords.x, chunkCoords.y, static_cast<int>(currentState));
        return;
    }

    if (!chunk->TrySetState(ChunkState::Inactive, ChunkState::CheckingDisk))
    {
        LogWarn("world", "Failed to transition chunk (%d, %d) to CheckingDisk state", chunkCoords.x, chunkCoords.y);
        return;
    }

    LogDebug("world", "Chunk (%d, %d) transitioned to CheckingDisk", chunkCoords.x, chunkCoords.y);

    // Check if chunk exists on disk
    bool chunkExistsOnDisk = false;
    if (m_chunkStorage)
    {
        chunkExistsOnDisk = m_chunkStorage->ChunkExists(chunkCoords.x, chunkCoords.y);
        LogDebug("world", "Chunk (%d, %d) disk check: %s",
                 chunkCoords.x, chunkCoords.y,
                 chunkExistsOnDisk ? "EXISTS" : "NOT_FOUND");
    }
    else
    {
        LogDebug("world", "Chunk (%d, %d) no storage configured, will generate", chunkCoords.x, chunkCoords.y);
    }

    if (chunkExistsOnDisk)
    {
        // Phase 4: Transition to PendingLoad and add to queue (ProcessJobQueues() will submit)
        if (chunk->TrySetState(ChunkState::CheckingDisk, ChunkState::PendingLoad))
        {
            m_pendingLoadQueue.push_back(chunkCoords);
            LogDebug("world", "Chunk (%d, %d) added to load queue (size: %zu)",
                     chunkCoords.x, chunkCoords.y, m_pendingLoadQueue.size());
        }
    }
    else
    {
        // Phase 4: Transition to PendingGenerate and add to queue (ProcessJobQueues() will submit)
        if (chunk->TrySetState(ChunkState::CheckingDisk, ChunkState::PendingGenerate))
        {
            m_pendingGenerateQueue.push_back(chunkCoords);
            LogDebug("world", "Chunk (%d, %d) added to generate queue (size: %zu)",
                     chunkCoords.x, chunkCoords.y, m_pendingGenerateQueue.size());
        }
    }
}

void World::SubmitGenerateChunkJob(IntVec2 chunkCoords, Chunk* chunk)
{
    // ===== Phase 5: Shutdown保护（防护点1） =====
    if (m_isShuttingDown.load())
    {
        LogDebug("world", "SubmitGenerateChunkJob rejected: world is shutting down");
        return;
    }

    // Phase 3: Use IsInQueue() instead of tracking set
    if (IsInQueue(m_pendingGenerateQueue, chunkCoords))
    {
        return; // Already in queue
    }

    if (!g_theSchedule)
    {
        LogError("world", "Cannot submit GenerateChunkJob - g_theSchedule not initialized");
        return;
    }

    if (!m_worldGenerator)
    {
        LogError("world", "Cannot submit GenerateChunkJob - WorldGenerator not set");
        return;
    }

    // Create GenerateChunkJob and transfer ownership to ScheduleSubsystem
    // [REFACTORED] Pass World* instead of Chunk* - Job will get Chunk via GetChunk()
    GenerateChunkJob* job = new GenerateChunkJob(
        chunkCoords,
        this,
        m_worldGenerator.get(),
        static_cast<uint32_t>(m_worldSeed)
    );

    // Submit to global ScheduleSubsystem (transfers ownership)
    g_theSchedule->AddTask(job);

    // Phase 3: Set chunk state (no tracking set needed)
    chunk->SetState(ChunkState::Generating);

    LogDebug("world", "Submitted GenerateChunkJob for chunk (%d, %d)", chunkCoords.x, chunkCoords.y);
}

void World::SubmitLoadChunkJob(IntVec2 chunkCoords, Chunk* chunk)
{
    // Phase 3: Use IsInQueue() instead of tracking set
    if (IsInQueue(m_pendingLoadQueue, chunkCoords))
    {
        return; // Already in queue
    }

    if (!g_theSchedule)
    {
        LogError("world", "Cannot submit LoadChunkJob - g_theSchedule not initialized");
        return;
    }

    if (!m_chunkStorage)
    {
        LogError("world", "Cannot submit LoadChunkJob - ChunkStorage not set");
        return;
    }

    // Try casting to ESFChunkStorage first
    ESFChunkStorage* esfStorage = dynamic_cast<ESFChunkStorage*>(m_chunkStorage.get());
    if (esfStorage)
    {
        // Create LoadChunkJob for ESF format and transfer ownership to ScheduleSubsystem
        // [REFACTORED] Pass World* instead of Chunk* - Job will get Chunk via GetChunk()
        LoadChunkJob* job = new LoadChunkJob(chunkCoords, this, esfStorage);
        g_theSchedule->AddTask(job);

        // Phase 3: Set chunk state (no tracking set needed)
        chunk->SetState(ChunkState::Loading);

        LogDebug("world", "Submitted LoadChunkJob (ESF) for chunk (%d, %d)", chunkCoords.x, chunkCoords.y);
        return;
    }

    // Try casting to ESFSChunkStorage
    ESFSChunkStorage* esfsStorage = dynamic_cast<ESFSChunkStorage*>(m_chunkStorage.get());
    if (esfsStorage)
    {
        // Create LoadChunkJob for ESFS format and transfer ownership to ScheduleSubsystem
        // [REFACTORED] Pass World* instead of Chunk* - Job will get Chunk via GetChunk()
        LoadChunkJob* job = new LoadChunkJob(chunkCoords, this, esfsStorage);
        g_theSchedule->AddTask(job);

        // Phase 3: Set chunk state (no tracking set needed)
        chunk->SetState(ChunkState::Loading);

        LogDebug("world", "Submitted LoadChunkJob (ESFS) for chunk (%d, %d)", chunkCoords.x, chunkCoords.y);
        return;
    }

    LogError("world", "ChunkStorage is neither ESFChunkStorage nor ESFSChunkStorage type");
}

void World::SubmitSaveChunkJob(IntVec2 chunkCoords, const Chunk* chunk)
{
    // Phase 3: Use IsInQueue() instead of tracking set
    if (IsInQueue(m_pendingSaveQueue, chunkCoords))
    {
        return; // Already in queue
    }

    if (!g_theSchedule)
    {
        LogError("world", "Cannot submit SaveChunkJob - g_theSchedule not initialized");
        return;
    }

    if (!m_chunkStorage)
    {
        LogError("world", "Cannot submit SaveChunkJob - ChunkStorage not set");
        return;
    }

    // Try casting to ESFChunkStorage first
    ESFChunkStorage* esfStorage = dynamic_cast<ESFChunkStorage*>(m_chunkStorage.get());
    if (esfStorage)
    {
        // Create SaveChunkJob for ESF format (deep copy performed in constructor) and transfer ownership
        SaveChunkJob* job = new SaveChunkJob(chunkCoords, chunk, esfStorage);
        g_theSchedule->AddTask(job);

        // Phase 3: Set chunk state (no tracking set needed)
        const_cast<Chunk*>(chunk)->SetState(ChunkState::Saving);

        LogDebug("world", "Submitted SaveChunkJob (ESF) for chunk (%d, %d)", chunkCoords.x, chunkCoords.y);
        return;
    }

    // Try casting to ESFSChunkStorage
    ESFSChunkStorage* esfsStorage = dynamic_cast<ESFSChunkStorage*>(m_chunkStorage.get());
    if (esfsStorage)
    {
        // Create SaveChunkJob for ESFS format (deep copy performed in constructor) and transfer ownership
        SaveChunkJob* job = new SaveChunkJob(chunkCoords, chunk, esfsStorage);
        g_theSchedule->AddTask(job);

        // Phase 3: Set chunk state (no tracking set needed)
        const_cast<Chunk*>(chunk)->SetState(ChunkState::Saving);

        LogDebug("world", "Submitted SaveChunkJob (ESFS) for chunk (%d, %d)", chunkCoords.x, chunkCoords.y);
        return;
    }

    LogError("world", "ChunkStorage is neither ESFChunkStorage nor ESFSChunkStorage type");
}

void World::HandleGenerateChunkCompleted(GenerateChunkJob* job)
{
    // Phase 2.2: Use coordinates to query chunk (no pointer checks needed)
    IntVec2 coords = job->GetChunkCoords();
    Chunk*  chunk  = GetChunk(coords.x, coords.y);

    // If chunk doesn't exist, it was unloaded - gracefully return
    if (!chunk)
    {
        LogDebug("world", "Chunk (%d, %d) not found for completed generate job (already unloaded)",
                 coords.x, coords.y);

        // Phase 3: No tracking set to clean up
        m_activeGenerateJobs--;
        return;
    }

    // Verify state (should be Generating)
    ChunkState currentState = chunk->GetState();
    if (currentState != ChunkState::Generating)
    {
        LogWarn("world", "Chunk (%d, %d) state mismatch in HandleGenerateChunkCompleted (expected Generating, got %d)",
                coords.x, coords.y, (int)currentState);

        // Phase 3: No tracking set to clean up
        m_activeGenerateJobs--;
        return;
    }

    // [Phase 6] Initialize lighting after generation
    chunk->InitializeLighting(this);

    // Activate chunk and mark for mesh rebuild
    chunk->SetState(ChunkState::Active);
    chunk->SetGenerated(true);
    chunk->SetModified(true);
    chunk->MarkDirty();
    ScheduleChunkMeshRebuild(chunk);

    // Phase 3: No tracking set to clean up
    m_activeGenerateJobs--;

    LogDebug("world", "Chunk (%d, %d) generation completed, now Active", coords.x, coords.y);
}

void World::HandleLoadChunkCompleted(LoadChunkJob* job)
{
    // Phase 2.2: Use coordinates to query chunk (no pointer checks needed)
    IntVec2 coords = job->GetChunkCoords();
    Chunk*  chunk  = GetChunk(coords.x, coords.y);

    // If chunk doesn't exist, it was unloaded - gracefully return
    if (!chunk)
    {
        LogDebug("world", "Chunk (%d, %d) not found for completed load job (already unloaded)",
                 coords.x, coords.y);

        // Phase 3: No tracking set to clean up
        m_activeLoadJobs--;
        return;
    }

    // Verify state (should be Loading)
    ChunkState currentState = chunk->GetState();
    if (currentState != ChunkState::Loading)
    {
        LogWarn("world", "Chunk (%d, %d) state mismatch in HandleLoadChunkCompleted (expected Loading, got %d)",
                coords.x, coords.y, (int)currentState);

        // Phase 3: No tracking set to clean up
        m_activeLoadJobs--;
        return;
    }

    // Check if load was successful
    if (job->WasSuccessful())
    {
        // [Phase 6] Initialize lighting after loading from disk
        chunk->InitializeLighting(this);

        // Load succeeded - activate chunk
        chunk->SetState(ChunkState::Active);
        chunk->SetGenerated(true);
        chunk->MarkDirty();
        ScheduleChunkMeshRebuild(chunk);
        LogDebug("world", "Chunk (%d, %d) loaded successfully, now Active", coords.x, coords.y);
    }
    else
    {
        // Load failed - add to generate queue
        chunk->SetState(ChunkState::PendingGenerate);
        m_pendingGenerateQueue.push_back(coords);
        LogDebug("world", "Chunk (%d, %d) load failed, added to generate queue", coords.x, coords.y);
    }

    // Phase 3: No tracking set to clean up
    m_activeLoadJobs--;
}


void World::HandleSaveChunkCompleted(SaveChunkJob* job)
{
    // Phase 2.2: Use coordinates to query chunk (no pointer checks needed)
    IntVec2 coords = job->GetChunkCoords();
    Chunk*  chunk  = GetChunk(coords.x, coords.y);

    // If chunk doesn't exist, save was still successful - gracefully return
    if (!chunk)
    {
        LogDebug("world", "Chunk (%d, %d) not found for completed save job (already unloaded, save successful)",
                 coords.x, coords.y);

        // Phase 3: No tracking set to clean up
        m_activeSaveJobs--;
        return;
    }

    // Verify state (should be Saving)
    ChunkState currentState = chunk->GetState();
    if (currentState != ChunkState::Saving)
    {
        LogWarn("world", "Chunk (%d, %d) state mismatch in HandleSaveChunkCompleted (expected Saving, got %d)",
                coords.x, coords.y, (int)currentState);

        // Phase 3: No tracking set to clean up
        m_activeSaveJobs--;
        return;
    }

    // Save completed - mark as clean and activate
    chunk->SetState(ChunkState::Active);
    chunk->SetModified(false);

    // Phase 3: No tracking set to clean up
    m_activeSaveJobs--;

    LogDebug("world", "Chunk (%d, %d) save completed, now Active", coords.x, coords.y);
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
        Chunk* chunk = GetChunk(chunkCoords.x, chunkCoords.y);
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
            LogDebug("world", "Submitted generate job for chunk (%d, %d) - Active: %d/%d",
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
        Chunk* chunk = GetChunk(chunkCoords.x, chunkCoords.y);
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
            LogDebug("world", "Submitted load job for chunk (%d, %d) - Active: %d/%d",
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
        Chunk* chunk = GetChunk(chunkCoords.x, chunkCoords.y);
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
            LogDebug("world", "Submitted save job for chunk (%d, %d) - Active: %d/%d",
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

    // [FIX] Collect distant coords before removing from queues
    // We need to also remove the chunk objects from m_loadedChunks to prevent
    // re-creation loop: chunk created -> added to queue -> removed from queue (distant)
    // -> chunk object remains with PendingGenerate state -> UnloadFarthestChunk removes it
    // -> next frame: chunk not in loadedChunks, not in queue -> re-created
    std::vector<IntVec2> distantChunksToRemove;

    // Remove distant jobs from Generate queue and collect coords
    for (const auto& coords : m_pendingGenerateQueue)
    {
        if (isDistant(coords))
        {
            distantChunksToRemove.push_back(coords);
        }
    }
    auto   it              = std::remove_if(m_pendingGenerateQueue.begin(), m_pendingGenerateQueue.end(), isDistant);
    size_t removedGenerate = std::distance(it, m_pendingGenerateQueue.end());
    m_pendingGenerateQueue.erase(it, m_pendingGenerateQueue.end());

    // Remove distant jobs from Load queue and collect coords
    for (const auto& coords : m_pendingLoadQueue)
    {
        if (isDistant(coords))
        {
            distantChunksToRemove.push_back(coords);
        }
    }
    it                 = std::remove_if(m_pendingLoadQueue.begin(), m_pendingLoadQueue.end(), isDistant);
    size_t removedLoad = std::distance(it, m_pendingLoadQueue.end());
    m_pendingLoadQueue.erase(it, m_pendingLoadQueue.end());

    // Remove distant jobs from Save queue (don't remove chunk objects for save queue)
    it                 = std::remove_if(m_pendingSaveQueue.begin(), m_pendingSaveQueue.end(), isDistant);
    size_t removedSave = std::distance(it, m_pendingSaveQueue.end());
    m_pendingSaveQueue.erase(it, m_pendingSaveQueue.end());

    // [FIX] Remove chunk objects from m_loadedChunks for distant Generate/Load jobs
    // This prevents the re-creation loop
    for (const auto& coords : distantChunksToRemove)
    {
        int64_t packedCoords = ChunkHelper::PackCoordinates(coords.x, coords.y);
        auto    chunkIt      = m_loadedChunks.find(packedCoords);
        if (chunkIt != m_loadedChunks.end())
        {
            Chunk* chunk = chunkIt->second.get();
            if (chunk)
            {
                ChunkState state = chunk->GetState();
                // Only remove if chunk is in pending state (not yet generated/loaded)
                if (state == ChunkState::PendingGenerate || state == ChunkState::PendingLoad ||
                    state == ChunkState::CheckingDisk)
                {
                    m_loadedChunks.erase(chunkIt);
                    LogDebug("world", "Removed distant pending chunk (%d, %d) from loadedChunks",
                             coords.x, coords.y);
                }
            }
        }
    }

    // Log cancellations (only if any were removed)
    if (removedGenerate > 0 || removedLoad > 0 || removedSave > 0)
    {
        LogDebug("world", "Removed distant jobs: %zu generate, %zu load, %zu save",
                 removedGenerate, removedLoad, removedSave);
    }
}

//-------------------------------------------------------------------------------------------
// Phase 2.2: Cancel Pending Jobs for Chunk
//-------------------------------------------------------------------------------------------

void World::CancelPendingJobsForChunk(IntVec2 coords)
{
    // Lambda 函数：匹配坐标
    auto matchCoords = [coords](const IntVec2& c)
    {
        return c == coords;
    };

    // 从 Generate 队列中移除
    m_pendingGenerateQueue.erase(
        std::remove_if(m_pendingGenerateQueue.begin(),
                       m_pendingGenerateQueue.end(),
                       matchCoords),
        m_pendingGenerateQueue.end()
    );

    // 从 Load 队列中移除
    m_pendingLoadQueue.erase(
        std::remove_if(m_pendingLoadQueue.begin(),
                       m_pendingLoadQueue.end(),
                       matchCoords),
        m_pendingLoadQueue.end()
    );

    // 从 Save 队列中移除
    m_pendingSaveQueue.erase(
        std::remove_if(m_pendingSaveQueue.begin(),
                       m_pendingSaveQueue.end(),
                       matchCoords),
        m_pendingSaveQueue.end()
    );

    LogDebug("world", "Cancelled pending jobs for chunk (%d, %d)", coords.x, coords.y);
}

//-------------------------------------------------------------------------------------------
// 区块距离和管理辅助方法（从 ChunkManager 迁移）
//-------------------------------------------------------------------------------------------

float World::GetChunkDistanceToPlayer(int32_t chunkX, int32_t chunkY) const
{
    // Convert player position to chunk coordinates
    int32_t playerChunkX = static_cast<int32_t>(std::floor(m_playerPosition.x / Chunk::CHUNK_SIZE_X));
    int32_t playerChunkY = static_cast<int32_t>(std::floor(m_playerPosition.y / Chunk::CHUNK_SIZE_Y));

    // Calculate distance in chunk units
    float dx = static_cast<float>(chunkX - playerChunkX);
    float dy = static_cast<float>(chunkY - playerChunkY);
    return std::sqrt(dx * dx + dy * dy);
}

std::pair<int32_t, int32_t> World::FindFarthestChunk() const
{
    std::pair<int32_t, int32_t> farthest    = {INT32_MAX, INT32_MAX};
    float                       maxDistance = -1.0f;

    for (const auto& [packedCoords, chunk] : m_loadedChunks)
    {
        int32_t chunkX, chunkY;
        ChunkHelper::UnpackCoordinates(packedCoords, chunkX, chunkY);

        float distance = GetChunkDistanceToPlayer(chunkX, chunkY);
        if (distance > maxDistance)
        {
            maxDistance = distance;
            farthest    = {chunkX, chunkY};
        }
    }

    return farthest;
}

//-------------------------------------------------------------------------------------------
// Phase 3: Helper method to check if coords are in queue
//-------------------------------------------------------------------------------------------
bool World::IsInQueue(const std::deque<IntVec2>& queue, IntVec2 coords) const
{
    return std::find(queue.begin(), queue.end(), coords) != queue.end();
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
        // Phase 1: BuildMeshJob removed - mesh building now happens on main thread
        // Other task types can be handled here in the future

        // Delete the task (caller is responsible for cleanup)
        delete task;
    }
}

//-----------------------------------------------------------------------------------------------
// Phase 1: Main Thread Mesh Building Implementation (Assignment 03 Requirements)
//-----------------------------------------------------------------------------------------------

void World::UpdateChunkMeshes()
{
    int rebuiltCount = 0;
    while (rebuiltCount < m_maxMeshRebuildsPerFrame && !m_pendingMeshRebuildQueue.empty())
    {
        Chunk* chunk = m_pendingMeshRebuildQueue.front();
        m_pendingMeshRebuildQueue.pop_front();

        if (chunk && chunk->IsActive() && chunk->NeedsMeshRebuild())
        {
            // Build mesh on main thread using ChunkMeshHelper
            chunk->RebuildMesh();
            rebuiltCount++;

            LogDebug("world", "Rebuilt mesh for chunk (%d, %d) on main thread (%d/%d this frame)",
                     chunk->GetChunkX(), chunk->GetChunkY(), rebuiltCount, m_maxMeshRebuildsPerFrame);
        }
    }

    if (rebuiltCount > 0)
    {
        LogDebug("world", "UpdateChunkMeshes: rebuilt %d meshes this frame, %zu remaining in queue",
                 rebuiltCount, m_pendingMeshRebuildQueue.size());
    }
}

void World::ScheduleChunkMeshRebuild(Chunk* chunk)
{
    if (!chunk)
    {
        return;
    }

    // Mark chunk as needing mesh rebuild
    chunk->MarkDirty();

    // Check if chunk is already in queue
    auto it = std::find(m_pendingMeshRebuildQueue.begin(), m_pendingMeshRebuildQueue.end(), chunk);
    if (it == m_pendingMeshRebuildQueue.end())
    {
        // Add to queue and sort by distance
        m_pendingMeshRebuildQueue.push_back(chunk);
        SortMeshQueueByDistance();

        LogDebug("world", "Marked chunk (%d, %d) dirty, added to mesh rebuild queue (size: %zu)",
                 chunk->GetChunkX(), chunk->GetChunkY(), m_pendingMeshRebuildQueue.size());
    }
}

void World::SortMeshQueueByDistance()
{
    if (m_pendingMeshRebuildQueue.empty()) return;

    // Phase 4: Use partial_sort for better performance (only sort first N elements)
    // Only need to sort up to m_maxMeshRebuildsPerFrame elements
    size_t sortCount = (std::min)(
        static_cast<size_t>(m_maxMeshRebuildsPerFrame),
        m_pendingMeshRebuildQueue.size()
    );

    std::partial_sort(
        m_pendingMeshRebuildQueue.begin(),
        m_pendingMeshRebuildQueue.begin() + sortCount,
        m_pendingMeshRebuildQueue.end(),
        [this](Chunk* a, Chunk* b)
        {
            return GetChunkDistanceToPlayer(a) < GetChunkDistanceToPlayer(b);
        }
    );
}

float World::GetChunkDistanceToPlayer(Chunk* chunk) const
{
    if (!chunk)
    {
        return FLT_MAX;
    }

    IntVec2 chunkCoords = chunk->GetChunkCoords();
    return GetChunkDistanceToPlayer(chunkCoords.x, chunkCoords.y);
}

//-------------------------------------------------------------------------------------------
// Phase 5: Graceful Shutdown Implementation
//-------------------------------------------------------------------------------------------

void World::PrepareShutdown()
{
    LogInfo("world", "Preparing graceful shutdown: stopping new task submissions");
    m_isShuttingDown.store(true);

    // Phase 3: Log current pending task counts using queues instead of tracking sets
    LogInfo("world", "Pending tasks at shutdown: Generate=%zu, Load=%zu, Save=%zu",
            m_pendingGenerateQueue.size(),
            m_pendingLoadQueue.size(),
            m_pendingSaveQueue.size());
}

//------------------------------------------------------------------------------------------------------------------
// Shutdown fix: wait for all jobs to complete
//
// Problem: Previously, I only waited for the Pending queue, which caused the Generator to be destructed while the Job in the Executing queue was still running.
// Solution: Add Executing queue waiting to ensure that all Worker threads are completed before destructing resources.
// Assignment 03: "Handles shut down nicely by waiting on all threads to complete"
//------------------------------------------------------------------------------------------------------------------
void World::WaitForPendingTasks()
{
    // Mark shutdown status
    m_isShuttingDown.store(true);

    // Phase 1: Wait for the Pending queue to be empty
    LogInfo("world", "Shutdown Phase 1: Waiting for pending queues to drain...");
    LogInfo("world", "  Pending tasks: Generate=%zu, Load=%zu, Save=%zu",
            m_pendingGenerateQueue.size(),
            m_pendingLoadQueue.size(),
            m_pendingSaveQueue.size());

    while (!m_pendingGenerateQueue.empty() || !m_pendingLoadQueue.empty() || !m_pendingSaveQueue.empty())
    {
        std::this_thread::yield();
    }

    LogInfo("world", "Shutdown Phase 1: All pending queues drained");

    // Phase 2: Wait for the Executing queue to be empty (new fix)
    // Wait for all executing Chunk generation tasks to complete
    LogInfo("world", "Shutdown Phase 2: Waiting for executing tasks to complete...");

    if (!g_theSchedule)
    {
        LogWarn("world", "ScheduleSubsystem not available, skipping executing task wait");
        return;
    }

    while (g_theSchedule->HasExecutingTasks("ChunkGen"))
    {
        std::this_thread::yield();
    }

    LogInfo("world", "Shutdown Phase 2: All executing tasks completed");

    // Phase 3: Get completed task results
    LogInfo("world", "Shutdown Phase 3: Retrieving completed job results...");
    ProcessCompletedChunkTasks();
    LogInfo("world", "Shutdown Phase 3: Job results retrieved");

    LogInfo("world", "WaitForPendingTasks complete - safe to shutdown");
}

bool World::IsChunkLoadedDirect(int32_t chunkX, int32_t chunkY) const
{
    int64_t chunkPackID = ChunkHelper::PackCoordinates(chunkX, chunkY);
    return m_loadedChunks.find(chunkPackID) != m_loadedChunks.end();
}

size_t World::GetLoadedChunkCount() const
{
    return m_loadedChunks.size();
}

std::unordered_map<int64_t, std::unique_ptr<Chunk>>& World::GetLoadedChunks()
{
    return m_loadedChunks;
}

bool World::SetEnableChunkDebugDirect(bool enable)
{
    m_enableChunkDebug = enable;
    return m_enableChunkDebug;
}

Texture* World::GetBlocksAtlasTexture() const
{
    return m_cachedBlocksAtlasTexture;
}

//-------------------------------------------------------------------------------------------
// Phase 6: Chunk Unloading Logic (Assignment 02 Requirement)
//-------------------------------------------------------------------------------------------

void World::UnloadFarthestChunk()
{
    // Reuse existing FindFarthestChunk() method to find the farthest loaded chunk
    auto [chunkX, chunkY] = FindFarthestChunk();

    // Check if a valid chunk was found
    if (chunkX == INT32_MAX || chunkY == INT32_MAX)
    {
        return; // No loaded chunks to unload
    }

    // Calculate distance in chunk units (circular range check)
    float distance = GetChunkDistanceToPlayer(chunkX, chunkY);

    // Check if chunk is beyond deactivation range
    if (distance > static_cast<float>(m_chunkDeactivationRange))
    {
        // Cancel all pending jobs for this chunk to avoid wasted CPU resources
        CancelPendingJobsForChunk(IntVec2(chunkX, chunkY));

        // Unload the chunk (handles state checks and VBO cleanup internally)
        UnloadChunkDirect(chunkX, chunkY);

        LogDebug("world", "Unloaded farthest chunk (%d, %d) at distance %.2f chunks",
                 chunkX, chunkY, distance);
    }
}

//-------------------------------------------------------------------------------------------
// [REFACTORED] Lighting System - Now delegates to VoxelLightEngine
//-------------------------------------------------------------------------------------------

void World::UndirtyAllBlocksInChunk(Chunk* chunk)
{
    // [REFACTORED] Delegate to VoxelLightEngine
    m_voxelLightEngine->UndirtyAllBlocksInChunk(chunk);
}

//-----------------------------------------------------------------------------------------------
// [NEW] UpdateSkyBrightness - Calculate sky darken from time
// Reference: Minecraft Level.java:559-563
//-----------------------------------------------------------------------------------------------

void World::UpdateSkyBrightness(const ITimeProvider& timeProvider)
{
    // Reference: Minecraft Level.updateSkyBrightness()
    // skyDarken = 0 at noon (max sun), 11 at midnight (dark)
    float celestialAngle = timeProvider.GetCelestialAngle();
    float skyBrightness  = 1.0f - (std::cos(celestialAngle * 3.14159f * 2.0f) * 2.0f + 0.2f);

    // Clamp to [0, 1]
    if (skyBrightness < 0.0f) skyBrightness = 0.0f;
    if (skyBrightness > 1.0f) skyBrightness = 1.0f;

    m_skyDarken = static_cast<int>((1.0f - skyBrightness) * 11.0f);
}

//-----------------------------------------------------------------------------------------------
// [NEW] MarkLightingDirty - Convenience wrapper for Chunk usage
// Delegates to both BlockLightEngine and SkyLightEngine
//-----------------------------------------------------------------------------------------------

void World::MarkLightingDirty(const BlockIterator& iter)
{
    if (!m_voxelLightEngine) return;

    // Mark dirty in both light engines
    m_voxelLightEngine->GetBlockEngine().MarkDirty(iter);
    m_voxelLightEngine->GetSkyEngine().MarkDirty(iter);
}

//-----------------------------------------------------------------------------------------------
// Phase 10: Block Digging and Placing Operations
//-----------------------------------------------------------------------------------------------

void World::DigBlock(const BlockIterator& blockIter)
{
    // 1. Check if iterator is valid
    if (!blockIter.IsValid())
    {
        return;
    }

    Chunk* chunk = blockIter.GetChunk();
    if (!chunk)
    {
        return;
    }

    // 2. Get current block state
    BlockState* currentState = blockIter.GetBlock();
    if (!currentState)
    {
        return;
    }

    // 3. Get air block state
    auto airBlock = enigma::registry::block::BlockRegistry::GetBlock("simpleminer", "air");
    if (!airBlock)
    {
        LogError("world", "DigBlock: Cannot find air block in registry");
        return;
    }
    BlockState* airState = airBlock->GetDefaultState();

    // 4. Check if already air (no need to dig)
    if (currentState == airState)
    {
        return; // Already air, nothing to dig
    }

    // 5. Get local coordinates and delegate to Chunk::SetBlockByPlayer
    // This method already handles:
    // - SKY flag propagation (digging opens sky column)
    // - Light dirty marking
    // - Chunk modification flags
    int32_t localX, localY, localZ;
    blockIter.GetLocalCoords(localX, localY, localZ);

    chunk->SetBlockByPlayer(localX, localY, localZ, airState);

    // 6. Schedule chunk mesh rebuild
    ScheduleChunkMeshRebuild(chunk);

    LogDebug("world", "DigBlock: Removed block at (%d, %d, %d)",
             blockIter.GetBlockPos().x, blockIter.GetBlockPos().y, blockIter.GetBlockPos().z);
}

void World::PlaceBlock(const BlockIterator& blockIter, BlockState* newState)
{
    // 1. Check if iterator and new state are valid
    if (!blockIter.IsValid() || !newState)
    {
        return;
    }

    Chunk* chunk = blockIter.GetChunk();
    if (!chunk)
    {
        return;
    }

    // 2. Get current block state
    BlockState* currentState = blockIter.GetBlock();
    if (!currentState)
    {
        return;
    }

    // 3. Get air block state for comparison
    auto airBlock = enigma::registry::block::BlockRegistry::GetBlock("simpleminer", "air");
    if (!airBlock)
    {
        LogError("world", "PlaceBlock: Cannot find air block in registry");
        return;
    }
    BlockState* airState = airBlock->GetDefaultState();

    // 4. Check if placement position is air (can only place in air)
    if (currentState != airState)
    {
        LogDebug("world", "PlaceBlock: Cannot place block at non-air position (%d, %d, %d)",
                 blockIter.GetBlockPos().x, blockIter.GetBlockPos().y, blockIter.GetBlockPos().z);
        return; // Cannot place in non-air position
    }

    // 5. Get local coordinates and delegate to Chunk::SetBlockByPlayer
    // This method already handles:
    // - SKY flag clearing (placing opaque blocks blocks sky column)
    // - Light dirty marking
    // - Chunk modification flags
    int32_t localX, localY, localZ;
    blockIter.GetLocalCoords(localX, localY, localZ);

    chunk->SetBlockByPlayer(localX, localY, localZ, newState);

    // 6. Schedule chunk mesh rebuild
    ScheduleChunkMeshRebuild(chunk);

    // 7. [NEW] Notify 6 neighbors about block change (for stairs shape update, etc.)
    //    This is critical for stairs/slab auto-connection feature
    BlockPos                        pos             = blockIter.GetBlockPos();
    enigma::registry::block::Block* placedBlockType = newState->GetBlock();

    constexpr Direction allDirections[] = {
        Direction::NORTH, Direction::SOUTH, Direction::EAST,
        Direction::WEST, Direction::UP, Direction::DOWN
    };

    for (Direction dir : allDirections)
    {
        BlockPos    neighborPos   = pos.GetRelative(dir);
        BlockState* neighborState = GetBlockState(neighborPos);

        if (neighborState && neighborState->GetBlock())
        {
            // Call neighbor's OnNeighborChanged callback
            neighborState->OnNeighborChanged(this, neighborPos, placedBlockType);
        }
    }

    LogDebug("world", "PlaceBlock: Placed block at (%d, %d, %d)",
             blockIter.GetBlockPos().x, blockIter.GetBlockPos().y, blockIter.GetBlockPos().z);
}

//-------------------------------------------------------------------------------------------
// [NEW] PlaceBlock with PlacementContext (for slab/stairs)
//-------------------------------------------------------------------------------------------
void World::PlaceBlock(const BlockIterator&        blockIter, enigma::registry::block::Block* blockType,
                       const VoxelRaycastResult3D& raycast, const Vec3&                       playerLookDir)
{
    using namespace enigma::registry::block;

    // [DEBUG] Track slab/stairs placement
    bool isDebugBlock = blockType && (blockType->GetRegistryName().find("slab") != std::string::npos ||
        blockType->GetRegistryName().find("stairs") != std::string::npos);

    if (isDebugBlock)
    {
        core::LogInfo("World", "[DEBUG] PlaceBlock (context) called for: %s:%s",
                      blockType->GetNamespace().c_str(), blockType->GetRegistryName().c_str());
    }

    // [VALIDATION] Check block iterator validity
    if (!blockIter.IsValid())
    {
        LogError("world", "PlaceBlock (context): Invalid block iterator");
        return;
    }

    Chunk* chunk = blockIter.GetChunk();
    if (!chunk)
    {
        LogError("world", "PlaceBlock (context): Chunk not found");
        return;
    }

    if (!blockType)
    {
        LogError("world", "PlaceBlock (context): Block type is null");
        return;
    }

    BlockPos targetPos = blockIter.GetBlockPos();

    // [FIX] Compute block-local hit point relative to CLICKED block (0-1 normalized)
    // Minecraft uses clickedPos for hit point calculation (getClickLocation - getClickedPos)
    // This is critical for slab merge detection: clicking top of bottom slab should give z=0.5
    Vec3     worldHitPos     = raycast.m_impactPos;
    BlockPos clickedPos      = raycast.m_hitBlockIter.GetBlockPos();
    Vec3     clickedWorldPos = Vec3(static_cast<float>(clickedPos.x),
                                    static_cast<float>(clickedPos.y),
                                    static_cast<float>(clickedPos.z));
    Vec3 localHitPoint = worldHitPos - clickedWorldPos; // Relative to clicked block, not target

    // [NEW] Construct PlacementContext
    PlacementContext ctx;
    ctx.world         = this;
    ctx.targetPos     = targetPos;
    ctx.clickedPos    = clickedPos;
    ctx.clickedFace   = raycast.m_hitFace;
    ctx.hitPoint      = localHitPoint;
    ctx.playerLookDir = playerLookDir;
    ctx.heldItemBlock = blockType;

    // [FIX] Check if can replace CLICKED block first (Slab merge case)
    // Minecraft logic: When clicking on a slab, check if we can merge at clickedPos
    // NOT at targetPos (which would be the adjacent empty block)
    BlockState* clickedState = raycast.m_hitBlockIter.GetBlock();
    if (clickedState && clickedState->GetBlock()->CanBeReplaced(clickedState, ctx))
    {
        // [MERGE] Replace existing slab with double slab at CLICKED position
        BlockState* mergedState = clickedState->GetBlock()->GetStateForPlacement(ctx);
        if (mergedState)
        {
            // [FIX] Cannot use PlaceBlock() here - it checks for air and would fail!
            // Directly replace the block using SetBlockByPlayer (supports replacement)
            Chunk* clickedChunk = raycast.m_hitBlockIter.GetChunk();
            if (clickedChunk)
            {
                int32_t localX, localY, localZ;
                raycast.m_hitBlockIter.GetLocalCoords(localX, localY, localZ);
                clickedChunk->SetBlockByPlayer(localX, localY, localZ, mergedState);
                ScheduleChunkMeshRebuild(clickedChunk);

                LogDebug("world", "PlaceBlock (context): Merged slab at clickedPos (%d, %d, %d)",
                         ctx.clickedPos.x, ctx.clickedPos.y, ctx.clickedPos.z);
            }
        }
        return;
    }

    // [OLD CASE] Check targetPos for potential replacement (kept for future use cases)
    // This handles cases where targetPos itself might be replaceable (e.g., water, grass)
    BlockState* existingState = blockIter.GetBlock();
    if (existingState && existingState->GetBlock() != blockType &&
        existingState->GetBlock()->CanBeReplaced(existingState, ctx))
    {
        // Non-slab replacement case (e.g., replacing water/grass)
        BlockState* replacedState = blockType->GetStateForPlacement(ctx);
        PlaceBlock(blockIter, replacedState);
        LogDebug("world", "PlaceBlock (context): Replaced block at targetPos (%d, %d, %d)",
                 targetPos.x, targetPos.y, targetPos.z);
        return;
    }

    // [FIX] Corrected log format - was printing hitPoint twice instead of playerLookDir
    LogInfo("world", "PlaceBlock with PlacementContent: \n"
            "TargetPos: (%d, %d, %d) \n"
            "ClickedPos: (%d, %d, %d) \n"
            "ClickedFace: %d \n"
            "HitPoint: (%f, %f, %f) \n"
            "PlayerLookDir: (%f, %f, %f) \n"
            , ctx.targetPos.x, ctx.targetPos.y, ctx.targetPos.z,
            ctx.clickedPos.x, ctx.clickedPos.y, ctx.clickedPos.z,
            static_cast<int>(ctx.clickedFace), ctx.hitPoint.x, ctx.hitPoint.y, ctx.hitPoint.z,
            ctx.playerLookDir.x, ctx.playerLookDir.y, ctx.playerLookDir.z);

    // [NEW] Get final state from placement logic
    BlockState* finalState = blockType->GetStateForPlacement(ctx);

    if (isDebugBlock)
    {
        core::LogInfo("World", "  GetStateForPlacement() returned: %s", finalState ? "valid" : "NULL");
        if (finalState)
        {
            core::LogInfo("World", "  Final state properties: '%s'", finalState->GetProperties().ToString().c_str());
        }
    }

    // [REUSE] Call original PlaceBlock
    PlaceBlock(blockIter, finalState);

    LogDebug("world", "PlaceBlock (context): Placed block at (%d, %d, %d) with placement context",
             targetPos.x, targetPos.y, targetPos.z);
}
