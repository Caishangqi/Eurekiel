#include "NoiseGenerator.hpp"
using namespace enigma::voxel;

float NoiseGenerator::Sample2D(float x, float z) const
{
    return Sample(x, 0.0f, z);
}
