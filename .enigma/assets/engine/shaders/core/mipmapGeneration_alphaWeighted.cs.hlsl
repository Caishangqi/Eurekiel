/**
 * @file mipmapGeneration_alphaWeighted.cs.hlsl
 * @brief Alpha-weighted mipmap downsampling compute shader
 *
 * Prevents color bleeding from transparent pixels into opaque regions.
 * Premultiplies RGB by alpha before averaging, then divides by total alpha.
 * Solves white/gray fringe on cutout textures (leaves, grass, flowers).
 *
 * Uses RWTexture2D for both source and destination (UAV-safe).
 * Dispatched once per mip level by MipmapGenerator.
 */

#include "../include/mipgen_uniforms.hlsl"

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= g_dstWidth || DTid.y >= g_dstHeight)
        return;

    // Bindless UAV access (SM6.6)
    RWTexture2D<float4> srcMip = ResourceDescriptorHeap[g_srcTextureIndex];
    RWTexture2D<float4> dstMip = ResourceDescriptorHeap[g_dstMipUavIndex];

    // Load 2x2 source texels
    int2 srcCoord = DTid.xy * 2;
    float4 s00 = srcMip[srcCoord];
    float4 s10 = srcMip[srcCoord + int2(1, 0)];
    float4 s01 = srcMip[srcCoord + int2(0, 1)];
    float4 s11 = srcMip[srcCoord + int2(1, 1)];

    // Alpha-weighted RGB average
    float totalAlpha = s00.a + s10.a + s01.a + s11.a;
    float3 weightedRgb = s00.rgb * s00.a + s10.rgb * s10.a
                       + s01.rgb * s01.a + s11.rgb * s11.a;

    float3 finalRgb = (totalAlpha > 0.001) ? weightedRgb / totalAlpha : float3(0, 0, 0);
    float  finalAlpha = totalAlpha * 0.25;

    dstMip[DTid.xy] = float4(finalRgb, finalAlpha);
}
