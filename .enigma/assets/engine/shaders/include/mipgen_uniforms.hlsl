/**
 * @file mipgen_uniforms.hlsl
 * @brief MipGen Uniforms Constant Buffer for compute shader mipmap generation
 *
 * [Memory Layout] 32 bytes (2 rows x 16 bytes)
 *   Row 0 [0-15]:  g_srcTextureIndex, g_dstMipUavIndex, g_srcMipLevel, g_samplerBindlessIndex
 *   Row 1 [16-31]: g_dstWidth, g_dstHeight, _mipgen_pad (uint2)
 *
 * C++ Counterpart: MipGenUniforms struct in MipmapGenerator.hpp
 * Root CBV Slot: b11 (ROOT_CBV_MIPGEN)
 */

#ifndef MIPGEN_UNIFORMS_HLSL
#define MIPGEN_UNIFORMS_HLSL

cbuffer MipGenUniforms : register(b11, space0)
{
    uint g_srcTextureIndex;      // Bindless SRV index (source texture)
    uint g_dstMipUavIndex;       // Bindless UAV index (destination mip)
    uint g_srcMipLevel;          // Source mip level to read from
    uint g_samplerBindlessIndex; // Sampler bindless index (from SamplerProvider)
    uint g_dstWidth;             // Destination mip width
    uint g_dstHeight;            // Destination mip height
    uint2 _mipgen_pad;          // Padding to 32 bytes
};

#endif // MIPGEN_UNIFORMS_HLSL
