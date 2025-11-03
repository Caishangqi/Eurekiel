#include "Climate.hpp"
#include "Engine/Voxel/NoiseGenerator/NoiseRouter.hpp"

namespace enigma::voxel
{
    Climate::TargetPoint Climate::Sampler::Sample(int x, int y, int z) const
    {
        if (!m_noiseRouter)
        {
            // 如果NoiseRouter为空,返回默认气候参数 (温和、适中、陆地、平坦、正常)
            // 这是一个回退机制,确保即使NoiseRouter未正确初始化也不会崩溃
            return TargetPoint(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        }

        // 从NoiseRouter采样5个气候参数
        // 每个参数都是一个独立的DensityFunction,通过噪声生成器计算
        TargetPoint result;
        result.temperature     = m_noiseRouter->GetTemperature(x, y, z);
        result.humidity        = m_noiseRouter->GetHumidity(x, y, z);
        result.continentalness = m_noiseRouter->GetContinentalness(x, y, z);
        result.erosion         = m_noiseRouter->GetErosion(x, y, z);
        result.weirdness       = m_noiseRouter->GetWeirdness(x, y, z);

        return result;
    }
} // namespace enigma::voxel
