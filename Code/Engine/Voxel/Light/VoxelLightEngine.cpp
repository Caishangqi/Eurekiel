//-----------------------------------------------------------------------------------------------
// VoxelLightEngine.cpp
//
// [NEW] Composite light engine manager implementation
// Composes BlockLightEngine and SkyLightEngine, provides unified interface
//
// Reference: Minecraft LevelLightEngine.java
//
//-----------------------------------------------------------------------------------------------

#include "VoxelLightEngine.hpp"
#include "LightEngineCommon.hpp"

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Voxel/Chunk/Chunk.hpp"
#include "Engine/Voxel/World/World.hpp"
#undef max
namespace enigma::voxel
{
    VoxelLightEngine::VoxelLightEngine(World* world) : m_world(world)
    {
        m_blockEngine = std::make_unique<BlockLightEngine>(world);
        m_skyEngine   = std::make_unique<SkyLightEngine>(world);
        LogInfo(LogVoxelLight, "VoxelLightEngine:: Initialized with BlockLightEngine and SkyLightEngine");
    }

    //-------------------------------------------------------------------------------------------
    // Light Query - Delegate to sub-engines
    //-------------------------------------------------------------------------------------------
    uint8_t VoxelLightEngine::GetSkyLight(const BlockPos& pos) const
    {
        return m_skyEngine->GetLightValue(pos);
    }

    uint8_t VoxelLightEngine::GetBlockLight(const BlockPos& pos) const
    {
        return m_blockEngine->GetLightValue(pos);
    }

    //-------------------------------------------------------------------------------------------
    // Combined Brightness
    // Reference: Minecraft LevelLightEngine.java:145-149
    //-------------------------------------------------------------------------------------------
    int VoxelLightEngine::GetRawBrightness(const BlockPos& pos, int skyDarken) const
    {
        int skyLight   = static_cast<int>(GetSkyLight(pos)) - skyDarken;
        int blockLight = static_cast<int>(GetBlockLight(pos));
        return std::max(skyLight, blockLight);
    }

    //-------------------------------------------------------------------------------------------
    // Unified Update Interface
    //-------------------------------------------------------------------------------------------
    void VoxelLightEngine::CheckBlock(const BlockPos& pos)
    {
        // [STEP 1] Validate world reference
        if (!m_world)
        {
            return;
        }

        // [STEP 2] [SIMPLIFIED] Use World::GetChunkAt and BlockPos helper methods
        Chunk* chunk = m_world->GetChunk(pos);
        if (!chunk || !chunk->IsActive())
        {
            return;
        }

        // [STEP 3] Calculate block index using BlockPos helper methods
        int blockIndex = static_cast<int>(Chunk::CoordsToIndex(pos.GetBlockXInChunk(), pos.GetBlockYInChunk(), pos.z));

        // [STEP 4] Create BlockIterator
        BlockIterator iter(chunk, blockIndex);
        if (!iter.IsValid())
        {
            return;
        }

        // [STEP 5] Mark dirty in both engines (Reference: Minecraft LevelLightEngine.checkBlock)
        m_blockEngine->MarkDirty(iter);
        m_skyEngine->MarkDirty(iter);
    }

    int VoxelLightEngine::RunLightUpdates()
    {
        int processed = 0;

        // [STEP 1] Process block light updates
        while (m_blockEngine->HasWork())
        {
            m_blockEngine->ProcessNextDirtyBlock();
            ++processed;
        }

        // [STEP 2] Process sky light updates
        while (m_skyEngine->HasWork())
        {
            m_skyEngine->ProcessNextDirtyBlock();
            ++processed;
        }

        return processed;
    }

    bool VoxelLightEngine::HasLightWork() const
    {
        return m_blockEngine->HasWork() || m_skyEngine->HasWork();
    }

    //-------------------------------------------------------------------------------------------
    // Chunk Cleanup
    //-------------------------------------------------------------------------------------------
    void VoxelLightEngine::UndirtyAllBlocksInChunk(Chunk* chunk)
    {
        if (!chunk)
            return;

        m_blockEngine->UndirtyAllBlocksInChunk(chunk);
        m_skyEngine->UndirtyAllBlocksInChunk(chunk);
    }
} // namespace enigma::voxel
