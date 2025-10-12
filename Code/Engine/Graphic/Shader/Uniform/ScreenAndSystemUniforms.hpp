#pragma once

#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec3.hpp"

namespace enigma::graphic
{
    /**
     * @brief Screen and System Uniforms - 屏幕和系统数据
     *
     * Iris官方文档参考:
     * https://shaders.properties/current/reference/uniforms/system/
     *
     * 教学要点:
     * 1. 此结构体对应Iris "Screen and System" Uniform分类
     * 2. 存储在GPU StructuredBuffer，通过screenAndSystemBufferIndex访问
     * 3. 使用alignas确保与HLSL对齐要求一致
     * 4. 所有字段名称、类型、语义与Iris官方文档完全一致
     *
     * HLSL访问示例:
     * ```hlsl
     * StructuredBuffer<ScreenAndSystemUniforms> screenSystemBuffer =
     *     ResourceDescriptorHeap[screenAndSystemBufferIndex];
     * float height = screenSystemBuffer[0].viewHeight;
     * int frame = screenSystemBuffer[0].frameCounter;
     * ```
     *
     * @note alignas(4)用于int/float确保4字节对齐（HLSL标准对齐）
     * @note alignas(8)用于ivec2/vec2确保8字节对齐
     * @note alignas(16)用于ivec3/vec3确保16字节对齐
     */
#pragma warning(push)
#pragma warning(disable: 4324) // 结构体因alignas而填充 - 预期行为
    struct ScreenAndSystemUniforms
    {
        /**
         * @brief 视口高度（像素）
         * @type float
         * @iris viewHeight
         *
         * 教学要点: 游戏窗口的像素高度
         */
        alignas(4) float viewHeight;

        /**
         * @brief 视口宽度（像素）
         * @type float
         * @iris viewWidth
         *
         * 教学要点: 游戏窗口的像素宽度
         */
        alignas(4) float viewWidth;

        /**
         * @brief 屏幕宽高比
         * @type float
         * @iris aspectRatio
         *
         * 教学要点: aspectRatio = viewWidth / viewHeight
         */
        alignas(4) float aspectRatio;

        /**
         * @brief 屏幕亮度设置
         * @type float
         * @iris screenBrightness
         * @range [0,1]
         *
         * 教学要点: 视频设置中的屏幕亮度选项，0为最暗，1为最亮
         */
        alignas(4) float screenBrightness;

        /**
         * @brief 帧计数器
         * @type int
         * @iris frameCounter
         * @range [0, 720719]
         *
         * 教学要点:
         * - 从程序启动开始计数
         * - 达到720719后下一帧重置为0
         */
        alignas(4) int frameCounter;

        /**
         * @brief 上一帧的帧时间（秒）
         * @type float
         * @iris frameTime
         *
         * 教学要点:
         * - Delta Time，表示上一帧到再上一帧之间的时间间隔
         * - 60fps时约为0.0167秒
         */
        alignas(4) float frameTime;

        /**
         * @brief 运行时间累计（秒）
         * @type float
         * @iris frameTimeCounter
         * @range [0, 3600)
         *
         * 教学要点:
         * - 从程序启动开始累计
         * - 3600秒（1小时）后重置为0
         * - 暂停屏幕时不暂停计时
         */
        alignas(4) float frameTimeCounter;

        /**
         * @brief 显示器颜色空间
         * @type int
         * @iris currentColorSpace
         * @range 0, 1, 2, 3, 4
         * @tag Iris Exclusive
         *
         * 教学要点:
         * - 读取视频设置中的色彩空间选项
         * - 0: sRGB
         * - 1: DCI_P3
         * - 2: Display P3
         * - 3: REC2020
         * - 4: Adobe RGB
         */
        alignas(4) int currentColorSpace;

        /**
         * @brief 系统日期 (年, 月, 日)
         * @type ivec3
         * @iris currentDate
         * @tag Iris Exclusive
         *
         * 教学要点:
         * - 由操作系统报告的真实日期
         * - 格式: ivec3(year, month, day)
         * - 可用于季节性效果
         */
        alignas(16) IntVec3 currentDate;

        /**
         * @brief 系统时间 (时, 分, 秒)
         * @type ivec3
         * @iris currentTime
         * @tag Iris Exclusive
         *
         * 教学要点:
         * - 由操作系统报告的真实时间
         * - 格式: ivec3(hour, minute, second)
         * - 小时为24小时制（如下午3点为15）
         */
        alignas(16) IntVec3 currentTime;

        /**
         * @brief 年内时间统计 (年内已过秒数, 年内剩余秒数)
         * @type ivec2
         * @iris currentYearTime
         * @tag Iris Exclusive
         *
         * 教学要点:
         * - 由操作系统报告的年内时间统计
         * - 格式: ivec2(seconds_elapsed_in_year, seconds_remaining_in_year)
         * - 可用于年度周期效果
         */
        alignas(8) IntVec2 currentYearTime;

        /**
         * @brief 默认构造函数 - 初始化为合理默认值
         */
        ScreenAndSystemUniforms()
            : viewHeight(720.0f) // 默认720p高度
              , viewWidth(1280.0f) // 默认720p宽度
              , aspectRatio(16.0f / 9.0f) // 默认16:9
              , screenBrightness(1.0f) // 默认最亮
              , frameCounter(0)
              , frameTime(0.0167f) // 默认60fps
              , frameTimeCounter(0.0f)
              , currentColorSpace(0) // 默认sRGB
              , currentDate(IntVec3(2025, 10, 10)) // 默认日期
              , currentTime(IntVec3(12, 0, 0)) // 默认中午12点
              , currentYearTime(IntVec2(0, 31536000)) // 默认年初（365天 = 31536000秒）
        {
        }
    };
#pragma warning(pop)

    // 编译期验证: 检查结构体大小合理性（不超过256字节）
    static_assert(sizeof(ScreenAndSystemUniforms) <= 256,
                  "ScreenAndSystemUniforms too large, consider optimization");
} // namespace enigma::graphic
