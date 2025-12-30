#pragma once
//-----------------------------------------------------------------------------------------------
// SkyLightEngine.hpp
//
// [NEW] Sky light engine for outdoor light calculation
// Handles sunlight propagation from sky blocks downward
//
// Reference: Minecraft SkyLightEngine.java
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
    // - Opaque blocks = 0
    // - Transparent blocks = max(neighbors) - 1
    //-------------------------------------------------------------------------------------------
    class SkyLightEngine : public LightEngine
    {
    public:
        SkyLightEngine() = default;

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
