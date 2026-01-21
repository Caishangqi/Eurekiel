//-----------------------------------------------------------------------------------------------
// SkyLightEngine.cpp
//
// [REFACTORED] Sky light engine implementation with PropagatesSkylightDown support
// 
// [MINECRAFT REF] SkyLightEngine.java - Sky light propagation algorithm
// [MINECRAFT REF] LightEngine.java:79 - Light attenuation: Math.max(1, blockState.getLightBlock(...))
//
// Key changes from original:
// - Uses BlockState::PropagatesSkylightDown() for vertical skylight propagation
// - Uses BlockState::GetLightBlock() for proper light attenuation
// - Skylight propagates without attenuation when PropagatesSkylightDown is true
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
    // [REFACTORED] Calculate correct sky light value for a block
    // 
    // [MINECRAFT REF] SkyLightEngine.java - propagateIncrease/propagateDecrease
    // [MINECRAFT REF] LightEngine.java:79 - getOpacity calculation
    //
    // Algorithm:
    // 1. Sky blocks (direct sky access) always have max light (15)
    // 2. For each neighbor direction:
    //    - DOWN direction: If PropagatesSkylightDown is true, no attenuation
    //    - Other directions: Attenuate by max(1, GetLightBlock())
    // 3. Return maximum propagated light value
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

        // [STEP 1] SKY blocks always have maximum outdoor light (15)
        // [MINECRAFT REF] SkyLightEngine.java - isSourceLevel(15)
        if (IsSkyBlock(iter))
        {
            return 15;
        }

        // Get block position for light cache queries
        BlockPos blockPos = iter.GetBlockPos();

        // [STEP 2] Get light attenuation value from BlockState cache
        // [MINECRAFT REF] LightEngine.java:79 - Math.max(1, blockState.getLightBlock(...))
        int lightBlock = state->GetLightBlock(m_world, blockPos);

        // Fully opaque blocks (lightBlock >= 15) block all light
        if (lightBlock >= 15)
        {
            return 0;
        }

        // [STEP 3] Check if this block propagates skylight downward
        // [MINECRAFT REF] BlockBehaviour.java:368-370 - propagatesSkylightDown
        bool propagatesSkylight = state->PropagatesSkylightDown(m_world, blockPos);

        // [STEP 4] Propagate light from neighbors
        uint8_t maxLight = 0;

        // Check all 6 neighbors (North, South, East, West, Up, Down)
        for (int dir = 0; dir < 6; ++dir)
        {
            Direction     direction = static_cast<Direction>(dir);
            BlockIterator neighbor  = iter.GetNeighbor(direction);

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

            if (neighborLight == 0)
            {
                continue;
            }

            // [STEP 5] Calculate propagated light based on direction
            uint8_t propagatedLight = 0;

            // [MINECRAFT REF] SkyLightEngine.java - Skylight from above propagates without loss
            // if PropagatesSkylightDown is true
            if (direction == Direction::UP && propagatesSkylight && neighborLight == 15)
            {
                // Skylight from directly above passes through without attenuation
                // This is the key behavior for leaves, glass, etc.
                propagatedLight = 15;
            }
            else
            {
                // [MINECRAFT REF] LightEngine.java:79 - Standard attenuation
                // Attenuation is max(1, lightBlock) for horizontal and non-skylight vertical
                int attenuation = std::max(1, lightBlock);
                propagatedLight = (neighborLight > attenuation)
                                      ? static_cast<uint8_t>(neighborLight - attenuation)
                                      : 0;
            }

            maxLight = std::max(maxLight, propagatedLight);
        }

        return maxLight;
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
