/**
 * @file mipmapGeneration_alphaWeightedSrgb.cs.hlsl
 * @brief Alpha-weighted + sRGB-correct mipmap downsampling compute shader
 *
 * Combines both techniques for highest quality mipmap generation:
 * 1. sRGB -> Linear conversion before averaging (correct color math)
 * 2. Alpha-weighted RGB averaging (prevents transparent pixel bleeding)
 * 3. Linear -> sRGB conversion after averaging
 *
 * Best for cutout textures (leaves, grass) authored in sRGB color space.
 *
 * Uses Texture2D for source reads and RWTexture2D for destination writes.
 * Source texels are fetched with Load() from the explicit source mip level.
 * Dispatched once per mip level by MipmapGenerator.
 */

#include "../include/mipgen_uniforms.hlsl"

float3 SRGBToLinear(float3 c) { return pow(max(c, 0.0), 2.2); }
float3 LinearToSRGB(float3 c) { return pow(max(c, 0.0), 1.0 / 2.2); }

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= g_dstWidth || DTid.y >= g_dstHeight)
        return;

    Texture2D srcTexture = ResourceDescriptorHeap[g_srcTextureIndex];
    RWTexture2D<float4> dstMip = ResourceDescriptorHeap[g_dstMipUavIndex];

    // Load 2x2 source texels
    int2 srcCoord = DTid.xy * 2;
    float4 s00 = srcTexture.Load(int3(srcCoord, g_srcMipLevel));
    float4 s10 = srcTexture.Load(int3(srcCoord + int2(1, 0), g_srcMipLevel));
    float4 s01 = srcTexture.Load(int3(srcCoord + int2(0, 1), g_srcMipLevel));
    float4 s11 = srcTexture.Load(int3(srcCoord + int2(1, 1), g_srcMipLevel));

    // Convert RGB to linear space
    float3 lin00 = SRGBToLinear(s00.rgb);
    float3 lin10 = SRGBToLinear(s10.rgb);
    float3 lin01 = SRGBToLinear(s01.rgb);
    float3 lin11 = SRGBToLinear(s11.rgb);

    // Alpha-weighted average in linear space
    float totalAlpha = s00.a + s10.a + s01.a + s11.a;
    float3 weightedLinear = lin00 * s00.a + lin10 * s10.a
                          + lin01 * s01.a + lin11 * s11.a;

    float3 avgLinear = (totalAlpha > 0.001) ? weightedLinear / totalAlpha : float3(0, 0, 0);
    float  avgAlpha  = totalAlpha * 0.25;

    // Convert back to sRGB
    dstMip[DTid.xy] = float4(LinearToSRGB(avgLinear), avgAlpha);
}
