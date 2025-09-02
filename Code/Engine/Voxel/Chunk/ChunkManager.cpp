#include "ChunkManager.hpp"
using namespace enigma::voxel::chunk;

ChunkManager::ChunkManager()
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
    // Current simply update the chunk.
    for (const std::pair<const long long, std::unique_ptr<Chunk>>& loaded_chunk : m_loadedChunks)
    {
        loaded_chunk.second->Update(deltaTime);
    }
}

void ChunkManager::Render(IRenderer* renderer)
{
    for (const std::pair<const long long, std::unique_ptr<Chunk>>& loaded_chunk : m_loadedChunks)
    {
        if (m_enableChunkDebug)
            loaded_chunk.second->DebugDraw(renderer);
        loaded_chunk.second->GetMesh()->RenderAll(renderer);
    }
}

bool ChunkManager::SetEnableChunkDebug(bool enable)
{
    m_enableChunkDebug = enable;
    return m_enableChunkDebug;
}
