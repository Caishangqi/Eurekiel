/**
 * @file mipmapGeneration_alphaWeighted.cs.hlsl
 * @brief Alpha-weighted mipmap downsampling compute shader
 *
 * Prevents color bleeding from transparent pixels into opaque regions.
 * Premultiplies RGB by alpha before averaging, then divides by total alpha.
 * Solves white/gray fringe on cutout textures (leaves, grass, flowers).
 *
 * Uses Texture2D for source reads and RWTexture2D for destination writes.
 * Source texels are fetched with Load() from the explicit source mip level.
 * Dispatched once per mip level by MipmapGenerator.
 */

#include "../include/mipgen_uniforms.hlsl"

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= g_dstWidth || DTid.y >= g_dstHeight)
        return;

    // Bindless source SRV + destination UAV access (SM6.6)
    Texture2D srcTexture = ResourceDescriptorHeap[g_srcTextureIndex];
    RWTexture2D<float4> dstMip = ResourceDescriptorHeap[g_dstMipUavIndex];

    // Load 2x2 source texels
    int2 srcCoord = DTid.xy * 2;
    float4 s00 = srcTexture.Load(int3(srcCoord, g_srcMipLevel));
    float4 s10 = srcTexture.Load(int3(srcCoord + int2(1, 0), g_srcMipLevel));
    float4 s01 = srcTexture.Load(int3(srcCoord + int2(0, 1), g_srcMipLevel));
    float4 s11 = srcTexture.Load(int3(srcCoord + int2(1, 1), g_srcMipLevel));

    // Alpha-weighted RGB average
    float totalAlpha = s00.a + s10.a + s01.a + s11.a;
    float3 weightedRgb = s00.rgb * s00.a + s10.rgb * s10.a
                       + s01.rgb * s01.a + s11.rgb * s11.a;

    float3 finalRgb = (totalAlpha > 0.001) ? weightedRgb / totalAlpha : float3(0, 0, 0);
    float  finalAlpha = totalAlpha * 0.25;

    dstMip[DTid.xy] = float4(finalRgb, finalAlpha);
}
