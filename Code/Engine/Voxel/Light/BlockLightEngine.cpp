//-----------------------------------------------------------------------------------------------
// BlockLightEngine.cpp
//
// [NEW] Block light engine implementation
// Migrated from World::ComputeCorrectBlockLight() (World.cpp:2117-2165)
//
//-----------------------------------------------------------------------------------------------

#include "BlockLightEngine.hpp"
#include "LightEngineCommon.hpp"

#include <algorithm>

#include "Engine/Voxel/Chunk/Chunk.hpp"
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
    // [TODO] Requires World reference - implement when VoxelLightEngine is complete
    //-------------------------------------------------------------------------------------------
    uint8_t BlockLightEngine::GetLightValue(const BlockPos& pos) const
    {
        // [TODO] Need World reference to get chunk and query light
        // This will be implemented when VoxelLightEngine provides World access
        UNUSED(pos);
        return 0;
    }

    //-------------------------------------------------------------------------------------------
    // ComputeCorrectLight
    //
    // Calculate correct block light for a position based on:
    // - Block's emission level (e.g., glowstone=15, torch=14)
    // - Propagated light from neighbors (-1 attenuation)
    //
    // Algorithm (from World.cpp:2117-2165):
    // 1. Get emission level from block
    // 2. Opaque blocks: return emission only (no propagation)
    // 3. Non-opaque: return max(emission, max(neighbors) - 1)
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

        // [STEP 1] Get block's intrinsic light emission
        uint8_t emission = GetEmission(state);

        // [STEP 2] Opaque blocks: only emit, don't propagate from neighbors
        if (state->GetBlock()->IsOpaque(const_cast<BlockState*>(state)))
        {
            return emission;
        }

        // [STEP 3] Non-opaque: propagate light from neighbors (max - 1)
        uint8_t maxNeighborLight = 0;

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
            maxNeighborLight      = std::max(maxNeighborLight, neighborLight);
        }

        // Light propagates with -1 attenuation per block
        uint8_t propagated = (maxNeighborLight > 0) ? (maxNeighborLight - 1) : 0;

        // [STEP 4] Return max of emission and propagated light
        return std::max(emission, propagated);
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
    // Get light emission level from block state
    //-------------------------------------------------------------------------------------------
    uint8_t BlockLightEngine::GetEmission(const BlockState* state) const
    {
        if (!state || !state->GetBlock())
        {
            return 0;
        }
        return state->GetBlock()->GetBlockLightEmission();
    }
} // namespace enigma::voxel
