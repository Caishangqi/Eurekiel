/**
 * @file gbuffers_terrain_cutout.ps.hlsl
 * @brief Terrain Cutout Pixel Shader - Alpha-tested blocks (leaves, grass)
 * @date 2026-01-19
 *
 * Iris Pipeline Reference:
 * - Phase: TERRAIN_CUTOUT / TERRAIN_CUTOUT_MIPPED
 * - Alpha Test: ONE_TENTH_ALPHA (alpha > 0.1)
 * - Fallback: gbuffers_terrain_cutout -> gbuffers_terrain
 *
 * G-Buffer Output Layout:
 * - colortex0 (SV_TARGET0): Albedo RGB + Alpha
 * - colortex1 (SV_TARGET1): Lightmap (R=blocklight, G=skylight)
 * - colortex2 (SV_TARGET2): Encoded Normal ([-1,1] -> [0,1])
 *
 * Key Difference from gbuffers_terrain.ps.hlsl:
 * - Alpha test threshold: 0.1 (Iris ONE_TENTH_ALPHA)
 * - Designed for leaves, grass, flowers, etc.
 *
 * Reference:
 * - Engine/Voxel/World/TerrainVertexLayout.hpp
 * - Iris ShaderKey.java: TERRAIN_CUTOUT(AlphaTests.ONE_TENTH_ALPHA)
 */

#include "../core/core.hlsl"

// [RENDERTARGETS] 0,1,2
// Output: colortex0 (Albedo), colortex1 (Lightmap), colortex2 (Normal)

// ============================================================================
// Alpha Test Configuration (Iris Compatible)
// ============================================================================

// Iris AlphaTests.ONE_TENTH_ALPHA = 0.1
#define ALPHA_TEST_THRESHOLD 0.1

// ============================================================================
// Terrain Cutout Pixel Shader Structures
// ============================================================================

/**
 * @brief Pixel shader input - matches VSOutput_TerrainCutout
 */
struct PSInput_TerrainCutout
{
    float4 Position : SV_POSITION; // Clip position (system use)
    float4 Color : COLOR0; // Vertex color
    float2 TexCoord : TEXCOORD0; // UV coordinates
    float3 Normal : NORMAL; // World normal
    float2 LightmapCoord: LIGHTMAP; // Lightmap (blocklight, skylight)
    float3 WorldPos : TEXCOORD2; // World position
    uint   entityId : TEXCOORD5; // Block entity ID (unused)
    float2 midTexCoord : TEXCOORD6; // Texture center (unused)
};

/**
 * @brief Pixel shader output - G-Buffer targets
 */
struct PSOutput_TerrainCutout
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
 * @brief Terrain cutout pixel shader main entry
 * @param input Interpolated vertex data from VS
 * @return G-Buffer output for deferred lighting
 *
 * [IMPORTANT] Alpha test with threshold 0.1 (Iris ONE_TENTH_ALPHA)
 * Pixels with alpha < 0.1 are discarded (not written to G-Buffer)
 */
PSOutput_TerrainCutout main(PSInput_TerrainCutout input)
{
    PSOutput_TerrainCutout output;

    // [STEP 1] Sample terrain atlas (customImage0 = gtexture)
    Texture2D gtexture = GetCustomImage(0);
    float4    texColor = gtexture.Sample(sampler1, input.TexCoord);

    // [STEP 2] Alpha test - CRITICAL for cutout rendering
    // Iris uses ONE_TENTH_ALPHA (0.1) for leaves, grass, etc.
    if (texColor.a < ALPHA_TEST_THRESHOLD)
    {
        discard;
    }

    // [STEP 3] Calculate albedo (texture * vertex color RGB)
    // [AO] In separateAo mode, Color.a contains AO value, not alpha
    // RGB = directional shade, A = AO
    float3 albedoRGB = texColor.rgb * input.Color.rgb;
    float  ao        = input.Color.a; // AO from vertex color alpha

    // [STEP 4] Write G-Buffer outputs
    // colortex0: Albedo RGB + AO in alpha channel (for composite pass)
    output.Color0 = float4(albedoRGB, ao);

    // colortex1: Lightmap data (R=blocklight, G=skylight, B=0, A=1)
    output.Color1 = float4(input.LightmapCoord.x, input.LightmapCoord.y, 0.0, 1.0);

    // colortex2: Encoded world normal
    output.Color2 = float4(EncodeNormal(normalize(input.Normal)), 1.0);

    return output;
}
