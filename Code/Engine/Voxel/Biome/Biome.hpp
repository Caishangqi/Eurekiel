#pragma once
#include <string>
#include <memory>
#include <vector>

namespace enigma::voxel
{
    /**
     * @brief 生物群系定义 - Biome Definition
     *
     * 架构参考: Minecraft 1.18+ Biome System
     *
     * 核心职责:
     *   定义一个生物群系的所有属性,包括气候参数、视觉效果、表面规则等
     *
     * Minecraft 1.18+ Biome系统关键概念:
     *   1. Climate Parameters (气候参数):
     *      - 5维参数空间: Temperature, Humidity, Continentalness, Erosion, Weirdness
     *      - 用于MultiNoiseBiomeSource的5D查找和匹配
     *      - 每个参数范围[-1.0, 1.0], 内部量化为long (x10000)
     *
     *   2. Surface Rules (表面规则):
     *      - 控制生物群系表面方块的生成规则
     *      - topBlock: 表面方块(草、沙、雪等)
     *      - fillerBlock: 填充方块(泥土、沙石等)
     *      - underwaterBlock: 水下方块(沙砾、粘土等)
     *
     *   3. Visual Effects (视觉效果):
     *      - 天空颜色、雾颜色、水颜色等
     *      - 草和树叶的渐变色
     *      - 粒子效果和环境音效
     *
     *   4. Spawning & Features (生成与特征):
     *      - 生物生成规则(未来实现)
     *      - 地形特征(树木、矿石等, 未来实现)
     *
     * 数据流:
     *   NoiseRouter (采样气候参数) -> MultiNoiseBiomeSource (5D匹配)
     *       -> Biome (提供表面规则和视觉效果) -> TerrainGenerator (应用规则)
     *
     * 参考: net.minecraft.world.level.biome.Biome (Minecraft 1.18+)
     */
    class Biome
    {
    public:
        /**
         * @brief 气候设置 - Climate Settings
         *
         * 定义生物群系在5D气候参数空间中的"理想位置"
         * MultiNoiseBiomeSource使用这些参数进行5D最近邻查找
         *
         * 技术细节:
         *   - Minecraft内部将浮点值量化为long类型(乘以10000)
         *   - 查找算法计算当前位置到所有已注册Biome的气候参数的欧几里得距离
         *   - 选择距离最小的Biome作为该位置的生物群系
         *
         * 参数说明:
         *   - temperature: 温度 [-1.0, 1.0], -1=冰冻, 0=温带, 1=炎热
         *   - humidity: 湿度 [-1.0, 1.0], -1=干燥, 0=适中, 1=潮湿
         *   - continentalness: 大陆度 [-1.0, 1.0], -1=深海, 0=海岸, 1=内陆
         *   - erosion: 侵蚀度 [-1.0, 1.0], -1=平坦, 0=正常, 1=崎岖
         *   - weirdness: 怪异度 [-1.0, 1.0], 控制地形的"奇特性"(峰谷、悬崖等)
         *
         * 注意: depth参数(第6维)在Minecraft 1.18+中已弃用,改为由地形塑形独立控制
         */
        struct ClimateSettings
        {
            float temperature; ///< 温度参数 [-1.0, 1.0]
            float humidity; ///< 湿度参数 [-1.0, 1.0]
            float continentalness; ///< 大陆度参数 [-1.0, 1.0]
            float erosion; ///< 侵蚀度参数 [-1.0, 1.0]
            float weirdness; ///< 怪异度参数 [-1.0, 1.0]

            /**
             * @brief 构造函数 - 初始化气候参数
             */
            ClimateSettings(float temp = 0.0f, float humid = 0.0f,
                            float cont = 0.0f, float ero   = 0.0f, float weird = 0.0f)
                : temperature(temp), humidity(humid), continentalness(cont)
                  , erosion(ero), weirdness(weird)
            {
            }

            /**
             * @brief 计算到另一个气候参数点的欧几里得距离(平方)
             *
             * 用于MultiNoiseBiomeSource的最近邻查找
             * 返回平方距离以避免开平方根运算(性能优化)
             *
             * @param other 另一个气候参数点
             * @return 距离的平方
             */
            float DistanceSquared(const ClimateSettings& other) const
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
         * @brief 表面规则 - Surface Rules
         *
         * 定义生物群系的表面方块生成规则
         * 在TerrainGenerator的ApplySurfaceRules阶段应用
         *
         * 工作原理:
         *   1. 生成器遍历每个XZ列,从上到下扫描
         *   2. 找到第一个非空气方块(表面)
         *   3. 表面方块替换为topBlockId
         *   4. 表面下N层替换为fillerBlockId
         *   5. 水下方块(z < seaLevel)替换为underwaterBlockId
         *
         * 示例:
         *   - 草原(Plains): top=grass, filler=dirt, underwater=gravel
         *   - 沙漠(Desert): top=sand, filler=sandstone, underwater=sand
         *   - 雪原(Tundra): top=snow, filler=dirt, underwater=ice
         */
        struct SurfaceRules
        {
            int topBlockId; ///< 表面方块ID (草、沙、雪等)
            int fillerBlockId; ///< 填充方块ID (泥土、沙石等)
            int underwaterBlockId; ///< 水下方块ID (沙砾、粘土等)
            int fillerDepth; ///< 填充层深度 (默认4层)

            /**
             * @brief 构造函数 - 初始化表面规则
             */
            SurfaceRules(int top = 0, int filler = 0, int underwater = 0, int depth = 4)
                : topBlockId(top), fillerBlockId(filler)
                  , underwaterBlockId(underwater), fillerDepth(depth)
            {
            }
        };

        /**
         * @brief 视觉效果设置 - Visual Effects (未来扩展)
         *
         * 控制生物群系的视觉表现
         * 当前为占位结构,未来可扩展为:
         *   - 天空颜色、雾颜色、水颜色
         *   - 草和树叶的渐变色
         *   - 粒子效果(飘雪、孢子等)
         *   - 环境音效
         */
        struct VisualEffects
        {
            // 未来扩展:
            // uint32_t skyColor;
            // uint32_t fogColor;
            // uint32_t waterColor;
            // GrassColorModifier grassColorModifier;
            // AmbientParticleSettings ambientParticle;
        };

    public:
        /**
         * @brief 构造函数 - 创建生物群系
         *
         * @param name 生物群系注册名称(例如: "plains", "desert")
         * @param climate 气候参数设置
         * @param surface 表面规则设置
         */
        Biome(const std::string&     name,
              const ClimateSettings& climate,
              const SurfaceRules&    surface)
            : m_name(name)
              , m_climate(climate)
              , m_surface(surface)
        {
        }

        /**
         * @brief 默认构造函数 - 创建空生物群系
         */
        Biome()
            : m_name("unknown")
              , m_climate()
              , m_surface()
        {
        }

        // Getters
        const std::string&     GetName() const { return m_name; }
        const ClimateSettings& GetClimateSettings() const { return m_climate; }
        const SurfaceRules&    GetSurfaceRules() const { return m_surface; }
        const VisualEffects&   GetVisualEffects() const { return m_effects; }

        /**
         * @brief 获取降水类型
         *
         * 根据温度参数判断降水形式:
         *   - temperature < -0.15: 降雪
         *   - temperature >= -0.15: 降雨
         *   - 干燥生物群系(humidity < -0.5): 无降水
         *
         * @return 0=无, 1=降雨, 2=降雪
         */
        int GetPrecipitationType() const
        {
            if (m_climate.humidity < -0.5f) return 0; // 干燥,无降水
            if (m_climate.temperature < -0.15f) return 2; // 降雪
            return 1; // 降雨
        }

        /**
         * @brief 判断是否为海洋生物群系
         *
         * 基于大陆度参数判断:
         *   - continentalness < -0.4: 海洋
         *   - continentalness >= -0.4: 陆地
         */
        bool IsOcean() const
        {
            return m_climate.continentalness < -0.4f;
        }

        /**
         * @brief 判断是否为山地生物群系
         *
         * 基于侵蚀度和大陆度判断:
         *   - 陆地区域(continentalness > 0)
         *   - 崎岖地形(erosion > 0.3)
         */
        bool IsMountainous() const
        {
            return m_climate.continentalness > 0.0f && m_climate.erosion > 0.3f;
        }

    private:
        std::string     m_name; ///< 生物群系注册名称
        ClimateSettings m_climate; ///< 气候参数设置
        SurfaceRules    m_surface; ///< 表面规则设置
        VisualEffects   m_effects; ///< 视觉效果设置(未来扩展)
    };
} // namespace enigma::voxel
