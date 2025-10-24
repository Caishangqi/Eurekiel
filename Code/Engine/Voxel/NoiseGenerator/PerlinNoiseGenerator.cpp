#include "PerlinNoiseGenerator.hpp"

#include "Engine/Math/SmoothNoise.hpp"
using namespace enigma::voxel;

PerlinNoiseGenerator::PerlinNoiseGenerator(unsigned int seed, float scale, unsigned int numOctaves, float octavePersistence, float octaveScale, bool renormalize) : m_seed(seed),
    m_scale(scale),
    m_numOctaves(numOctaves),
    m_octavePersistence(octavePersistence),
    m_octaveScale(octaveScale),
    m_renormalize(renormalize)
{
}

float PerlinNoiseGenerator::Sample(float x, float y, float z) const
{
    return Compute3dPerlinNoise(
        x, y, z,
        m_scale,
        m_numOctaves,
        m_octavePersistence,
        m_octaveScale,
        m_renormalize,
        m_seed
    );
}

float PerlinNoiseGenerator::Sample2D(float x, float z) const
{
    // 2D Perlin noise
    return Compute2dPerlinNoise(
        x, z,
        m_scale,
        m_numOctaves,
        m_octavePersistence,
        m_octaveScale,
        m_renormalize,
        m_seed
    );
}

NoiseType PerlinNoiseGenerator::GetType() const
{
    return NoiseType::PERLIN;
}

std::string PerlinNoiseGenerator::GetConfigString() const
{
    return "Perlin(seed=" + std::to_string(m_seed) + ", octaves=" + std::to_string(m_numOctaves) + ")";
}
