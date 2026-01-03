#pragma once

#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec4.hpp"

namespace enigma::graphic
{
    /**
     * @brief World/Weather Uniforms - Weather and Time Data
     *
     * [Iris Reference]
     *   https://shaders.properties/current/reference/uniforms/world/
     *
     * [IMPORTANT] Celestial fields moved to CelestialUniforms (CelestialConstantBuffer.hpp)
     *   - sunPosition, moonPosition, shadowLightPosition -> CelestialUniforms
     *   - sunAngle (celestialAngle), shadowAngle -> CelestialUniforms
     *   - upPosition -> CelestialUniforms
     *
     * This struct now contains ONLY weather and time data:
     *   - moonPhase, rainStrength, wetness, thunderStrength
     *   - lightningBoltPosition, worldTime, worldDay
     *
     * HLSL Access:
     * ```hlsl
     * StructuredBuffer<WorldAndWeatherUniforms> worldWeatherBuffer =
     *     ResourceDescriptorHeap[worldAndWeatherBufferIndex];
     * float rainStr = worldWeatherBuffer[0].rainStrength;
     * ```
     */
#pragma warning(push)
#pragma warning(disable: 4324) // structure padding due to alignas - expected
    struct WorldAndWeatherUniforms
    {
        /**
         * @brief Moon phase
         * @type int
         * @iris moonPhase
         * @range [0,7]
         *
         * Values:
         * - 0: Full moon
         * - 1-3: Waning moon
         * - 4: New moon
         * - 5-7: Waxing moon
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
         * @brief Default constructor
         */
        WorldAndWeatherUniforms()
            : moonPhase(0) // Full moon
              , rainStrength(0.0f) // No rain
              , wetness(0.0f) // Dry
              , thunderStrength(0.0f) // No thunder
              , lightningBoltPosition(Vec4(0.0f, 0.0f, 0.0f, 0.0f)) // No lightning
              , worldTime(6000) // Noon
              , worldDay(0) // Day 0
        {
        }
    };
#pragma warning(pop)

    // 编译期验证: 检查结构体大小合理性（不超过256字节）
    static_assert(sizeof(WorldAndWeatherUniforms) <= 256,
                  "WorldAndWeatherUniforms too large, consider optimization");
} // namespace enigma::graphic
