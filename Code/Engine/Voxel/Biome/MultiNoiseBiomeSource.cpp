#include "MultiNoiseBiomeSource.hpp"
#include "Engine/Voxel/NoiseGenerator/NoiseRouter.hpp"
#include <algorithm>

namespace enigma::voxel
{
    // 构造函数实现
    MultiNoiseBiomeSource::MultiNoiseBiomeSource(std::shared_ptr<NoiseRouter> noiseRouter)
        : m_noiseRouter(noiseRouter)
          , m_climateSampler(nullptr)
    {
        if (m_noiseRouter)
        {
            // 创建气候采样器,传入NoiseRouter的原始指针
            m_climateSampler = std::make_unique<Climate::Sampler>(m_noiseRouter.get());
        }
    }

    std::shared_ptr<Biome> MultiNoiseBiomeSource::GetBiome(int x, int y, int z) const
    {
        // 步骤1: 使用Climate::Sampler采样当前位置的气候参数
        Climate::TargetPoint target;
        if (m_climateSampler)
        {
            target = m_climateSampler->Sample(x, y, z);
        }
        else
        {
            // 如果采样器为空,使用默认气候参数
            target = Climate::TargetPoint(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        }

        // 步骤2: 查找最近的生物群系
        return FindNearestBiome(target);
    }

    std::shared_ptr<Biome> MultiNoiseBiomeSource::FindNearestBiome(const Climate::TargetPoint& target) const
    {
        // 边界情况: 如果没有注册任何生物群系,返回nullptr
        if (m_parameterList.empty())
        {
            return nullptr;
        }

        // 初始化:选择第一个生物群系作为初始最近候选
        std::shared_ptr<Biome> nearestBiome       = m_parameterList[0].biome;
        float                  minDistanceSquared = target.DistanceSquared(m_parameterList[0].climate);

        // 遍历所有已注册的生物群系,查找距离最小的
        for (size_t i = 1; i < m_parameterList.size(); ++i)
        {
            const auto& paramPoint      = m_parameterList[i];
            float       distanceSquared = target.DistanceSquared(paramPoint.climate);

            if (distanceSquared < minDistanceSquared)
            {
                minDistanceSquared = distanceSquared;
                nearestBiome       = paramPoint.biome;

                // 性能优化: 早期退出
                // 如果找到完美匹配(距离为0),立即返回
                if (minDistanceSquared == 0.0f)
                {
                    break;
                }
            }
        }

        return nearestBiome;
    }
} // namespace enigma::voxel
