//-----------------------------------------------------------------------------------------------
// SkyLightEngine.cpp
//
// [NEW] Sky light engine implementation
// Migrated from World::ComputeCorrectSkyLight (World.cpp:2049-2098)
//
//-----------------------------------------------------------------------------------------------

#include "SkyLightEngine.hpp"
#include <algorithm>

#include "Engine/Voxel/Chunk/Chunk.hpp"
#include "Engine/Voxel/World/World.hpp"
#undef max
namespace enigma::voxel
{
    SkyLightEngine::SkyLightEngine(World* world) : LightEngine(world)
    {
    }

    //-------------------------------------------------------------------------------------------
    // GetLightValue
    //
    // [OVERRIDE] Get sky light at world position
    // Reference: World::GetSkyLight (before refactoring)
    //-------------------------------------------------------------------------------------------
    uint8_t SkyLightEngine::GetLightValue(const BlockPos& pos) const
    {
        if (!m_world)
        {
            return 0;
        }

        // [SIMPLIFIED] Use World::GetChunkAt and BlockPos helper methods
        Chunk* chunk = m_world->GetChunk(pos);
        if (!chunk)
        {
            return 0;
        }

        return chunk->GetSkyLight(pos.GetBlockXInChunk(), pos.GetBlockYInChunk(), pos.z);
    }

    //-------------------------------------------------------------------------------------------
    // ComputeCorrectLight
    //
    // [OVERRIDE] Calculate correct sky light value for a block
    // Algorithm migrated from World::ComputeCorrectSkyLight (World.cpp:2049-2098)
    //
    // Steps:
    // 1. Sky blocks always have max light (15)
    // 2. Opaque blocks block all sky light (0)
    // 3. Transparent blocks: max(neighbors) - 1
    //-------------------------------------------------------------------------------------------
    uint8_t SkyLightEngine::ComputeCorrectLight(const BlockIterator& iter) const
    {
        if (!iter.IsValid())
        {
            return 0;
        }

        const BlockState* state = iter.GetBlock();
        if (!state)
        {
            return 0;
        }

        // [STEP 1] SKY blocks always have maximum outdoor light
        if (IsSkyBlock(iter))
        {
            return 15;
        }

        // [STEP 2] Opaque blocks block all outdoor light
        if (state->GetBlock()->IsOpaque(const_cast<BlockState*>(state)))
        {
            return 0;
        }

        // [STEP 3] Non-opaque blocks: propagate light from neighbors (max - 1)
        uint8_t maxNeighborLight = 0;

        // Check all 6 neighbors (North, South, East, West, Up, Down)
        for (int dir = 0; dir < 6; ++dir)
        {
            BlockIterator neighbor = iter.GetNeighbor(static_cast<Direction>(dir));

            if (!neighbor.IsValid())
            {
                continue;
            }

            Chunk* neighborChunk = neighbor.GetChunk();
            if (!neighborChunk)
            {
                continue;
            }

            int32_t localX, localY, localZ;
            neighbor.GetLocalCoords(localX, localY, localZ);
            uint8_t neighborLight = neighborChunk->GetSkyLight(localX, localY, localZ);

            maxNeighborLight = std::max(maxNeighborLight, neighborLight);
        }

        // Light propagates with -1 attenuation per block (minimum 0)
        return (maxNeighborLight > 0) ? (maxNeighborLight - 1) : 0;
    }

    //-------------------------------------------------------------------------------------------
    // SetLightValue
    //
    // [OVERRIDE] Set sky light value in chunk
    //-------------------------------------------------------------------------------------------
    void SkyLightEngine::SetLightValue(Chunk* chunk, int32_t x, int32_t y, int32_t z, uint8_t value)
    {
        if (chunk)
        {
            chunk->SetSkyLight(x, y, z, value);
        }
    }

    //-------------------------------------------------------------------------------------------
    // GetCurrentLightValue
    //
    // [OVERRIDE] Get current sky light value from chunk
    //-------------------------------------------------------------------------------------------
    uint8_t SkyLightEngine::GetCurrentLightValue(Chunk* chunk, int32_t x, int32_t y, int32_t z) const
    {
        if (chunk)
        {
            return chunk->GetSkyLight(x, y, z);
        }
        return 0;
    }

    //-------------------------------------------------------------------------------------------
    // IsSkyBlock
    //
    // [PRIVATE] Check if block has direct sky access
    // Uses Chunk::GetIsSky() for sky column tracking
    //-------------------------------------------------------------------------------------------
    bool SkyLightEngine::IsSkyBlock(const BlockIterator& iter) const
    {
        Chunk* chunk = iter.GetChunk();
        if (!chunk)
        {
            return false;
        }

        int32_t localX, localY, localZ;
        iter.GetLocalCoords(localX, localY, localZ);
        return chunk->GetIsSky(localX, localY, localZ);
    }
} // namespace enigma::voxel
