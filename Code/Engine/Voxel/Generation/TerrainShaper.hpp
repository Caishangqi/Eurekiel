#pragma once
#include <memory>

namespace enigma::voxel
{
    class SplineDensityFunction;

    /**
     * TerrainShaper - Minecraft 1.18+ terrain shaping system
     * 
     * Manages three spline curves (offset, factor, jaggedness) that shape terrain
     * based on climate parameters (continentalness, erosion, weirdness).
     * 
     * Final density formula:
     *   final_density = offset + (base_3d_noise + jaggedness) * factor
     * 
     * Reference: Minecraft 1.18+ TerrainShaper
     */
    class TerrainShaper
    {
    public:
        /**
         * Construct TerrainShaper with three spline curves
         * 
         * @param offsetSpline - Height offset spline
         * @param factorSpline - Terrain amplification factor spline  
         * @param jaggerdnessSpline - Terrain jaggedness/roughness spline
         */
        TerrainShaper(
            std::shared_ptr<SplineDensityFunction> offsetSpline,
            std::shared_ptr<SplineDensityFunction> factorSpline,
            std::shared_ptr<SplineDensityFunction> jaggerdnessSpline
        );

        ~TerrainShaper();

        /**
         * Calculate height offset based on climate parameters
         * 
         * @param continentalness - Continental parameter (-1 to 1)
         * @param erosion - Erosion parameter (-1 to 1)
         * @param weirdness - Weirdness parameter (-1 to 1)
         * @return Height offset value
         */
        float CalculateOffset(float continentalness, float erosion, float weirdness) const;

        /**
         * Calculate terrain amplification factor
         * 
         * @param continentalness - Continental parameter (-1 to 1)
         * @param erosion - Erosion parameter (-1 to 1)
         * @param weirdness - Weirdness parameter (-1 to 1)
         * @return Amplification factor (typically 0 to 1)
         */
        float CalculateFactor(float continentalness, float erosion, float weirdness) const;

        /**
         * Calculate terrain jaggedness/roughness
         * 
         * @param continentalness - Continental parameter (-1 to 1)
         * @param erosion - Erosion parameter (-1 to 1)
         * @param weirdness - Weirdness parameter (-1 to 1)
         * @return Jaggedness value
         */
        float CalculateJaggedness(float continentalness, float erosion, float weirdness) const;

        // Serialization (empty implementation for now)
        // JsonObject ToJson() const;
        // static TerrainShaper FromJson(const JsonObject& json);

    private:
        std::shared_ptr<SplineDensityFunction> m_offsetSpline;
        std::shared_ptr<SplineDensityFunction> m_factorSpline;
        std::shared_ptr<SplineDensityFunction> m_jaggerdnessSpline;
    };
}
