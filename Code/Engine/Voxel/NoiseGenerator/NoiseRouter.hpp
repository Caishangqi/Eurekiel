#pragma once
#include <memory>

namespace enigma::voxel
{
    class DensityFunction;

    /**
     * NoiseRouter - Central registry for all named DensityFunctions in world generation
     *
     * Architecture: Minecraft 1.18+ noise routing system
     *
     * Purpose:
     *   Provides centralized access to all noise sources used throughout the world
     *   generation pipeline. Acts as a dependency injection container for density
     *   functions, allowing different generation stages to query the same named
     *   noise sources consistently.
     *
     * Responsibilities:
     *   - Store references to all named DensityFunctions (climate, terrain, caves)
     *   - Provide getter methods for accessing these functions
     *   - Coordinate noise sampling across different generation stages
     *
     * What NoiseRouter does NOT do:
     *   - Calculate terrain shaping parameters (that's TerrainShaper's job)
     *   - Store intermediate computation results
     *   - Manage biome selection logic (that's BiomeSource's job)
     *
     * Key members:
     *   - Climate parameters: continentalness, erosion, temperature, humidity, weirdness
     *   - Terrain density: finalDensity, initialDensityWithoutJaggedness
     *   - Cave systems: barrierNoise, fluidLevel*, oreVein*
     *   - Terrain features: peakAndValley (ridges), depth
     *
     * Data flow:
     *   TerrainGenerator -> NoiseRouter (get climate params) -> TerrainShaper (calculate shaping)
     *                    -> Apply formula: final = offset + (base + jagg) * factor
     *
     * Reference: net.minecraft.world.level.levelgen.NoiseRouter (Minecraft 1.18+)
     */
    class NoiseRouter
    {
    public:
        // 构造函数 - Constructor
        NoiseRouter();

        // Builder模式setter方法 - Builder pattern setters
        void SetFinalDensity(std::shared_ptr<DensityFunction> func);
        void SetInitialDensityWithoutJaggedness(std::shared_ptr<DensityFunction> func);
        void SetContinentalness(std::shared_ptr<DensityFunction> func);
        void SetErosion(std::shared_ptr<DensityFunction> func);
        void SetTemperature(std::shared_ptr<DensityFunction> func);
        void SetHumidity(std::shared_ptr<DensityFunction> func);
        void SetWeirdness(std::shared_ptr<DensityFunction> func);
        void SetPeakAndValley(std::shared_ptr<DensityFunction> func);
        void SetRidges(std::shared_ptr<DensityFunction> func);
        void SetDepth(std::shared_ptr<DensityFunction> func);
        void SetBarrierNoise(std::shared_ptr<DensityFunction> func);
        void SetFluidLevelFloodedness(std::shared_ptr<DensityFunction> func);
        void SetFluidLevelSpread(std::shared_ptr<DensityFunction> func);
        void SetLavaNoise(std::shared_ptr<DensityFunction> func);
        void SetOreVeinA(std::shared_ptr<DensityFunction> func);
        void SetOreVeinB(std::shared_ptr<DensityFunction> func);

        // 现有的getter方法 - Existing getter methods
        float EvaluateFinalDensity(int x, int y, int z) const;
        float GetContinentalness(int x, int y, int z) const;
        float GetErosion(int x, int y, int z) const;
        float GetTemperature(int x, int y, int z) const;
        float GetHumidity(int x, int y, int z) const;
        float GetWeirdness(int x, int y, int z) const;

        // 新增的getter方法 - New getter methods (for Climate system)
        float GetPeakAndValley(int x, int y, int z) const;
        float GetRidges(int x, int y, int z) const;
        float GetDepth(int x, int y, int z) const;
        float GetBarrierNoise(int x, int y, int z) const;
        float GetFluidLevelFloodedness(int x, int y, int z) const;
        float GetFluidLevelSpread(int x, int y, int z) const;
        float GetLavaNoise(int x, int y, int z) const;
        float GetOreVeinA(int x, int y, int z) const;
        float GetOreVeinB(int x, int y, int z) const;

    protected:
        //Core terrain function
        std::shared_ptr<DensityFunction> m_finalDensity; // final density
        std::shared_ptr<DensityFunction> m_initialDensityWithoutJaggedness;

        // Terrain shaping parameters
        std::shared_ptr<DensityFunction> m_continentalness;
        std::shared_ptr<DensityFunction> m_erosion;
        std::shared_ptr<DensityFunction> m_peakAndValley;
        std::shared_ptr<DensityFunction> m_ridges;
        std::shared_ptr<DensityFunction> m_weirdness;
        std::shared_ptr<DensityFunction> m_depth;

        // Terrain shaping parameters (offset, factor, jaggedness) are NOT stored in NoiseRouter.
        //
        // In Minecraft 1.18+ architecture, these values are dynamically calculated by TerrainShaper
        // based on climate parameters (continentalness, erosion, weirdness). They represent the
        // OUTPUT of terrain shaping computation, not INPUT noise sources.
        //
        // Storing them as DensityFunctions here would:
        // 1. Violate single responsibility principle (NoiseRouter = noise source registry)
        // 2. Create circular dependency risks
        // 3. Prevent dynamic calculation based on climate parameters
        //
        // Correct usage:
        //   float c = noiseRouter->GetContinentalness(x, y, z);
        //   float e = noiseRouter->GetErosion(x, y, z);
        //   float w = noiseRouter->GetWeirdness(x, y, z);
        //   float offset = terrainShaper->CalculateOffset(c, e, w);
        //   float factor = terrainShaper->CalculateFactor(c, e, w);
        //   float jaggedness = terrainShaper->CalculateJaggedness(c, e, w);
        //
        // See: Engine/Voxel/Generation/TerrainShaper.hpp

        // Biome selection parameters
        std::shared_ptr<DensityFunction> m_temperature;
        std::shared_ptr<DensityFunction> m_humidity;

        // cave
        std::shared_ptr<DensityFunction> m_barrierNoise;
        std::shared_ptr<DensityFunction> m_fluidLevelFloodedness;
        std::shared_ptr<DensityFunction> m_fluidLevelSpread;
        std::shared_ptr<DensityFunction> m_lavaNoise;

        // Ore vein
        std::shared_ptr<DensityFunction> m_oreVeinA;
        std::shared_ptr<DensityFunction> m_oreVeinB;
    };
}
