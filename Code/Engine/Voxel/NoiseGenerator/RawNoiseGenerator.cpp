#include "RawNoiseGenerator.hpp"

#include <cmath>

#include "Engine/Math/RawNoise.hpp"
using namespace enigma::voxel;

RawNoiseGenerator::RawNoiseGenerator(unsigned int seed, bool useNegOneToOne) : m_seed(seed), m_useNegOneToOne(useNegOneToOne)
{
}

float RawNoiseGenerator::Sample(float x, float y, float z) const
{
    // Convert float to int (take the nearest integer coordinate)
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    int iz = static_cast<int>(std::floor(z));

    if (m_useNegOneToOne)
    {
        return Get3dNoiseNegOneToOne(ix, iy, iz, m_seed);
    }
    else
    {
        return Get3dNoiseZeroToOne(ix, iy, iz, m_seed);
    }
}

float RawNoiseGenerator::Sample2D(float x, float z) const
{
    int ix = static_cast<int>(std::floor(x));
    int iz = static_cast<int>(std::floor(z));

    if (m_useNegOneToOne)
    {
        return Get2dNoiseNegOneToOne(ix, iz, m_seed);
    }
    else
    {
        return Get2dNoiseZeroToOne(ix, iz, m_seed);
    }
}

NoiseType RawNoiseGenerator::GetType() const
{
    return NoiseType::RAW;
}

std::string RawNoiseGenerator::GetConfigString() const
{
    return "Raw(seed=" + std::to_string(m_seed) + ", range=" + (m_useNegOneToOne ? "[-1,1]" : "[0,1]") + ")";
}
