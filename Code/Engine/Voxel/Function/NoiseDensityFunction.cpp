#include "NoiseDensityFunction.hpp"

#include "Engine/Voxel/NoiseGenerator/NoiseGenerator.hpp"
using namespace enigma::voxel;

NoiseDensityFunction::NoiseDensityFunction(std::shared_ptr<NoiseGenerator> noise, float xzScale, float yScale) : m_noise(noise),
                                                                                                                 m_xzScale(xzScale),
                                                                                                                 m_yScale(yScale)
{
}

float NoiseDensityFunction::Evaluate(int x, int y, int z) const
{
    //Apply additional scaling
    float scaledX = (float)x * m_xzScale;
    float scaledY = (float)y * m_yScale;
    float scaledZ = (float)z * m_xzScale;

    //Call the Sample method of NoiseGenerator
    return m_noise->Sample(scaledX, scaledY, scaledZ);
}

std::string NoiseDensityFunction::GetTypeName() const
{
    return "engine:noise";
}
