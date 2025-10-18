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

#include "Common.hlsl"

/**
 * @brief 像素着色器主函数
 * @param input 像素输入 (PSInput from Common.hlsli)
 * @return float4 - 最终颜色 (RGBA)
 *
 * 教学要点:
 * 1. 直接返回插值后的顶点颜色
 * 2. 无需纹理采样 (gbuffers_basic 不使用纹理)
 * 3. 无光照计算 (延迟渲染在 Deferred 阶段处理)
 * 4. KISS 原则 - 最简单的像素着色器实现
 */
float4 main(PSInput input) : SV_Target0
{
    // 直接返回顶点颜色
    // - 颜色已经在顶点着色器中从 uint 解包为 float4
    // - 硬件自动对颜色进行插值
    // - 无需任何额外计算
    return input.Color;
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
