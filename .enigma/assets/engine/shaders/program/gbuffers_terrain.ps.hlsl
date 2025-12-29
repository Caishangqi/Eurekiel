/**
 * @file gbuffers_terrain.ps.hlsl
 * @brief Terrain Pixel Shader - G-Buffer Deferred Output
 * @date 2025-12-25
 *
 * G-Buffer Output Layout:
 * - colortex0 (SV_TARGET0): Albedo RGB (texture * vertex color)
 * - colortex1 (SV_TARGET1): Lightmap (R=blocklight, G=skylight)
 * - colortex2 (SV_TARGET2): Encoded Normal ([-1,1] -> [0,1])
 *
 * Reference: Engine/Voxel/World/TerrainVertexLayout.hpp
 */

#include "../core/Common.hlsl"
#include "../include/common_uniforms.hlsl"

// [RENDERTARGETS] 0,1,2
// Output: colortex0 (Albedo), colortex1 (Lightmap), colortex2 (Normal)

// ============================================================================
// Terrain-Specific Pixel Shader Structures
// ============================================================================

/**
 * @brief Pixel shader input - matches VSOutput_Terrain
 */
struct PSInput_Terrain
{
    float4 Position : SV_POSITION; // Clip position (system use)
    float4 Color : COLOR0; // Vertex color
    float2 TexCoord : TEXCOORD0; // UV coordinates
    float3 Normal : NORMAL; // World normal
    float2 LightmapCoord: LIGHTMAP; // Lightmap (blocklight, skylight)
    float3 WorldPos : TEXCOORD2; // World position
};

/**
 * @brief Pixel shader output - G-Buffer targets
 */
struct PSOutput_Terrain
{
    float4 Color0 : SV_TARGET0; // colortex0: Albedo (RGB) + Alpha
    float4 Color1 : SV_TARGET1; // colortex1: Lightmap (R=block, G=sky, BA=0)
    float4 Color2 : SV_TARGET2; // colortex2: Encoded Normal (RGB)
};

/**
 * @brief Encode normal from [-1,1] to [0,1] for storage
 */
float3 EncodeNormal(float3 normal)
{
    return normal * 0.5 + 0.5;
}

/**
 * @brief Terrain pixel shader main entry
 * @param input Interpolated vertex data from VS
 * @return G-Buffer output for deferred lighting
 */
PSOutput_Terrain main(PSInput_Terrain input)
{
    PSOutput_Terrain output;

    // [STEP 1] Sample terrain atlas (customImage0 = gtexture)
    Texture2D gtexture = GetCustomImage(0);
    float4    texColor = gtexture.Sample(pointSampler, input.TexCoord);

    // [STEP 2] Alpha test - discard transparent pixels
    if (texColor.a < 0.1)
    {
        discard;
    }

    // [STEP 3] Calculate albedo (texture * vertex color)
    float4 albedo = texColor * input.Color;

    // [STEP 4] Write G-Buffer outputs
    // colortex0: Albedo with alpha
    output.Color0 = albedo;

    // colortex1: Lightmap data (R=blocklight, G=skylight, B=0, A=1)
    output.Color1 = float4(input.LightmapCoord.x, input.LightmapCoord.y, 0.0, 1.0);

    // colortex2: Encoded world normal
    output.Color2 = float4(EncodeNormal(normalize(input.Normal)), 1.0);

    return output;
}
