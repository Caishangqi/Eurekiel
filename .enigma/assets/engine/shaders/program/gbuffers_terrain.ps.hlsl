/**
 * @file gbuffers_terrain.ps.hlsl
 * @brief Terrain Pixel Shader - G-Buffer Output with Vanilla Lighting
 * @date 2026-01-01
 *
 * G-Buffer Output Layout:
 * - colortex0 (SV_TARGET0): Lit Albedo RGB (with lighting applied)
 * - colortex1 (SV_TARGET1): Lightmap (R=blocklight, G=skylight)
 * - colortex2 (SV_TARGET2): Encoded Normal ([-1,1] -> [0,1])
 *
 * Lighting Model (Minecraft Vanilla):
 * - effectiveSkyLight = skyLight * skyBrightness
 * - finalLight = max(blockLight, effectiveSkyLight)
 * - litColor = albedo * finalLight
 *
 * Reference:
 * - Engine/Voxel/World/TerrainVertexLayout.hpp
 * - Minecraft LevelLightEngine.getRawBrightness()
 * - voxel-light-engine/design.md
 */

#include "../core/Common.hlsl"
#include "../include/celestial_uniforms.hlsl"

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
 * @brief Calculate final lighting intensity using Minecraft vanilla formula
 * @param blockLight Block light value (0.0 - 1.0, from lightmap.r)
 * @param skyLight Sky light value (0.0 - 1.0, from lightmap.g)
 * @param skyBrightnessValue Time-based sky brightness (0.2 - 1.0, from celestial uniforms)
 * @return Final light intensity (0.0 - 1.0)
 *
 * Reference: Minecraft LevelLightEngine.getRawBrightness()
 * Formula: finalLight = max(blockLight, skyLight * skyBrightness)
 *
 * [IMPORTANT] This is NOT multiplication! It's max() operation.
 * - Block light is independent of time
 * - Sky light is scaled by skyBrightness (time-dependent)
 * - Final light = whichever is brighter
 */
float CalculateLightIntensity(float blockLight, float skyLight, float skyBrightnessValue)
{
    // Scale sky light by time-based brightness
    // skyBrightness: 1.0 at noon, 0.2 at midnight
    float effectiveSkyLight = skyLight * skyBrightnessValue;

    // Final light = max of block light and effective sky light
    // [IMPORTANT] This matches Minecraft's lighting model
    return max(blockLight, effectiveSkyLight);
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

    // [STEP 4] Calculate lighting using Minecraft vanilla formula
    // Lightmap: x = blockLight, y = skyLight (both 0.0 - 1.0)
    // skyBrightness: from celestial_uniforms.hlsl (0.2 - 1.0 based on time)
    float blockLight     = input.LightmapCoord.x;
    float skyLight       = input.LightmapCoord.y;
    float lightIntensity = CalculateLightIntensity(blockLight, skyLight, skyBrightness);

    // [STEP 5] Apply lighting to albedo
    // Add minimum ambient (0.03) to prevent completely black areas
    float finalLight = max(lightIntensity, 0.03);
    // TODO: Should happen in final or composite pass
    // See https://shaders.properties/current/guides/your-first-shaderpack/3_deferred_lighting/, the deferred light equation should be
    // color.rgb *= blocklight + skylight + ambient + sunlight;
    float4 litColor = float4(albedo.rgb * finalLight, albedo.a);

    // [STEP 6] Write G-Buffer outputs
    // colortex0: Lit albedo with alpha
    output.Color0 = litColor;

    // colortex1: Lightmap data (R=blocklight, G=skylight, B=0, A=1)
    output.Color1 = float4(input.LightmapCoord.x, input.LightmapCoord.y, 0.0, 1.0);

    // colortex2: Encoded world normal
    output.Color2 = float4(EncodeNormal(normalize(input.Normal)), 1.0);

    return output;
}
