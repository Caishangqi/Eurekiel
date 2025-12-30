#pragma once
//-----------------------------------------------------------------------------------------------
// BlockLightEngine.hpp
//
// [NEW] Block light engine for emission-based light calculation
// Handles light sources like torches, glowstone, lava
//
// Reference: Minecraft BlockLightEngine.java
//
//-----------------------------------------------------------------------------------------------

#include "LightEngine.hpp"

namespace enigma::voxel
{
    class BlockState;

    //-------------------------------------------------------------------------------------------
    // BlockLightEngine
    //
    // Handles block light (emission-based) calculation and propagation.
    // Light propagates from emissive blocks with -1 attenuation per block.
    //-------------------------------------------------------------------------------------------
    class BlockLightEngine : public LightEngine
    {
    public:
        explicit BlockLightEngine(World* world);

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
        // Block emission helper
        //-----------------------------------------------------------------------------------
        uint8_t GetEmission(const BlockState* state) const;
    };
} // namespace enigma::voxel
