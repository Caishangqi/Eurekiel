#pragma once
//-----------------------------------------------------------------------------------------------
// BlockLightEngine.hpp
//
// [REFACTORED] Block light engine for emission-based light calculation
// Handles light sources like torches, glowstone, lava with proper attenuation
//
// [MINECRAFT REF] BlockLightEngine.java - Block light propagation
// [MINECRAFT REF] LightEngine.java - Base light engine with attenuation
//
// Key features:
// - GetLightBlock-based attenuation (not just opaque/transparent)
// - GetLightEmission from BlockState cache
// - Attenuation is max(1, lightBlock) per block
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
    // Light propagates from emissive blocks with variable attenuation based on GetLightBlock().
    //
    // [MINECRAFT REF] LightEngine.java:79 - Math.max(1, blockState.getLightBlock(...))
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
