/**
 * @file gbuffers_basic.ps.hlsl
 * @brief Iris 兼容 Fallback 着色器 - gbuffers_basic (像素着色器)
 * @date 2025-10-04
 *
 * 教学要点:
 * 1. 最基础的像素着色器 - 仅输出顶点颜色
 * 2. 无纹理采样、无光照计算、无特殊效果
 * 3. GBuffers 阶段 - 写入 colortex0 (主颜色缓冲)
 * 4. 延迟渲染架构 - 光照在后续 Deferred/Composite 阶段处理
 *
 * 输出目标:
 * - SV_Target0 (colortex0) - RGBA 颜色缓冲
 * - (可选) SV_Target1-7 - 其他 GBuffer 输出 (法线、深度等)
 *
 * Iris 注释支持:
 * - RENDERTARGETS: 0  - 只写入 colortex0
 * - 可通过 ShaderDirectiveParser 解析
 */

#include "../core/Common.hlsl"

/**
 * @brief 像素着色器输出结构体 - 多RT输出
 *
 * [TEMPORARY TEST] 临时测试配置 - 输出到colortex4-7
 * 正常情况下应该输出到colortex0-3 (GBuffer标准配置)
 */
struct PSOutput
{
    float4 color0 : SV_Target0; // colortex4 - 主颜色 (Albedo)
    float4 color1 : SV_Target1; // colortex5 - 预留
    float4 color2 : SV_Target2; // colortex6 - 预留
    float4 color3 : SV_Target3; // colortex7 - 预留
};

/**
 * @brief 像素着色器主函数
 * @param input 像素输入 (PSInput from Common.hlsli)
 * @return PSOutput - 多RT输出
 *
 * 教学要点:
 * 1. 多RT输出需要定义结构体，每个成员对应一个SV_Target
 * 2. SV_Target索引对应OMSetRenderTargets绑定的RTV数组索引
 * 3. 当前配置：输出到colortex4-7 (测试用途)
 * 4. 正常配置：应该输出到colortex0-3 (GBuffer标准)
 */
PSOutput main(PSInput input)
{
    PSOutput output;

    // [IMPORTANT] 主颜色输出到第一个RT (colortex4)
    output.color0 = input.Color;

    // [TEST] 其他RT输出测试数据
    output.color1 = float4(0.0, 0.0, 0.0, 1.0); // 黑色
    output.color2 = float4(0.0, 0.0, 0.0, 1.0); // 黑色
    output.color3 = float4(0.0, 0.0, 0.0, 1.0); // 黑色

    return output;
}

/**
 * Iris 注释示例:
 * RENDERTARGETS: 0
 *
 * 说明:
 * - 只写入 RT0 (colortex0)
 * - 不写入法线、深度等其他 GBuffer
 * - ShaderDirectiveParser 会解析此注释并配置 PSO
 */
