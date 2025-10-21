/**
 * @file RenderState.hpp
 * @brief 渲染状态枚举定义 - BlendMode和DepthMode
 *
 * 教学重点:
 * 1. DirectX 12中渲染状态是PSO (Pipeline State Object) 的一部分
 * 2. 理解不同混合模式的应用场景
 * 3. 理解深度测试和深度写入的配置
 * 4. 学习现代图形API的静态状态管理
 */

#pragma once

#include <cstdint>

namespace enigma::graphic
{
    /**
     * @brief 混合模式枚举
     * @details
     * 定义了常用的颜色混合模式，用于配置PSO的混合状态。
     * DirectX 12中混合状态是静态的，需要在创建PSO时指定。
     *
     * 教学要点:
     * - Opaque: 不透明渲染，最常用的模式（天空盒、实心物体）
     * - Alpha: 标准Alpha混合，用于半透明物体（玻璃、粒子效果）
     * - Additive: 加法混合，用于发光效果（光晕、火焰）
     * - Multiply: 乘法混合，用于阴影和暗化效果
     * - Premultiplied: 预乘Alpha，用于正确的半透明混合
     */
    enum class BlendMode : uint8_t
    {
        /**
         * @brief 不透明模式 - 无混合
         * @details 完全不透明，直接覆盖目标颜色
         *
         * DirectX 12配置:
         * - BlendEnable = FALSE
         *
         * 应用场景:
         * - 所有实心物体（地形、建筑、角色）
         * - 天空盒
         * - 延迟渲染的G-Buffer Pass
         */
        Opaque,

        /**
         * @brief 标准Alpha混合
         * @details 基于源Alpha值的线性插值混合
         *
         * DirectX 12配置:
         * - BlendEnable = TRUE
         * - SrcBlend = D3D12_BLEND_SRC_ALPHA
         * - DestBlend = D3D12_BLEND_INV_SRC_ALPHA
         * - BlendOp = D3D12_BLEND_OP_ADD
         *
         * 混合公式: FinalColor = SrcColor * SrcAlpha + DstColor * (1 - SrcAlpha)
         *
         * 应用场景:
         * - 半透明物体（玻璃、水面）
         * - UI元素
         * - 粒子效果（烟雾、雾）
         */
        Alpha,

        /**
         * @brief 加法混合
         * @details 源和目标颜色相加，用于发光效果
         *
         * DirectX 12配置:
         * - BlendEnable = TRUE
         * - SrcBlend = D3D12_BLEND_ONE
         * - DestBlend = D3D12_BLEND_ONE
         * - BlendOp = D3D12_BLEND_OP_ADD
         *
         * 混合公式: FinalColor = SrcColor + DstColor
         *
         * 应用场景:
         * - 发光效果（光晕、Bloom）
         * - 火焰、爆炸
         * - 粒子累积效果
         *
         * 注意: 颜色值会叠加，可能导致过曝
         */
        Additive,

        /**
         * @brief 乘法混合
         * @details 源和目标颜色相乘，用于暗化效果
         *
         * DirectX 12配置:
         * - BlendEnable = TRUE
         * - SrcBlend = D3D12_BLEND_DEST_COLOR
         * - DestBlend = D3D12_BLEND_ZERO
         * - BlendOp = D3D12_BLEND_OP_ADD
         *
         * 混合公式: FinalColor = SrcColor * DstColor
         *
         * 应用场景:
         * - 阴影贴图混合
         * - 光照衰减
         * - 颜色滤镜
         */
        Multiply,

        /**
         * @brief 预乘Alpha混合
         * @details 假设源颜色已预乘Alpha，用于正确的半透明混合
         *
         * DirectX 12配置:
         * - BlendEnable = TRUE
         * - SrcBlend = D3D12_BLEND_ONE
         * - DestBlend = D3D12_BLEND_INV_SRC_ALPHA
         * - BlendOp = D3D12_BLEND_OP_ADD
         *
         * 混合公式: FinalColor = SrcColor + DstColor * (1 - SrcAlpha)
         * 注意: SrcColor已预乘Alpha，即 SrcColor = OriginalColor * SrcAlpha
         *
         * 应用场景:
         * - 图像库的预乘Alpha纹理
         * - UI框架的正确混合
         * - 避免黑边问题
         */
        Premultiplied,

        /**
         * @brief 非预乘Alpha混合（同Alpha）
         * @details 源颜色未预乘Alpha，与Alpha模式相同
         *
         * 应用场景:
         * - 显式标记非预乘纹理
         * - 与Alpha模式功能相同
         */
        NonPremultiplied,

        /**
         * @brief 禁用混合
         * @details 显式禁用混合，与Opaque功能相同
         *
         * 应用场景:
         * - 显式标记不需要混合的Pass
         */
        Disabled
    };

    /**
     * @brief 深度模式枚举
     * @details
     * 定义了深度测试和深度写入的配置模式。
     * DirectX 12中深度状态是PSO的一部分，需要在创建时指定。
     *
     * 教学要点:
     * - 深度测试: 决定片段是否通过深度检查
     * - 深度写入: 决定是否更新深度缓冲区
     * - 比较函数: 决定深度比较的逻辑（Less、LessEqual等）
     */
    enum class DepthMode : uint8_t
    {
        /**
         * @brief 启用深度测试和写入（LessEqual）
         * @details 完整的深度功能，最常用的模式
         *
         * DirectX 12配置:
         * - DepthEnable = TRUE
         * - DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL
         * - DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL
         *
         * 应用场景:
         * - 所有实心物体的正常渲染
         * - 延迟渲染的G-Buffer Pass
         * - 前向渲染的不透明物体
         *
         * 教学要点: 默认深度模式，确保正确的遮挡关系
         */
        Enabled,

        /**
         * @brief 只读深度测试（测试但不写入）
         * @details 测试深度但不更新深度缓冲区
         *
         * DirectX 12配置:
         * - DepthEnable = TRUE
         * - DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO
         * - DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL
         *
         * 应用场景:
         * - 半透明物体渲染（避免遮挡后续半透明物体）
         * - 粒子效果（需要正确排序但不阻挡）
         * - 天空盒（在最远处，不写入深度）
         *
         * 教学要点: 保持深度缓冲区不变，允许后续绘制
         */
        ReadOnly,

        /**
         * @brief 只写深度（写入但不测试）
         * @details 不测试深度但更新深度缓冲区
         *
         * DirectX 12配置:
         * - DepthEnable = TRUE
         * - DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL
         * - DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS
         *
         * 应用场景:
         * - 初始化深度缓冲区
         * - 特殊效果（强制写入深度）
         *
         * 注意: 很少使用，大多数情况应使用Enabled或ReadOnly
         */
        WriteOnly,

        /**
         * @brief 完全禁用深度
         * @details 不测试也不写入深度
         *
         * DirectX 12配置:
         * - DepthEnable = FALSE
         * - DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO
         *
         * 应用场景:
         * - 全屏后处理Pass
         * - UI渲染
         * - Debug绘制（覆盖在所有物体之上）
         *
         * 教学要点: 完全忽略深度，按绘制顺序叠加
         */
        Disabled,

        // ==================== 比较函数变体 ====================
        // 以下模式提供不同的深度比较函数，适用于特殊渲染需求

        /**
         * @brief 小于等于比较（默认）
         * @details 等同于Enabled，显式标记比较函数
         *
         * DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL
         *
         * 应用场景: 标准深度测试
         */
        LessEqual,

        /**
         * @brief 严格小于比较
         * @details 只有严格小于（更近）才通过
         *
         * DepthFunc = D3D12_COMPARISON_FUNC_LESS
         *
         * 应用场景:
         * - 避免Z-Fighting（深度冲突）
         * - 特殊的深度剔除需求
         */
        Less,

        /**
         * @brief 大于等于比较（反向深度）
         * @details 反向深度缓冲区（远近颠倒）
         *
         * DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL
         *
         * 应用场景:
         * - 反向Z技术（提高深度精度）
         * - 特殊的渲染管线
         *
         * 教学要点: 现代引擎常用反向Z提高精度
         */
        GreaterEqual,

        /**
         * @brief 严格大于比较
         * @details 只有严格大于才通过
         *
         * DepthFunc = D3D12_COMPARISON_FUNC_GREATER
         */
        Greater,

        /**
         * @brief 相等比较
         * @details 只有深度完全相等才通过
         *
         * DepthFunc = D3D12_COMPARISON_FUNC_EQUAL
         *
         * 应用场景:
         * - 特殊的深度标记
         * - 精确深度匹配
         */
        Equal,

        /**
         * @brief 总是通过（同WriteOnly）
         * @details 深度测试总是通过，但仍写入深度
         *
         * DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS
         */
        Always
    };
} // namespace enigma::graphic
