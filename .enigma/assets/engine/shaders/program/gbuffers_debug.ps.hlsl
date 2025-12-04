#include "../core/Common.hlsl"

struct PSOutput
{
    float4 color0 : SV_Target0; // colortex 0
};

PSOutput main(PSInput input)
{
    PSOutput output;

    // 采样CustomImage0纹理（使用线性采样器和UV坐标）
    // CustomImage槽位在CPU侧通过SetCustomImage(0, texture)设置
    float4 texColor = customImage0.Sample(wrapSampler, input.TexCoord);

    // 混合纹理颜色与顶点颜色和模型颜色
    // 最终颜色 = 纹理颜色 × 顶点颜色 × 模型颜色
    output.color0 = texColor * input.Color * modelColor;
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
