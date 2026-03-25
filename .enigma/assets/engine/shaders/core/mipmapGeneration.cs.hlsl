/**
 * @file MipmapGeneration.cs.hlsl
 * @brief Built-in compute shader for single-level mipmap downsampling
 *
 * Uses SM6.6 Bindless access for all resources:
 * - Texture2D via ResourceDescriptorHeap[g_srcTextureIndex]
 * - RWTexture2D<float4> via ResourceDescriptorHeap[g_dstMipUavIndex]
 * - SamplerState via SamplerDescriptorHeap[g_samplerBindlessIndex]
 *
 * Box filter via bilinear SampleLevel at texel center:
 * Sampling at (texel + 0.5) / dstSize with bilinear filtering
 * effectively averages the 2x2 source texels = box filter.
 *
 * Dispatched once per mip level by MipmapGenerator.
 */

#include "../include/mipgen_uniforms.hlsl"

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // Bounds check: skip threads outside destination mip dimensions
    if (DTid.x >= g_dstWidth || DTid.y >= g_dstHeight)
        return;

    // Bindless resource access (SM6.6)
    Texture2D srcTexture = ResourceDescriptorHeap[g_srcTextureIndex];
    RWTexture2D<float4> dstMip = ResourceDescriptorHeap[g_dstMipUavIndex];
    SamplerState linearSampler = SamplerDescriptorHeap[g_samplerBindlessIndex];

    // Bilinear sampling at texel center = free 2x2 box filter
    float2 texelSize = 1.0 / float2(g_dstWidth, g_dstHeight);
    float2 uv = (float2(DTid.xy) + 0.5) * texelSize;

    dstMip[DTid.xy] = srcTexture.SampleLevel(linearSampler, uv, g_srcMipLevel);
}
