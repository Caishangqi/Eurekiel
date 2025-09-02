#include "World.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Registry/Block/BlockProperties.hpp"

using namespace enigma::voxel::world;

World::World(std::string worldName, uint64_t worldSeed, std::unique_ptr<ChunkManager> chunkManager) :
    m_worldName(worldName), m_worldSeed(worldSeed), m_chunkManager(std::move(chunkManager))
{
    core::LogInfo("world", "World created: %s", m_worldName.c_str());
}

World::~World()
{
}

BlockState World::GetBlockState(const BlockPos& pos)
{
    Chunk* chunk = m_chunkManager->GetChunk(pos.GetBlockXInChunk(), pos.GetBlockYInChunk());
    if (chunk)
    {
        return chunk->GetBlockWorld(pos);
    }
    return BlockState(nullptr, PropertyMap(), 0);
}

void World::SetBlockState(const BlockPos& pos, BlockState* state) const
{
    Chunk* chunk = m_chunkManager->GetChunk(pos.GetBlockXInChunk(), pos.GetBlockYInChunk());
    if (chunk)
    {
        return chunk->SetBlockWorld(pos, *state);
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

void World::LoadChunk(int32_t chunkX, int32_t chunkY)
{
    core::LogInfo("chunk", "Try Loading chunk: %d, %d", chunkX, chunkY);
    m_chunkManager->LoadChunk(chunkX, chunkY);
}

void World::UnloadChunk(int32_t chunkX, int32_t chunkY)
{
    UNUSED(chunkX)
    UNUSED(chunkY)

    ERROR_RECOVERABLE("World::UnloadChunk is not implemented")
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
