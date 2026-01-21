#pragma once
//-----------------------------------------------------------------------------------------------
// SkyLightEngine.hpp
//
// [REFACTORED] Sky light engine for outdoor light calculation
// Handles sunlight propagation from sky blocks downward with proper attenuation
//
// [MINECRAFT REF] SkyLightEngine.java - Sky light propagation
// [MINECRAFT REF] LightEngine.java - Base light engine with attenuation
//
// Key features:
// - PropagatesSkylightDown support for leaves, glass, etc.
// - GetLightBlock-based attenuation (not just opaque/transparent)
// - Skylight passes through vertically without loss when PropagatesSkylightDown is true
//
//-----------------------------------------------------------------------------------------------

#include "LightEngine.hpp"

namespace enigma::voxel
{
    class BlockState;

    //-------------------------------------------------------------------------------------------
    // SkyLightEngine
    //
    // Calculates sky light values based on:
    // - Sky blocks (direct sunlight) = 15
    // - Blocks with lightBlock >= 15 = 0 (fully opaque)
    // - PropagatesSkylightDown = true: skylight from above passes without attenuation
    // - Other cases: max(neighbors) - max(1, lightBlock)
    //
    // [MINECRAFT REF] BlockBehaviour.java:368-370 - propagatesSkylightDown
    // [MINECRAFT REF] LightEngine.java:79 - Math.max(1, blockState.getLightBlock(...))
    //-------------------------------------------------------------------------------------------
    class SkyLightEngine : public LightEngine
    {
    public:
        explicit SkyLightEngine(World* world);

        //-----------------------------------------------------------------------------------
        // Override from LightEngine
        //-----------------------------------------------------------------------------------
        uint8_t GetLightValue(const BlockPos& pos) const override;
        uint8_t ComputeCorrectLight(const BlockIterator& iter) const override;

    protected:
        //-----------------------------------------------------------------------------------
        // Override from LightEngine - Light value access
        //-----------------------------------------------------------------------------------
        void    SetLightValue(Chunk* chunk, int32_t x, int32_t y, int32_t z, uint8_t value) override;
        uint8_t GetCurrentLightValue(Chunk* chunk, int32_t x, int32_t y, int32_t z) const override;

    private:
        //-----------------------------------------------------------------------------------
        // Sky block detection helper
        //-----------------------------------------------------------------------------------
        bool IsSkyBlock(const BlockIterator& iter) const;
    };
} // namespace enigma::voxel
