#include "TerrainShaper.hpp"
#include "Engine/Voxel/Function/SplineDensityFunction.hpp"

using namespace enigma::voxel;

TerrainShaper::TerrainShaper(
    std::shared_ptr<SplineDensityFunction> offsetSpline,
    std::shared_ptr<SplineDensityFunction> factorSpline,
    std::shared_ptr<SplineDensityFunction> jaggerdnessSpline
)
    : m_offsetSpline(offsetSpline)
      , m_factorSpline(factorSpline)
      , m_jaggerdnessSpline(jaggerdnessSpline)
{
}

TerrainShaper::~TerrainShaper()
{
}

float TerrainShaper::CalculateOffset(float continentalness, float erosion, float weirdness) const
{
    if (!m_offsetSpline)
    {
        return 0.0f;
    }

    // Combine climate parameters into a single coordinate
    // This is a simplified approach - Minecraft uses more complex mapping
    float coordinate = continentalness + erosion * 0.5f + weirdness * 0.25f;

    return m_offsetSpline->EvaluateSpline(coordinate);
}

float TerrainShaper::CalculateFactor(float continentalness, float erosion, float weirdness) const
{
    if (!m_factorSpline)
    {
        return 1.0f; // Default amplification factor
    }

    // Combine climate parameters
    float coordinate = continentalness + erosion * 0.5f + weirdness * 0.25f;

    return m_factorSpline->EvaluateSpline(coordinate);
}

float TerrainShaper::CalculateJaggedness(float continentalness, float erosion, float weirdness) const
{
    if (!m_jaggerdnessSpline)
    {
        return 0.0f; // No jaggedness by default
    }

    // Combine climate parameters
    float coordinate = continentalness + erosion * 0.5f + weirdness * 0.25f;

    return m_jaggerdnessSpline->EvaluateSpline(coordinate);
}
