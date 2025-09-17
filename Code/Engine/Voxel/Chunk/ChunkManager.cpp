#include "ChunkManager.hpp"
#include "../Generation/Generator.hpp"
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
        // Create new chunk
        auto newChunk = std::make_unique<Chunk>(IntVec2(chunkX, chunkY));

        // Generate terrain using the assigned generator
        if (m_generator)
        {
            uint32_t worldSeed = 0; // TODO: Get world seed from World class
            m_generator->GenerateChunk(newChunk.get(), chunkX, chunkY, worldSeed);
            core::LogInfo("chunk", "Generated chunk (%d, %d) using generator: %s",
                          chunkX, chunkY, m_generator->GetDisplayName().c_str());
        }
        else
        {
            core::LogWarn("chunk", "No generator assigned, chunk (%d, %d) will be empty", chunkX, chunkY);
        }

        m_loadedChunks[chunkPackID] = std::move(newChunk);
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
        loaded_chunk.second->Render(renderer);
    }
}

bool ChunkManager::SetEnableChunkDebug(bool enable)
{
    m_enableChunkDebug = enable;
    return m_enableChunkDebug;
}

void ChunkManager::SetGenerator(enigma::voxel::generation::Generator* generator)
{
    m_generator = generator;
    if (generator)
    {
        core::LogInfo("chunk", "ChunkManager: Generator set to %s", generator->GetDisplayName().c_str());
    }
    else
    {
        core::LogInfo("chunk", "ChunkManager: Generator cleared");
    }
}
