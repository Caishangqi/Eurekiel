#include "FractalNoiseGenerator.hpp"

#include "Engine/Math/SmoothNoise.hpp"
using namespace enigma::voxel;

FractalNoiseGenerator::FractalNoiseGenerator(unsigned int seed, float scale, unsigned int numOctaves, float octavePersistence, float octaveScale, bool renormalize)
    : m_seed(seed),
      m_scale(scale),
      m_numOctaves(numOctaves),
      m_octavePersistence(octavePersistence),
      m_octaveScale(octaveScale),
      m_renormalize(renormalize)
{
}

float FractalNoiseGenerator::Sample(float x, float y, float z) const
{
    return Compute3dFractalNoise(
        x, y, z,
        m_scale,
        m_numOctaves,
        m_octavePersistence,
        m_octaveScale,
        m_renormalize,
        m_seed
    );
}

float FractalNoiseGenerator::Sample2D(float x, float z) const
{
    return Compute2dFractalNoise(
        x, z,
        m_scale,
        m_numOctaves,
        m_octavePersistence,
        m_octaveScale,
        m_renormalize,
        m_seed
    );
}

NoiseType FractalNoiseGenerator::GetType() const
{
    return NoiseType::FRACTAL;
}

std::string FractalNoiseGenerator::GetConfigString() const
{
    return "Fractal(seed=" + std::to_string(m_seed) + ", octaves=" + std::to_string(m_numOctaves) + ")";
}
