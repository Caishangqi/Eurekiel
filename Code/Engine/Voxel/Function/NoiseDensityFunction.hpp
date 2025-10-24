#pragma once
#include "DensityFunction.hpp"

namespace enigma::voxel
{
    class NoiseGenerator;

    class NoiseDensityFunction : public DensityFunction
    {
    public:
        NoiseDensityFunction(
            std::shared_ptr<NoiseGenerator> noise,
            float                           xzScale = 1.0f,
            float                           yScale  = 1.0f
        );

        float Evaluate(int x, int y, int z) const override;
        //static std::unique_ptr<NoiseDensityFunction> FromJson( const Json&    json, NoiseRegistry& noiseRegistry);

        std::string GetTypeName() const override;

    private:
        std::shared_ptr<NoiseGenerator> m_noise;
        float                           m_xzScale; // Additional scaling of the XZ plane
        float                           m_yScale; // Additional Y-axis scaling
    };
}
