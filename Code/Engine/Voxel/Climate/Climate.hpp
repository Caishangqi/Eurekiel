#pragma once
#include <memory>

namespace enigma::voxel
{
    class NoiseRouter;

    /**
     * @brief Climate - 气候系统模块 (Climate System Module)
     *
     * 架构参考: Minecraft 1.18+ Climate System
     *
     * 核心职责:
     *   提供气候参数采样和气候点定义,支持MultiNoiseBiomeSource的5D生物群系匹配
     *
     * 设计目标:
     *   - 将气候采样逻辑从MultiNoiseBiomeSource中解耦
     *   - 提供统一的气候参数访问接口
     *   - 支持未来扩展(例如动态气候、季节变化)
     *
     * 架构分层:
     *   NoiseRouter (噪声源) -> Climate::Sampler (气候采样)
     *       -> Climate::TargetPoint (气候点) -> MultiNoiseBiomeSource (生物群系匹配)
     *
     * 参考: net.minecraft.world.level.biome.Climate (Minecraft 1.18+)
     */
    class Climate
    {
    public:
        /**
         * @brief 目标点 - Target Point
         *
         * 表示世界中某个位置(x, y, z)的气候参数采样结果
         * 用于MultiNoiseBiomeSource的5D最近邻查找
         *
         * 5D气候参数空间:
         *   1. temperature: 温度 [-1.0, 1.0], -1=冰冻, 0=温带, 1=炎热
         *   2. humidity: 湿度 [-1.0, 1.0], -1=干燥, 0=适中, 1=潮湿
         *   3. continentalness: 大陆度 [-1.0, 1.0], -1=深海, 0=海岸, 1=内陆
         *   4. erosion: 侵蚀度 [-1.0, 1.0], -1=平坦, 0=正常, 1=崎岖
         *   5. weirdness: 怪异度 [-1.0, 1.0], 控制地形的"奇特性"
         *
         * 技术细节:
         *   - Minecraft内部将浮点值量化为long类型(乘以10000)
         *   - 我们保持浮点类型以简化实现和提高性能
         *
         * 使用示例:
         *   Climate::Sampler sampler(&noiseRouter);
         *   Climate::TargetPoint point = sampler.Sample(100, 64, 200);
         *   // point.temperature, point.humidity, ...
         */
        struct TargetPoint
        {
            float temperature; ///< 温度参数 [-1.0, 1.0]
            float humidity; ///< 湿度参数 [-1.0, 1.0]
            float continentalness; ///< 大陆度参数 [-1.0, 1.0]
            float erosion; ///< 侵蚀度参数 [-1.0, 1.0]
            float weirdness; ///< 怪异度参数 [-1.0, 1.0]

            /**
             * @brief 构造函数 - 初始化气候参数
             */
            TargetPoint(float temp = 0.0f, float humid = 0.0f,
                        float cont = 0.0f, float ero   = 0.0f, float weird = 0.0f)
                : temperature(temp), humidity(humid), continentalness(cont)
                  , erosion(ero), weirdness(weird)
            {
            }

            /**
             * @brief 计算到另一个气候点的欧几里得距离(平方)
             *
             * 用于MultiNoiseBiomeSource的最近邻查找
             * 返回平方距离以避免开平方根运算(性能优化)
             *
             * @param other 另一个气候参数点
             * @return 距离的平方
             */
            float DistanceSquared(const TargetPoint& other) const
            {
                float dt = temperature - other.temperature;
                float dh = humidity - other.humidity;
                float dc = continentalness - other.continentalness;
                float de = erosion - other.erosion;
                float dw = weirdness - other.weirdness;
                return dt * dt + dh * dh + dc * dc + de * de + dw * dw;
            }
        };

        /**
         * @brief 参数点 - Parameter Point
         *
         * 存储一个生物群系的理想气候参数
         * MultiNoiseBiomeSource使用ParameterPoint列表进行生物群系匹配
         *
         * 设计说明:
         *   - ParameterPoint = TargetPoint的语义扩展
         *   - TargetPoint表示"世界中的实际气候"
         *   - ParameterPoint表示"生物群系的理想气候"
         *   - 两者结构相同,但用途不同
         *
         * 未来扩展:
         *   - 可添加权重系数(weight),用于加权距离计算
         *   - 可添加范围参数(min/max),支持范围匹配
         *
         * 当前实现:
         *   - 使用TargetPoint作为类型别名,保持简洁
         *   - 未来可扩展为独立结构体
         */
        using ParameterPoint = TargetPoint;

        /**
         * @brief 气候采样器 - Climate Sampler
         *
         * 负责从NoiseRouter采样5个气候参数,构建TargetPoint
         *
         * 核心职责:
         *   - 封装对NoiseRouter的访问
         *   - 提供统一的气候采样接口
         *   - 将噪声采样逻辑与生物群系匹配逻辑解耦
         *
         * 设计优势:
         *   - MultiNoiseBiomeSource不需要直接依赖NoiseRouter
         *   - 未来可轻松替换采样实现(例如,从缓存读取)
         *   - 支持单元测试(可Mock Sampler)
         *
         * 使用示例:
         *   std::shared_ptr<NoiseRouter> router = CreateNoiseRouter();
         *   Climate::Sampler sampler(router.get());
         *
         *   // 采样世界坐标(100, 64, 200)的气候参数
         *   Climate::TargetPoint point = sampler.Sample(100, 64, 200);
         *
         *   // 使用气候点进行生物群系匹配
         *   std::shared_ptr<Biome> biome = biomeSource->FindNearestBiome(point);
         *
         * 注意:
         *   - NoiseRouter必须在Sampler的整个生命周期内有效
         *   - Sampler不拥有NoiseRouter,调用者负责管理其生命周期
         */
        class Sampler
        {
        public:
            /**
             * @brief 构造函数
             *
             * @param noiseRouter 噪声路由器,提供气候参数噪声采样
             *
             * 注意: NoiseRouter必须在Sampler的整个生命周期内有效
             */
            explicit Sampler(NoiseRouter* noiseRouter)
                : m_noiseRouter(noiseRouter)
            {
            }

            /**
             * @brief 采样指定位置的气候参数
             *
             * 从NoiseRouter采样5个气候参数,构建TargetPoint
             *
             * @param x 世界X坐标
             * @param y 世界Y坐标 (高度)
             * @param z 世界Z坐标
             * @return 目标点(5D气候参数)
             *
             * 实现细节:
             *   - temperature = NoiseRouter::GetTemperature(x, y, z)
             *   - humidity = NoiseRouter::GetHumidity(x, y, z)
             *   - continentalness = NoiseRouter::GetContinentalness(x, y, z)
             *   - erosion = NoiseRouter::GetErosion(x, y, z)
             *   - weirdness = NoiseRouter::GetWeirdness(x, y, z)
             *
             * 性能考虑:
             *   - 每次调用会进行5次噪声采样
             *   - 建议在块级别缓存结果(每个Chunk只采样一次)
             *   - 未来可优化为SIMD批量采样
             */
            TargetPoint Sample(int x, int y, int z) const;

        private:
            NoiseRouter* m_noiseRouter; ///< 噪声路由器(非拥有指针)
        };
    };
} // namespace enigma::voxel
