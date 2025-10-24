#include "NoiseRouter.hpp"

#include "Engine/Voxel/Function/DensityFunction.hpp"

using namespace enigma::voxel;

float NoiseRouter::EvaluateFinalDensity(int x, int y, int z) const
{
    return m_finalDensity->Evaluate(x, y, z);
}

float NoiseRouter::GetContinentalness(int x, int y, int z) const
{
    return m_continentalness->Evaluate(x, y, z);
}

float NoiseRouter::GetErosion(int x, int y, int z) const
{
    return m_erosion->Evaluate(x, y, z);
}

float NoiseRouter::GetTemperature(int x, int y, int z) const
{
    return m_temperature->Evaluate(x, y, z);
}

float NoiseRouter::GetHumidity(int x, int y, int z) const
{
    return m_humidity->Evaluate(x, y, z);
}

float NoiseRouter::GetWeirdness(int x, int y, int z) const
{
    return m_weirdness->Evaluate(x, y, z);
}
