#pragma once
#include <string>

namespace enigma::voxel
{
    // Noise type enumeration
    enum class NoiseType
    {
        PERLIN, // Perlin noise
        FRACTAL, // fractal noise
        SIMPLEX, // Simplex noise (future support)
        RAW // original noise
    };

    // NoiseGenerator - abstract base class
    // Purpose: Unify the interfaces of different noise functions to facilitate the use of DensityFunction
    class NoiseGenerator
    {
    public:
        virtual ~NoiseGenerator() = default;
        // Core sampling function - 3D noise
        virtual float Sample(float x, float y, float z) const = 0;
        // Optional: 2D sampling (required for some applications)
        virtual float Sample2D(float x, float z) const;
        // Get the noise type
        virtual NoiseType GetType() const = 0;
        // Get configuration information (for serialization)
        virtual std::string GetConfigString() const = 0;
    };
}
