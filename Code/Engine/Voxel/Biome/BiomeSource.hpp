#pragma once
#include <memory>

namespace enigma::voxel
{
    class Biome;

    /**
     * @brief 生物群系源接口 - BiomeSource Interface
     *
     * 架构参考: Minecraft 1.18+ BiomeSource Abstract Class
     *
     * 核心职责:
     *   提供一个抽象接口,根据世界坐标返回对应的生物群系
     *
     * 设计模式: Strategy Pattern (策略模式)
     *   - BiomeSource是抽象策略接口
     *   - MultiNoiseBiomeSource, FixedBiomeSource等是具体策略实现
     *   - TerrainGenerator通过BiomeSource*获取Biome,无需关心具体实现
     *
     * 为什么需要BiomeSource抽象层?
     *   1. 解耦: TerrainGenerator不依赖具体的生物群系生成算法
     *   2. 可扩展: 轻松添加新的生物群系生成策略(棋盘格、分层、自定义等)
     *   3. 可测试: 可以使用FixedBiomeSource进行单元测试
     *   4. 可配置: 运行时切换不同的生物群系生成策略
     *
     * 常见实现:
     *   - MultiNoiseBiomeSource: 基于5D气候参数的多重噪声生物群系源(主要实现)
     *   - FixedBiomeSource: 固定返回单一生物群系(调试用)
     *   - CheckerboardBiomeSource: 棋盘格生物群系(测试用)
     *   - VoronoiBiomeSource: 基于Voronoi图的生物群系布局(未来扩展)
     *
     * 数据流:
     *   TerrainGenerator -> BiomeSource::GetBiome(x,y,z)
     *       -> MultiNoiseBiomeSource (采样NoiseRouter, 5D查找)
     *           -> 返回最匹配的Biome
     *               -> TerrainGenerator (应用表面规则)
     *
     * 参考: net.minecraft.world.level.biome.BiomeSource (Minecraft 1.18+)
     */
    class BiomeSource
    {
    public:
        virtual ~BiomeSource() = default;

        /**
         * @brief 获取指定位置的生物群系
         *
         * 核心接口方法,所有BiomeSource实现必须提供此方法
         *
         * @param x 世界X坐标
         * @param y 世界Y坐标 (注意: 在SimpleMiner中Y是水平轴, Z是高度轴)
         * @param z 世界Z坐标 (高度)
         * @return 生物群系对象的共享指针
         *
         * 实现注意事项:
         *   1. 返回值不能为nullptr (必须返回有效的Biome)
         *   2. 对于同一坐标,必须返回相同的Biome (确定性)
         *   3. 应该缓存结果以提升性能 (可选)
         *   4. 线程安全性取决于具体实现 (MultiNoiseBiomeSource是线程安全的)
         *
         * 性能考虑:
         *   - GetBiome会在地形生成时被频繁调用
         *   - MultiNoiseBiomeSource每次调用需要:
         *       1. 采样5个2D噪声 (temperature, humidity, continentalness, erosion, weirdness)
         *       2. 在~7000个ParameterPoint中进行最近邻查找
         *   - 优化策略:
         *       1. 块级缓存 (每个Chunk只查询一次Biome)
         *       2. 空间哈希 (预计算区域的Biome)
         *       3. 层级采样 (低分辨率Biome + 高分辨率变种)
         */
        virtual std::shared_ptr<Biome> GetBiome(int x, int y, int z) const = 0;

        /**
         * @brief 获取所有可能的生物群系 (未来扩展)
         *
         * 返回此BiomeSource可能生成的所有生物群系列表
         * 用于预加载资源、UI显示等
         *
         * @return 所有可能生成的生物群系列表
         */
        // virtual std::vector<std::shared_ptr<Biome>> GetPossibleBiomes() const = 0;

        /**
         * @brief 获取BiomeSource的类型名称 (调试用)
         *
         * @return BiomeSource类型名称 (例如: "MultiNoiseBiomeSource")
         */
        // virtual std::string GetTypeName() const = 0;
    };

    /**
     * @brief 固定生物群系源 - Fixed Biome Source (调试用)
     *
     * 总是返回同一个生物群系,用于测试和调试
     *
     * 使用场景:
     *   1. 单元测试: 测试表面规则应用逻辑
     *   2. 调试: 隔离地形生成和生物群系选择的问题
     *   3. 原型: 快速验证新的表面规则设计
     *
     * 示例:
     *   auto plainsBiome = std::make_shared<Biome>("plains", ...);
     *   auto biomeSource = std::make_shared<FixedBiomeSource>(plainsBiome);
     *   // 整个世界都是Plains生物群系
     */
    class FixedBiomeSource : public BiomeSource
    {
    public:
        /**
         * @brief 构造函数
         * @param biome 固定返回的生物群系
         */
        explicit FixedBiomeSource(std::shared_ptr<Biome> biome)
            : m_biome(biome)
        {
        }

        std::shared_ptr<Biome> GetBiome(int x, int y, int z) const override
        {
            // 忽略坐标,总是返回同一个生物群系
            (void)x;
            (void)y;
            (void)z;
            return m_biome;
        }

    private:
        std::shared_ptr<Biome> m_biome; ///< 固定返回的生物群系
    };
} // namespace enigma::voxel
