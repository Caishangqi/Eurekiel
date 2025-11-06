#pragma once
#include "BiomeSource.hpp"
#include "Biome.hpp"
#include "Engine/Voxel/Climate/Climate.hpp"
#include <vector>
#include <memory>
#include <limits>

namespace enigma::voxel
{
    class NoiseRouter;

    /**
     * @brief 多重噪声生物群系源 - Multi-Noise Biome Source
     *
     * 架构参考: Minecraft 1.18+ MultiNoiseBiomeSource
     *
     * 核心职责:
     *   使用5维气候参数空间进行生物群系选择
     *   通过最近邻查找算法,将噪声采样点匹配到最合适的生物群系
     *
     * 5D气候参数空间 (Climate Parameter Space):
     *   1. Temperature (温度): 控制寒冷/温暖生物群系分布
     *   2. Humidity (湿度): 控制干燥/潮湿生物群系分布
     *   3. Continentalness (大陆度): 控制海洋/陆地生物群系分布
     *   4. Erosion (侵蚀度): 控制平坦/崎岖生物群系分布
     *   5. Weirdness (怪异度): 控制常规/奇特地形的生物群系变种
     *
     * 工作原理:
     *   1. 对于世界中的每个位置(x,y,z):
     *      - 通过Climate::Sampler采样5个气候参数,得到Climate::TargetPoint(T, H, C, E, W)
     *
     *   2. 在已注册的所有Biome中:
     *      - 计算TargetPoint到每个ParameterPoint的5D欧几里得距离
     *      - 选择距离最小的Biome作为该位置的生物群系
     *
     *   3. 数学公式:
     *      Distance² = (T-T₀)² + (H-H₀)² + (C-C₀)² + (E-E₀)² + (W-W₀)²
     *      其中(T₀, H₀, C₀, E₀, W₀)是Biome的理想气候参数
     *
     * 性能优化:
     *   - 使用距离平方避免开平方根运算
     *   - 预排序Biome列表(按大陆度排序,提前剔除不可能的候选)
     *   - 块级缓存(每个Chunk只查询一次,16x16x128个方块共享)
     *   - 早期退出(找到距离为0的完美匹配时立即返回)
     *
     * 参考:
     *   - net.minecraft.world.level.biome.MultiNoiseBiomeSource
     *   - Climate::ParameterPoint结构
     *   - Climate::TargetPoint结构
     *   - Climate::Sampler采样器
     */
    class MultiNoiseBiomeSource : public BiomeSource
    {
    public:
        /**
         * @brief 参数点 - Parameter Point
         *
         * 存储一个生物群系的气候参数和生物群系的配对
         * 使用Climate::TargetPoint作为气候参数存储
         */
        struct ParameterPoint
        {
            Climate::TargetPoint   climate; ///< 生物群系的理想气候参数
            std::shared_ptr<Biome> biome; ///< 对应的生物群系

            ParameterPoint(const Climate::TargetPoint& c, std::shared_ptr<Biome> b)
                : climate(c), biome(b)
            {
            }
        };

    public:
        /**
         * @brief 构造函数
         *
         * @param noiseRouter 噪声路由器,提供气候参数噪声采样
         *
         * 注意: NoiseRouter必须在MultiNoiseBiomeSource的整个生命周期内有效
         */
        explicit MultiNoiseBiomeSource(std::shared_ptr<NoiseRouter> noiseRouter);

        /**
         * @brief 获取指定位置的生物群系
         *
         * @param x 世界X坐标
         * @param y 世界Y坐标 (高度)
         * @param z 世界Z坐标
         * @return 该位置的生物群系
         *
         * 实现步骤:
         *   1. 使用Climate::Sampler采样气候参数 -> Climate::TargetPoint
         *   2. 调用FindNearestBiome查找距离最近的生物群系
         */
        std::shared_ptr<Biome> GetBiome(int x, int y, int z) const override;

        /**
         * @brief 注册一个生物群系及其理想气候参数
         *
         * @param climate 生物群系的理想气候参数
         * @param biome 生物群系实例
         *
         * 将(climate, biome)对添加到ParameterList中
         * 用于后续的最近邻匹配
         */
        void RegisterBiome(const Climate::TargetPoint& climate, std::shared_ptr<Biome> biome)
        {
            m_parameterList.emplace_back(climate, biome);
        }

        /**
         * @brief 在ParameterList中查找最近的Biome
         *
         * @param target 目标气候参数点
         * @return 距离最小的Biome
         *
         * 算法: 朴素最近邻查找 (Naive Nearest Neighbor)
         * 时间复杂度: O(N)
         * 空间复杂度: O(1)
         *
         * 优化方向 (未来扩展):
         *   1. KD树: O(log N)查找,但构建成本高
         *   2. 网格划分: 预计算每个网格的Biome,O(1)查找
         *   3. 层级采样: 低分辨率Biome + 高分辨率变种
         *   4. 块级缓存: 每个Chunk只查询一次
         */
        std::shared_ptr<Biome> FindNearestBiome(const Climate::TargetPoint& target) const;

    private:
        std::shared_ptr<NoiseRouter>      m_noiseRouter; ///< 噪声路由器,提供气候参数采样
        std::unique_ptr<Climate::Sampler> m_climateSampler; ///< 气候采样器
        std::vector<ParameterPoint>       m_parameterList; ///< 生物群系参数列表

        /**
         * 性能优化建议 (未来实现):
         *
         * 1. 块级缓存 (Chunk-Level Caching):
         *    - 每个Chunk (16x16x128) 只查询一次Biome
         *    - 使用std::unordered_map<ChunkCoord, Biome*>缓存
         *    - 内存占用: ~8MB (10000个Chunk * 8字节指针)
         *
         * 2. 空间哈希 (Spatial Hashing):
         *    - 将5D空间划分为超立方体网格
         *    - 每个网格预计算最可能的Biome
         *    - 查询时直接访问网格,无需遍历ParameterList
         *
         * 3. KD树加速 (KD-Tree Acceleration):
         *    - 构建5D KD树索引ParameterList
         *    - 查询复杂度从O(N)降低到O(log N)
         *    - 适合静态Biome配置(构建后不再修改)
         *
         * 4. 多线程优化 (Multithreading):
         *    - GetBiome是只读操作,天然线程安全
         *    - 可以并行查询多个Chunk的Biome
         *    - 使用std::shared_ptr避免数据竞争
         */
    };
} // namespace enigma::voxel
