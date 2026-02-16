/// Core Buffer Index import

#include "../include/custom_image_uniforms.hlsl"
#include "../include/color_texture_uniforms.hlsl"
#include "../include/matrices_uniforms.hlsl"
#include "../include/shadow_color_uniforms.hlsl"
#include "../include/depth_texture_uniforms.hlsl"
#include "../include/shadow_texture_uniforms.hlsl"
#include "../include/sampler_uniforms.hlsl"
#include "../include/perobject_uniforms.hlsl"
#include "../include/viewport_uniforms.hlsl"
#include "../include/worldtime_uniforms.hlsl"

#include "../include/camera_uniforms.hlsl"
#include "../include/celestial_uniforms.hlsl"
#include "../include/fog_uniforms.hlsl"
#include "../include/common_uniforms.hlsl"
#include "../include/worldinfo_uniforms.hlsl"

/**
 * @brief vertex shader input structure
 *
 *Teaching points:
 * 1. Fixed vertex format - all geometry uses the same layout
 * 2. Corresponds to C++ enigma::graphic::Vertex (Vertex_PCUTBN)
 * 3. For Gbuffers Pass (geometry rendering)
 *
 * Note: Composite Pass uses full-screen triangles and does not require complex vertex formats
 */
struct VSInput
{
    float3 Position : POSITION; // Vertex position (world space)
    float4 Color : COLOR0; // Vertex color (R8G8B8A8_UNORM)
    float2 TexCoord : TEXCOORD0; // Texture coordinates
    float3 Tangent : TANGENT; // Tangent vector
    float3 Bitangent : BITANGENT; // Bitangent vector
    float3 Normal : NORMAL; // normal vector
};

/**
 * @brief vertex shader output / pixel shader input
 */
struct VSOutput
{
    float4 Position : SV_POSITION; // Clipping space position
    float4 Color : COLOR0; // Vertex color (after unpacking)
    float2 TexCoord : TEXCOORD0; // Texture coordinates
    float3 Tangent : TANGENT; // Tangent vector
    float3 Bitangent : BITANGENT; // Bitangent vector
    float3 Normal : NORMAL; // normal vector
    float3 WorldPos : TEXCOORD1; // World space position
};

// Pixel shader input (same as VSOutput)
typedef VSOutput PSInput;

/**
 * @brief Unpack Rgba8 color (uint → float4)
 * @param packedColor packed RGBA8 color (0xAABBGGRR)
 * @return unpacked float4 color (0.0-1.0 range)
 */
float4 UnpackRgba8(uint packedColor)
{
    float r = float((packedColor >> 0) & 0xFF) / 255.0;
    float g = float((packedColor >> 8) & 0xFF) / 255.0;
    float b = float((packedColor >> 16) & 0xFF) / 255.0;
    float a = float((packedColor >> 24) & 0xFF) / 255.0;
    return float4(r, g, b, a);
}

/**
 * @brief Standard vertex transformation function (for Fallback Shader)
 * @param input vertex input (VSInput structure)
 * @return VSOutput - transformed vertex output
 *
 * Teaching points:
 * 1. Fallback shader for gbuffers_basic and gbuffers_textured
 * 2. Automatically handle MVP transformation, color unpacking, and data transfer
 * 3. Get the transformation matrix from Uniform Buffers
 * 4. KISS principle - minimalist implementation, no additional calculations
 *
 * Workflow:
 * 1. [NEW] Directly access cbuffer Matrices to obtain the transformation matrix (no function call required)
 * 2. Vertex position transformation: Position → ViewSpace → ClipSpace
 * 3. Normal transformation: normalMatrix transforms the normal vector
 * 4. Color unpacking: uint → float4
 * 5. Pass all vertex attributes to the pixel shader
 */
VSOutput StandardVertexTransform(VSInput input)
{
    VSOutput output;

    // 1. Vertex position transformation
    float4 localPos  = float4(input.Position, 1.0);
    float4 worldPos  = mul(modelMatrix, localPos);
    float4 cameraPos = mul(gbufferView, worldPos);
    float4 renderPos = mul(gbufferRenderer, cameraPos);
    float4 clipPos   = mul(gbufferProjection, renderPos);

    output.Position = clipPos;
    output.WorldPos = worldPos.xyz;
    output.Color    = input.Color;
    output.TexCoord = input.TexCoord;

    // 4. Normal transformation
    output.Normal = normalize(mul(normalMatrix, float4(input.Normal, 0.0)).xyz);

    // 5. Tangents and secondary tangents
    output.Tangent   = normalize(mul(gbufferView, float4(input.Tangent, 0.0)).xyz);
    output.Bitangent = normalize(mul(gbufferView, float4(input.Bitangent, 0.0)).xyz);

    return output;
}

#define PI 3.14159265359
#define TWO_PI 6.28318530718
#define HALF_PI 1.57079632679
