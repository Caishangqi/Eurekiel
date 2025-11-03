#include "NoiseRouter.hpp"

#include "Engine/Voxel/Function/DensityFunction.hpp"

using namespace enigma::voxel;

// 构造函数 - 初始化所有成员为nullptr
NoiseRouter::NoiseRouter()
    : m_finalDensity(nullptr)
      , m_initialDensityWithoutJaggedness(nullptr)
      , m_continentalness(nullptr)
      , m_erosion(nullptr)
      , m_peakAndValley(nullptr)
      , m_ridges(nullptr)
      , m_weirdness(nullptr)
      , m_depth(nullptr)
      , m_temperature(nullptr)
      , m_humidity(nullptr)
      , m_barrierNoise(nullptr)
      , m_fluidLevelFloodedness(nullptr)
      , m_fluidLevelSpread(nullptr)
      , m_lavaNoise(nullptr)
      , m_oreVeinA(nullptr)
      , m_oreVeinB(nullptr)
{
}

// Setter方法实现 - Setter method implementations

void NoiseRouter::SetFinalDensity(std::shared_ptr<DensityFunction> func)
{
    m_finalDensity = func;
}

void NoiseRouter::SetInitialDensityWithoutJaggedness(std::shared_ptr<DensityFunction> func)
{
    m_initialDensityWithoutJaggedness = func;
}

void NoiseRouter::SetContinentalness(std::shared_ptr<DensityFunction> func)
{
    m_continentalness = func;
}

void NoiseRouter::SetErosion(std::shared_ptr<DensityFunction> func)
{
    m_erosion = func;
}

void NoiseRouter::SetTemperature(std::shared_ptr<DensityFunction> func)
{
    m_temperature = func;
}

void NoiseRouter::SetHumidity(std::shared_ptr<DensityFunction> func)
{
    m_humidity = func;
}

void NoiseRouter::SetWeirdness(std::shared_ptr<DensityFunction> func)
{
    m_weirdness = func;
}

void NoiseRouter::SetPeakAndValley(std::shared_ptr<DensityFunction> func)
{
    m_peakAndValley = func;
}

void NoiseRouter::SetRidges(std::shared_ptr<DensityFunction> func)
{
    m_ridges = func;
}

void NoiseRouter::SetDepth(std::shared_ptr<DensityFunction> func)
{
    m_depth = func;
}

void NoiseRouter::SetBarrierNoise(std::shared_ptr<DensityFunction> func)
{
    m_barrierNoise = func;
}

void NoiseRouter::SetFluidLevelFloodedness(std::shared_ptr<DensityFunction> func)
{
    m_fluidLevelFloodedness = func;
}

void NoiseRouter::SetFluidLevelSpread(std::shared_ptr<DensityFunction> func)
{
    m_fluidLevelSpread = func;
}

void NoiseRouter::SetLavaNoise(std::shared_ptr<DensityFunction> func)
{
    m_lavaNoise = func;
}

void NoiseRouter::SetOreVeinA(std::shared_ptr<DensityFunction> func)
{
    m_oreVeinA = func;
}

void NoiseRouter::SetOreVeinB(std::shared_ptr<DensityFunction> func)
{
    m_oreVeinB = func;
}

// 现有的getter方法实现 - Existing getter method implementations

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

// 新增的getter方法实现 - New getter method implementations

float NoiseRouter::GetPeakAndValley(int x, int y, int z) const
{
    return m_peakAndValley ? m_peakAndValley->Evaluate(x, y, z) : 0.0f;
}

float NoiseRouter::GetRidges(int x, int y, int z) const
{
    return m_ridges ? m_ridges->Evaluate(x, y, z) : 0.0f;
}

float NoiseRouter::GetDepth(int x, int y, int z) const
{
    return m_depth ? m_depth->Evaluate(x, y, z) : 0.0f;
}

float NoiseRouter::GetBarrierNoise(int x, int y, int z) const
{
    return m_barrierNoise ? m_barrierNoise->Evaluate(x, y, z) : 0.0f;
}

float NoiseRouter::GetFluidLevelFloodedness(int x, int y, int z) const
{
    return m_fluidLevelFloodedness ? m_fluidLevelFloodedness->Evaluate(x, y, z) : 0.0f;
}

float NoiseRouter::GetFluidLevelSpread(int x, int y, int z) const
{
    return m_fluidLevelSpread ? m_fluidLevelSpread->Evaluate(x, y, z) : 0.0f;
}

float NoiseRouter::GetLavaNoise(int x, int y, int z) const
{
    return m_lavaNoise ? m_lavaNoise->Evaluate(x, y, z) : 0.0f;
}

float NoiseRouter::GetOreVeinA(int x, int y, int z) const
{
    return m_oreVeinA ? m_oreVeinA->Evaluate(x, y, z) : 0.0f;
}

float NoiseRouter::GetOreVeinB(int x, int y, int z) const
{
    return m_oreVeinB ? m_oreVeinB->Evaluate(x, y, z) : 0.0f;
}
