#pragma once

#include <cstdint>

namespace enigma::graphic
{
    /**
     * @brief Biome/Dimension Uniforms - 生物群系和维度数据
     *
     * Iris官方文档参考:
     * https://shaders.properties/current/reference/uniforms/biome/
     *
     * 教学要点:
     * 1. 此结构体对应Iris "Biome and Dimension" Uniform分类
     * 2. 存储在GPU StructuredBuffer，通过biomeAndDimensionBufferIndex访问
     * 3. 包含生物群系属性（biome, temperature, rainfall）和维度属性（hasSkylight, cloudHeight等）
     * 4. 所有字段名称严格遵循Iris官方规范（使用下划线命名）
     *
     * HLSL访问示例:
     * ```hlsl
     * StructuredBuffer<BiomeAndDimensionUniforms> biomeDimBuffer =
     *     ResourceDescriptorHeap[biomeAndDimensionBufferIndex];
     * int currentBiome = biomeDimBuffer[0].biome;
     * float temp = biomeDimBuffer[0].temperature;
     * bool hasSky = biomeDimBuffer[0].hasSkylight;
     * ```
     *
     * @note 所有字段均严格基于 Iris 官方文档
     * @note dimension, fogColor, waterColor 等字段属于其他类别，不在此结构体中
     */
    struct BiomeAndDimensionUniforms
    {
        // ========================================================================
        // Biome Uniforms (OptiFine Custom Uniforms / Iris)
        // ========================================================================

        /**
         * @brief 生物群系ID
         * @type int
         * @iris biome
         * @tag OptiFine CU / Iris
         *
         * 教学要点:
         * - 玩家当前所在生物群系的唯一ID
         * - 可与预定义常量比较: BIOME_PLAINS, BIOME_RIVER, BIOME_DESERT, BIOME_SWAMP 等
         * - 详见 Minecraft Wiki 的生物群系列表
         */
        int32_t biome;

        /**
         * @brief 生物群系类别
         * @type int
         * @iris biome_category
         * @tag OptiFine CU / Iris
         *
         * 教学要点:
         * - 生物群系的大类分类
         * - 可与预定义常量比较: CAT_NONE, CAT_TAIGA, CAT_EXTREME_HILLS, CAT_JUNGLE,
         *   CAT_MESA, CAT_PLAINS, CAT_SAVANNA, CAT_ICY, CAT_THE_END, CAT_BEACH,
         *   CAT_FOREST, CAT_OCEAN, CAT_DESERT, CAT_RIVER, CAT_SWAMP, CAT_MUSHROOM, CAT_NETHER
         */
        int32_t biome_category;

        /**
         * @brief 降水类型
         * @type int
         * @iris biome_precipitation
         * @range 0, 1, 2
         * @tag OptiFine CU / Iris
         *
         * 取值含义:
         * - 0 (PPT_NONE): 无降水
         * - 1 (PPT_RAIN): 雨
         * - 2 (PPT_SNOW): 雪
         *
         * 教学要点: 当前生物群系的降水形式
         */
        int32_t biome_precipitation;

        /**
         * @brief 生物群系降雨量
         * @type float
         * @iris rainfall
         * @range [0,1]
         * @tag OptiFine CU / Iris
         *
         * 教学要点:
         * - 生物群系的降雨属性（Minecraft内部定义）
         * - 0 = 干旱，1 = 多雨
         * - 不依赖当前天气，是生物群系固有属性
         */
        float rainfall;

        /**
         * @brief 生物群系温度
         * @type float
         * @iris temperature
         * @range [-0.7, 2.0] (原版Minecraft)
         * @tag OptiFine CU / Iris
         *
         * 教学要点:
         * - Minecraft内部定义的温度属性
         * - 影响降水形式（雨/雪）
         * - 影响植被颜色
         * - 模组可能使用此范围外的值
         */
        float temperature;

        // ========================================================================
        // Dimension Uniforms (Iris Exclusive)
        // ========================================================================

        /**
         * @brief 环境光照等级
         * @type float
         * @iris ambientLight
         * @tag Iris Exclusive
         *
         * 教学要点:
         * - 当前维度的环境光属性
         * - 大致对应天空光照对光照值的影响程度
         * - 详见 Minecraft Wiki "ambient_light"
         */
        float ambientLight;

        /**
         * @brief 基岩层高度
         * @type int
         * @iris bedrockLevel
         * @tag Iris Exclusive
         *
         * 教学要点:
         * - 当前维度的基岩层地板Y坐标
         * - 详见 Minecraft Wiki "min_y"
         */
        int32_t bedrockLevel;

        /**
         * @brief 云层高度
         * @type float
         * @iris cloudHeight
         * @tag Iris Exclusive
         *
         * 教学要点:
         * - 原版云层的Y坐标（基于用户设置）
         * - 对于无云的维度，值为 NaN
         */
        float cloudHeight;

        /**
         * @brief 是否有天花板
         * @type bool (int)
         * @iris hasCeiling
         * @tag Iris Exclusive
         *
         * 教学要点:
         * - true: 当前维度有顶（如下界）
         * - false: 无顶（如主世界、末地）
         * - 详见 Minecraft Wiki "has_ceiling"
         */
        int32_t hasCeiling;

        /**
         * @brief 是否有天空光
         * @type bool (int)
         * @iris hasSkylight
         * @tag Iris Exclusive
         *
         * 教学要点:
         * - true: 有天空和天空光照（主世界、末地）
         * - false: 无天空光（下界）
         * - 详见 Minecraft Wiki "has_skylight"
         */
        int32_t hasSkylight;

        /**
         * @brief 世界高度限制
         * @type int
         * @iris heightLimit
         * @tag Iris Exclusive
         *
         * 教学要点:
         * - 当前维度最低方块和最高方块之间的高度差
         * - 详见 Minecraft Wiki "height"
         */
        int32_t heightLimit;

        /**
         * @brief 逻辑高度限制
         * @type int
         * @iris logicalHeightLimit
         * @tag Iris Exclusive
         *
         * 教学要点:
         * - 维度的逻辑高度
         * - 紫颂果可生长的最大高度
         * - 下界传送门可传送玩家的最大高度
         * - 详见 Minecraft Wiki "logical_height"
         */
        int32_t logicalHeightLimit;

        /**
         * @brief 默认构造函数 - 初始化为主世界平原生物群系的合理默认值
         */
        BiomeAndDimensionUniforms()
            : biome(1) // 默认平原生物群系
              , biome_category(1) // CAT_PLAINS
              , biome_precipitation(1) // PPT_RAIN (雨)
              , rainfall(0.4f) // 中等降雨量
              , temperature(0.8f) // 温和气候
              , ambientLight(0.0f) // 主世界环境光
              , bedrockLevel(-64) // 1.18+ 主世界基岩层Y=-64
              , cloudHeight(128.0f) // 原版云层高度
              , hasCeiling(0) // 主世界无天花板
              , hasSkylight(1) // 主世界有天空光
              , heightLimit(384) // 1.18+ 世界高度384
              , logicalHeightLimit(256) // 主世界逻辑高度256
        {
        }
    };

    // 编译期验证: 检查结构体大小合理性（不超过256字节）
    static_assert(sizeof(BiomeAndDimensionUniforms) <= 256,
                  "BiomeAndDimensionUniforms too large, consider optimization");
} // namespace enigma::graphic
