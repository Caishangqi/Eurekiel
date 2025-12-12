#include "../core/Common.hlsl"

struct PSOutput
{
    float4 color0 : SV_Target0; // colortex 0
};

PSOutput main(PSInput input)
{
    PSOutput output;

    // Sample CustomImage0 texture (using linear sampler and UV coordinates)
    // The CustomImage slot is set on the CPU side through SetCustomImage(0, texture)
    float4 texColor = customImage0.Sample(wrapSampler, input.TexCoord);

    // Blend texture color with vertex color and model color
    // Final color = texture color × vertex color × model color
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
