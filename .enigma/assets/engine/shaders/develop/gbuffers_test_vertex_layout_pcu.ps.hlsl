#include "../core/Common.hlsl"

struct VSOutput_PCU
{
    float4 Position : SV_POSITION; // Clipping space position
    float4 Color : COLOR0; // Vertex color (after unpacking)
    float2 TexCoord : TEXCOORD0; // Texture coordinates
};

struct PSOutput_PCU
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
PSOutput_PCU main(VSOutput_PCU input)
{
    PSOutput_PCU output;
    // Sample CustomImage0 texture (using linear sampler and UV coordinates)
    // The CustomImage slot is set on the CPU side through SetCustomImage(0, texture)
    float4 texColor = customImage0.Sample(sampler1, input.TexCoord);

    // [NEW] Multi-draw data independence test
    // Final color = Custom Buffer color (should be Red/Green/Blue for each cube)
    // If Ring Buffer isolation fails, all cubes will show Blue (last uploaded color)
    output.color0 = texColor * input.Color * modelColor;
    return output;
}
