#pragma once

#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec4.hpp"

namespace enigma::graphic
{
    /**
     * @brief World/Weather Uniforms - 世界和天气数据
     *
     * Iris官方文档参考:
     * https://shaders.properties/current/reference/uniforms/world/
     *
     * 教学要点:
     * 1. 此结构体对应Iris "World/Weather" Uniform分类
     * 2. 存储在GPU StructuredBuffer，通过worldAndWeatherBufferIndex访问
     * 3. 使用alignas确保与HLSL对齐要求一致
     * 4. 所有字段名称、类型、语义与Iris官方文档完全一致
     *
     * HLSL访问示例:
     * ```hlsl
     * StructuredBuffer<WorldAndWeatherUniforms> worldWeatherBuffer =
     *     ResourceDescriptorHeap[worldAndWeatherBufferIndex];
     * float3 sunPos = worldWeatherBuffer[0].sunPosition;
     * float rainStr = worldWeatherBuffer[0].rainStrength;
     * ```
     *
     * @note alignas(16)用于Vec3/Vec4确保16字节对齐（HLSL标准对齐）
     * @note alignas(4)用于int/float确保4字节对齐
     */
#pragma warning(push)
#pragma warning(disable: 4324) // 结构体因alignas而填充 - 预期行为
    struct WorldAndWeatherUniforms
    {
        /**
         * @brief 太阳位置（视图空间）
         * @type vec3
         * @iris sunPosition
         * @range 长度为100
         *
         * 教学要点:
         * - 视图空间中的太阳方向向量
         * - 长度固定为100（归一化向量 * 100）
         * - 类似upPosition的表示方式
         */
        alignas(16) Vec3 sunPosition;

        /**
         * @brief 月亮位置（视图空间）
         * @type vec3
         * @iris moonPosition
         * @range 长度为100
         *
         * 教学要点:
         * - 视图空间中的月亮方向向量
         * - 长度固定为100（归一化向量 * 100）
         */
        alignas(16) Vec3 moonPosition;

        /**
         * @brief 阴影光源位置（视图空间）
         * @type vec3
         * @iris shadowLightPosition
         * @range 长度为100
         *
         * 教学要点:
         * - 指向太阳或月亮（取决于哪个更高）
         * - 用于阴影计算的主光源方向
         * - 长度固定为100
         */
        alignas(16) Vec3 shadowLightPosition;

        /**
         * @brief 太阳角度
         * @type float
         * @iris sunAngle
         * @range [0,1]
         *
         * 教学要点:
         * - 太阳在完整昼夜循环中的角度
         * - 0.0 = 日出, 0.25 = 正午, 0.5 = 日落, 0.75 = 午夜
         */
        alignas(4) float sunAngle;

        /**
         * @brief 阴影角度
         * @type float
         * @iris shadowAngle
         * @range [0,0.5]
         *
         * 教学要点:
         * - 阴影光源（太阳或月亮）的角度
         * - 范围0-0.5，对应半天周期
         */
        alignas(4) float shadowAngle;

        /**
         * @brief 月相
         * @type int
         * @iris moonPhase
         * @range [0,7]
         *
         * 教学要点:
         * - 0: 满月
         * - 1-3: 渐亏月
         * - 4: 新月
         * - 5-7: 渐盈月
         */
        alignas(4) int moonPhase;

        /**
         * @brief 雨强度
         * @type float
         * @iris rainStrength
         * @range [0,1]
         *
         * 教学要点:
         * - 当前降雨强度
         * - 0=无雨，1=最大雨量
         */
        alignas(4) float rainStrength;

        /**
         * @brief 湿度
         * @type float
         * @iris wetness
         * @range [0,1]
         *
         * 教学要点:
         * - rainStrength随时间平滑后的值
         * - 用于雨后湿润效果
         * - 雨停后逐渐降低
         */
        alignas(4) float wetness;

        /**
         * @brief 雷暴强度
         * @type float
         * @iris thunderStrength
         * @range [0,1]
         * @tag Iris Exclusive
         *
         * 教学要点:
         * - 当前雷暴强度
         * - Iris独有特性
         */
        alignas(4) float thunderStrength;

        /**
         * @brief 闪电位置
         * @type vec4
         * @iris lightningBoltPosition
         * @tag Iris Exclusive
         *
         * 教学要点:
         * - xyz: 闪电击中的世界坐标
         * - w: 闪电强度（0表示无闪电）
         * - Iris独有特性
         */
        alignas(16) Vec4 lightningBoltPosition;

        /**
         * @brief 游戏内时间
         * @type int
         * @iris worldTime
         * @range [0, 23999]
         *
         * 教学要点:
         * - Minecraft一天24000 ticks
         * - 0 = 早上6点, 6000 = 正午, 12000 = 傍晚6点, 18000 = 午夜
         * - 达到23999后下一tick重置为0
         */
        alignas(4) int worldTime;

        /**
         * @brief 游戏内天数
         * @type int
         * @iris worldDay
         *
         * 教学要点:
         * - 从世界创建开始的天数
         * - 每24000 ticks（一天）增加1
         */
        alignas(4) int worldDay;

        /**
         * @brief 默认构造函数 - 初始化为合理默认值
         */
        WorldAndWeatherUniforms()
            : sunPosition(Vec3(0.0f, 100.0f, 0.0f)) // 默认正上方，长度100
              , moonPosition(Vec3(0.0f, -100.0f, 0.0f)) // 默认正下方，长度100
              , shadowLightPosition(Vec3(0.0f, 100.0f, 0.0f)) // 默认太阳方向
              , sunAngle(0.25f) // 默认正午
              , shadowAngle(0.25f) // 默认正午阴影角
              , moonPhase(0) // 默认满月
              , rainStrength(0.0f) // 默认无雨
              , wetness(0.0f) // 默认干燥
              , thunderStrength(0.0f) // 默认无雷暴
              , lightningBoltPosition(Vec4(0.0f, 0.0f, 0.0f, 0.0f)) // 默认无闪电
              , worldTime(6000) // 默认正午
              , worldDay(0) // 默认第0天
        {
        }
    };
#pragma warning(pop)

    // 编译期验证: 检查结构体大小合理性（不超过256字节）
    static_assert(sizeof(WorldAndWeatherUniforms) <= 256,
                  "WorldAndWeatherUniforms too large, consider optimization");
} // namespace enigma::graphic
