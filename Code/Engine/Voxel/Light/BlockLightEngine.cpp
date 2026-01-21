//-----------------------------------------------------------------------------------------------
// BlockLightEngine.cpp
//
// [REFACTORED] Block light engine implementation with GetLightBlock support
// 
// [MINECRAFT REF] BlockLightEngine.java - Block light propagation
// [MINECRAFT REF] LightEngine.java:79 - Light attenuation: Math.max(1, blockState.getLightBlock(...))
//
// Key changes from original:
// - Uses BlockState::GetLightBlock() for proper light attenuation
// - Uses BlockState::GetLightEmission() for emission values
// - Attenuation is max(1, lightBlock) instead of fixed 1
//
//-----------------------------------------------------------------------------------------------

#include "BlockLightEngine.hpp"
#include "LightEngineCommon.hpp"

#include <algorithm>

#include "Engine/Voxel/Chunk/Chunk.hpp"
#include "Engine/Voxel/World/World.hpp"
#undef max
namespace enigma::voxel
{
    BlockLightEngine::BlockLightEngine(World* world) : LightEngine(world)
    {
    }

    //-------------------------------------------------------------------------------------------
    // GetLightValue
    //
    // Get block light value at world position
    // Reference: World::GetBlockLight (before refactoring)
    //-------------------------------------------------------------------------------------------
    uint8_t BlockLightEngine::GetLightValue(const BlockPos& pos) const
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

        return chunk->GetBlockLight(pos.GetBlockXInChunk(), pos.GetBlockYInChunk(), pos.z);
    }

    //-------------------------------------------------------------------------------------------
    // ComputeCorrectLight
    //
    // [REFACTORED] Calculate correct block light for a position
    // 
    // [MINECRAFT REF] BlockLightEngine.java - propagateIncrease/propagateDecrease
    // [MINECRAFT REF] LightEngine.java:79 - getOpacity calculation
    //
    // Algorithm:
    // 1. Get emission level from BlockState cache
    // 2. Get light attenuation from BlockState::GetLightBlock()
    // 3. Fully opaque blocks (lightBlock >= 15): return emission only
    // 4. Other blocks: return max(emission, max(neighbors) - attenuation)
    //-------------------------------------------------------------------------------------------
    uint8_t BlockLightEngine::ComputeCorrectLight(const BlockIterator& iter) const
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

        // Get block position for light cache queries
        BlockPos blockPos = iter.GetBlockPos();

        // [STEP 1] Get block's intrinsic light emission from cache
        // [MINECRAFT REF] BlockState.getLightEmission()
        uint8_t emission = static_cast<uint8_t>(state->GetLightEmission());

        // [STEP 2] Get light attenuation value from BlockState cache
        // [MINECRAFT REF] LightEngine.java:79 - Math.max(1, blockState.getLightBlock(...))
        int lightBlock = state->GetLightBlock(m_world, blockPos);

        // [STEP 3] Fully opaque blocks (lightBlock >= 15): only emit, don't propagate
        if (lightBlock >= 15)
        {
            return emission;
        }

        // [STEP 4] Calculate attenuation for light propagation
        // [MINECRAFT REF] LightEngine.java:79 - Minimum attenuation is 1
        int attenuation = std::max(1, lightBlock);

        // [STEP 5] Propagate light from neighbors with proper attenuation
        uint8_t maxPropagated = 0;

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

            uint8_t neighborLight = neighborChunk->GetBlockLight(localX, localY, localZ);

            if (neighborLight > attenuation)
            {
                uint8_t propagated = static_cast<uint8_t>(neighborLight - attenuation);
                maxPropagated      = std::max(maxPropagated, propagated);
            }
        }

        // [STEP 6] Return max of emission and propagated light
        return std::max(emission, maxPropagated);
    }

    //-------------------------------------------------------------------------------------------
    // SetLightValue
    //
    // Set block light value in chunk
    //-------------------------------------------------------------------------------------------
    void BlockLightEngine::SetLightValue(Chunk* chunk, int32_t x, int32_t y, int32_t z, uint8_t value)
    {
        if (chunk)
        {
            chunk->SetBlockLight(x, y, z, value);
        }
    }

    //-------------------------------------------------------------------------------------------
    // GetCurrentLightValue
    //
    // Get current block light value from chunk
    //-------------------------------------------------------------------------------------------
    uint8_t BlockLightEngine::GetCurrentLightValue(Chunk* chunk, int32_t x, int32_t y, int32_t z) const
    {
        if (chunk)
        {
            return chunk->GetBlockLight(x, y, z);
        }
        return 0;
    }

    //-------------------------------------------------------------------------------------------
    // GetEmission
    //
    // [DEPRECATED] Use BlockState::GetLightEmission() instead
    // Kept for backward compatibility
    //-------------------------------------------------------------------------------------------
    uint8_t BlockLightEngine::GetEmission(const BlockState* state) const
    {
        if (!state)
        {
            return 0;
        }
        return static_cast<uint8_t>(state->GetLightEmission());
    }
} // namespace enigma::voxel
