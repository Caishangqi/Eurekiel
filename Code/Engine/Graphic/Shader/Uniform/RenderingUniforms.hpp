#pragma once

#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec4.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec4.hpp"
#include <cstdint>

namespace enigma::graphic
{
#ifdef near
#undef near
#endif

#ifdef far
#undef far
#endif

    /**
     * @brief Rendering Uniforms - 渲染相关数据
     *
     * Iris官方文档参考:
     * https://shaders.properties/current/reference/uniforms/rendering/
     *
     * 教学要点:
     * 1. 此结构体对应Iris "Rendering" Uniform分类
     * 2. 存储在GPU StructuredBuffer，通过renderingBufferIndex访问
     * 3. 包含裁剪面、渲染阶段、Alpha测试、混合模式、雾参数等
     * 4. ⚠️ 所有字段名称严格遵循Iris官方规范（无"Plane"后缀）
     *
     * HLSL访问示例:
     * ```hlsl
     * StructuredBuffer<RenderingUniforms> renderingBuffer =
     *     ResourceDescriptorHeap[renderingBufferIndex];
     * float nearPlane = renderingBuffer[0].near;
     * float alphaTestRef = renderingBuffer[0].alphaTestRef;
     * vec3 fogColor = renderingBuffer[0].fogColor;
     * ```
     *
     * @note alignas(16)用于Vec3/Vec4确保16字节对齐
     * @note alignas(8)用于IntVec2确保8字节对齐
     */
#pragma warning(push)
#pragma warning(disable: 4324) // 结构体因alignas而填充 - 预期行为
    struct RenderingUniforms
    {
        /**
         * @brief 近裁剪面距离
         * @type float
         * @iris near
         * @value 0.05
         *
         * 教学要点:
         * - 相机近裁剪面的距离
         * - 除非被模组修改，通常固定为 0.05
         */
        float near;

        /**
         * @brief 当前渲染距离（方块）
         * @type float
         * @iris far
         * @range (0, +∞)
         *
         * 教学要点:
         * - 当前渲染距离设置（方块数）
         * - ⚠️ 注意：这不是远裁剪面距离（实际约为 far * 4.0）
         */
        float far;

        /**
         * @brief Alpha测试参考值
         * @type float
         * @iris alphaTestRef
         * @range [0,1]
         *
         * 教学要点:
         * - Alpha < alphaTestRef的像素被丢弃
         * - 用于镂空纹理（如树叶、玻璃）
         * - 通常为 0.1，但可通过 alphaTest 指令修改
         * - 推荐使用此 uniform 而非硬编码值
         *
         * HLSL使用示例:
         * ```hlsl
         * if (albedoOut.a < alphaTestRef) discard;
         * ```
         */
        float alphaTestRef;

        /**
         * @brief 区块偏移（模型空间）
         * @type vec3
         * @iris chunkOffset
         *
         * 教学要点:
         * - 当前渲染地块的模型空间偏移
         * - 用于大世界渲染的精度优化
         * - 与 vaPosition 结合获取模型空间位置:
         *   ```
         *   vec3 model_pos = vaPosition + chunkOffset;
         *   ```
         * - 非地形渲染时为全 0
         * - 兼容性配置文件下为全 0
         */
        alignas(16) Vec3 chunkOffset;

        /**
         * @brief 实体染色颜色
         * @type vec4
         * @iris entityColor
         * @range [0,1]
         *
         * 教学要点:
         * - rgb: 染色颜色
         * - a: 混合因子
         * - 应用方式:
         *   ```
         *   color.rgb = mix(color.rgb, entityColor.rgb, entityColor.a);
         *   ```
         */
        alignas(16) Vec4 entityColor;

        /**
         * @brief Alpha混合函数
         * @type ivec4
         * @iris blendFunc
         *
         * 教学要点:
         * - 当前程序的Alpha混合函数乘数（由 blend.<program> 指令定义）
         * - x: 源RGB混合因子
         * - y: 目标RGB混合因子
         * - z: 源Alpha混合因子
         * - w: 目标Alpha混合因子
         *
         * 取值含义（LWJGL常量）:
         * - GL_ZERO = 0
         * - GL_ONE = 1
         * - GL_SRC_COLOR = 768
         * - GL_ONE_MINUS_SRC_COLOR = 769
         * - GL_SRC_ALPHA = 770
         * - GL_ONE_MINUS_SRC_ALPHA = 771
         * - GL_DST_ALPHA = 772
         * - GL_ONE_MINUS_DST_ALPHA = 773
         * - GL_DST_COLOR = 774
         * - GL_ONE_MINUS_DST_COLOR = 775
         * - GL_SRC_ALPHA_SATURATE = 776
         */
        alignas(16) IntVec4 blendFunc;

        /**
         * @brief 纹理图集大小
         * @type ivec2
         * @iris atlasSize
         *
         * 教学要点:
         * - 纹理图集的尺寸（像素）
         * - 仅在纹理图集绑定时有值
         * - 未绑定时返回 (0, 0)
         */
        alignas(8) IntVec2 atlasSize;

        /**
         * @brief 当前渲染阶段
         * @type int
         * @iris renderStage
         *
         * 教学要点:
         * - 当前几何体的"渲染阶段"
         * - 比 gbuffer 程序更细粒度地标识几何体类型
         * - 应与 Iris 定义的预处理器宏比较（如 MC_RENDER_STAGE_TERRAIN_SOLID）
         */
        int32_t renderStage;

        /**
         * @brief 地平线雾颜色
         * @type vec3
         * @iris fogColor
         * @range [0,1]
         *
         * 教学要点:
         * - 原版游戏用于天空渲染和雾效的地平线雾颜色
         * - 可能受生物群系、时间、视角影响
         */
        alignas(16) Vec3 fogColor;

        /**
         * @brief 上层天空颜色
         * @type vec3
         * @iris skyColor
         * @range [0,1]
         *
         * 教学要点:
         * - 原版游戏用于天空渲染的上层天空颜色
         * - 可能受生物群系、时间影响
         * - 不受视角影响（与 fogColor 的区别）
         */
        alignas(16) Vec3 skyColor;

        /**
         * @brief 相对雾密度
         * @type float
         * @iris fogDensity
         * @range [0.0, 1.0]
         *
         * 教学要点:
         * - 原版雾的相对密度
         * - 基于当前生物群系、天气、流体（水/熔岩/积雪）等
         * - 0.0 = 最低密度，1.0 = 最高密度
         */
        float fogDensity;

        /**
         * @brief 雾起始距离（方块）
         * @type float
         * @iris fogStart
         * @range [0, +∞)
         *
         * 教学要点:
         * - 原版雾的起始距离
         * - 基于当前生物群系、天气、流体等
         */
        float fogStart;

        /**
         * @brief 雾结束距离（方块）
         * @type float
         * @iris fogEnd
         * @range [0, +∞)
         *
         * 教学要点:
         * - 原版雾的结束距离
         * - 基于当前生物群系、天气、流体等
         */
        float fogEnd;

        /**
         * @brief 雾模式
         * @type int
         * @iris fogMode
         * @range 2048, 2049, 9729
         *
         * 教学要点:
         * - 原版雾的类型（基于生物群系、天气、流体等）
         * - 决定雾强度随距离的衰减函数
         *
         * 取值含义（LWJGL常量）:
         * - GL_EXP = 2048: 指数雾
         * - GL_EXP2 = 2049: 平方指数雾
         * - GL_LINEAR = 9729: 线性雾
         */
        int32_t fogMode;

        /**
         * @brief 雾形状
         * @type int
         * @iris fogShape
         * @range 0, 1
         *
         * 教学要点:
         * - 原版雾的形状（基于生物群系、天气、流体、洞穴等）
         *
         * 取值含义:
         * - 0: 球形雾
         * - 1: 柱形雾
         */
        int32_t fogShape;


        /**
         * @brief 默认构造函数 - 初始化为合理默认值
         */
        RenderingUniforms()
            : near(0.05f)
              , far(256.0f) // 默认16个区块渲染距离
              , alphaTestRef(0.1f) // 默认Alpha阈值
              , chunkOffset(Vec3::ZERO)
              , entityColor(Vec4(1.0f, 1.0f, 1.0f, 0.0f)) // 默认白色，无混合
              , blendFunc(IntVec4(1, 0, 1, 0)) // 默认不混合 (GL_ONE, GL_ZERO, GL_ONE, GL_ZERO)
              , atlasSize(IntVec2::ZERO)
              , renderStage(0)
              , fogColor(Vec3(0.7f, 0.8f, 0.9f))
              , skyColor(Vec3(0.4f, 0.6f, 1.0f))
              , fogDensity(0.1f)
              , fogStart(32.0f)
              , fogEnd(256.0f)
              , fogMode(9729) // 默认线性雾 (GL_LINEAR)
              , fogShape(0) // 默认球形雾
        {
        }
    };
#pragma warning(pop)

    // 编译期验证: 检查结构体大小合理性（不超过256字节）
    static_assert(sizeof(RenderingUniforms) <= 256,
                  "RenderingUniforms too large, consider optimization");
} // namespace enigma::graphic
