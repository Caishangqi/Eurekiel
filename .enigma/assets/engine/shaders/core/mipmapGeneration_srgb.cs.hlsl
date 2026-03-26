/**
 * @file mipmapGeneration_srgb.cs.hlsl
 * @brief sRGB-correct mipmap downsampling compute shader
 *
 * Converts sRGB texels to linear space before averaging, then back to sRGB.
 * Prevents distant textures from appearing darker/desaturated due to
 * incorrect gamma-space averaging.
 *
 * Uses RWTexture2D for both source and destination (UAV-safe).
 * Dispatched once per mip level by MipmapGenerator.
 */

#include "../include/mipgen_uniforms.hlsl"

// sRGB <-> Linear conversion (gamma 2.2 approximation)
float3 SRGBToLinear(float3 c) { return pow(max(c, 0.0), 2.2); }
float3 LinearToSRGB(float3 c) { return pow(max(c, 0.0), 1.0 / 2.2); }

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= g_dstWidth || DTid.y >= g_dstHeight)
        return;

    RWTexture2D<float4> srcMip = ResourceDescriptorHeap[g_srcTextureIndex];
    RWTexture2D<float4> dstMip = ResourceDescriptorHeap[g_dstMipUavIndex];

    // Load 2x2 source texels
    int2 srcCoord = DTid.xy * 2;
    float4 s00 = srcMip[srcCoord];
    float4 s10 = srcMip[srcCoord + int2(1, 0)];
    float4 s01 = srcMip[srcCoord + int2(0, 1)];
    float4 s11 = srcMip[srcCoord + int2(1, 1)];

    // Convert RGB to linear space, average, convert back
    float3 linear00 = SRGBToLinear(s00.rgb);
    float3 linear10 = SRGBToLinear(s10.rgb);
    float3 linear01 = SRGBToLinear(s01.rgb);
    float3 linear11 = SRGBToLinear(s11.rgb);

    float3 avgLinear = (linear00 + linear10 + linear01 + linear11) * 0.25;
    float  avgAlpha  = (s00.a + s10.a + s01.a + s11.a) * 0.25;

    dstMip[DTid.xy] = float4(LinearToSRGB(avgLinear), avgAlpha);
}
