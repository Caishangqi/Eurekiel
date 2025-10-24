#pragma once
#include <memory>

namespace enigma::voxel
{
    class DensityFunction;

    class NoiseRouter
    {
    public:
        float EvaluateFinalDensity(int x, int y, int z) const;
        float GetContinentalness(int x, int y, int z) const;

        float GetErosion(int x, int y, int z) const;
        float GetTemperature(int x, int y, int z) const;
        float GetHumidity(int x, int y, int z) const;
        float GetWeirdness(int x, int y, int z) const;

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
        std::shared_ptr<DensityFunction> m_offset;
        std::shared_ptr<DensityFunction> m_factor;
        std::shared_ptr<DensityFunction> m_jaggedness;

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
