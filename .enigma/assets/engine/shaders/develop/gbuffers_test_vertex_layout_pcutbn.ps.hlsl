#include "../core/core.hlsl"
#include "../include/developer_uniforms.hlsl"
/**
 * @brief Pixel shader output structure - multi-RT output
 *
 * [TEMPORARY TEST] Temporary test configuration - output to colortex4-7
 * Normally it should be output to colortex0-3 (GBuffer standard configuration)
 */
struct PSOutput
{
    float4 color0 : SV_Target0;
};

/**
 * @brief pixel shader main function
 * @param input pixel input (PSInput from Common.hlsli)
 * @return PSOutput - multiple RT outputs
 *
 *Teaching points:
 * 1. Multiple RT output needs to define a structure, each member corresponds to an SV_Target
 * 2. The SV_Target index corresponds to the RTV array index bound to OMSetRenderTargets
 * 3. Current configuration: output to colortex4-7 (test purpose)
 * 4. Normal configuration: should output to colortex0-3 (GBuffer standard)
 */
PSOutput main(PSInput input)
{
    PSOutput output;

    // 采样CustomImage0纹理（使用线性采样器和UV坐标）
    // CustomImage槽位在CPU侧通过SetCustomImage(0, texture)设置
    float4 texColor = customImage0.Sample(sampler1, input.TexCoord);

    // [NEW] Multi-draw data independence test
    // Final color = Custom Buffer color (should be Red/Green/Blue for each cube)
    // If Ring Buffer isolation fails, all cubes will show Blue (last uploaded color)
    output.color0 = texColor * input.Color * modelColor;

    return output;
}
