/**
 * @file gbuffers_water.ps.hlsl
 * @brief Water Pixel Shader - Translucent Rendering with Alpha Blending
 * @date 2026-01-19
 *
 * Phase 1: Simple translucent water rendering
 * - Sample block atlas texture
 * - Apply vertex color with alpha
 * - Output to colortex0 (Albedo) and colortex1 (Lightmap)
 * - Alpha blending handled by render state (BlendMode::ALPHA)
 * - BlockID read from vertex data (input.entityId), not Uniform buffer
 *
 * [FIX] Sky/Unloaded Chunk Boundary Issue:
 * - Read depthtex1 (opaque-only depth, copied before translucent pass)
 * - When background is sky (depth >= 1.0), reduce water alpha to prevent bright blending
 * - Reference: Complementary Reimagined water.glsl Line 148-179
 *
 * Output Layout:
 * - colortex0 (SV_TARGET0): Albedo RGB + Alpha
 * - colortex1 (SV_TARGET1): Lightmap (blockLight, skyLight) for composite lighting
 *
 * Reference:
 * - gbuffers_terrain.ps.hlsl: Texture sampling pattern
 * - TerrainTranslucentRenderPass.cpp: Render state setup
 * - Complementary Reimagined: water.glsl depth-aware alpha
 */

#include "../core/Common.hlsl"

// [RENDERTARGETS] 0,1
// Output: colortex0 (Albedo with alpha), colortex1 (Lightmap)

// ============================================================================
// Water-Specific Pixel Shader Structures
// ============================================================================

/**
 * @brief Pixel shader input - matches VSOutput_Water
 */
struct PSInput_Water
{
    float4 Position : SV_POSITION; // Clip position (system use)
    float4 Color : COLOR0; // Vertex color (includes alpha)
    float2 TexCoord : TEXCOORD0; // UV coordinates
    float3 Normal : NORMAL; // World normal
    float2 LightmapCoord: LIGHTMAP; // Lightmap (blocklight, skylight)
    float3 WorldPos : TEXCOORD2; // World position
    uint   entityId : TEXCOORD3; // Block entity ID (for future effects)
    float2 midTexCoord : TEXCOORD4; // Texture center (for future effects)
};

/**
 * @brief Pixel shader output - Dual render targets
 */
struct PSOutput_Water
{
    float4 Color : SV_TARGET0; // colortex0: Albedo (RGB) + Alpha
    float4 Lightmap : SV_TARGET1; // colortex1: Lightmap (blockLight, skyLight, 0, 1)
};

/**
 * @brief Water pixel shader main entry
 * @param input Interpolated vertex data from VS
 * @return Final color with alpha for blending
 */
PSOutput_Water main(PSInput_Water input)
{
    PSOutput_Water output;

    // [STEP 1] Sample block atlas (customImage0 = gtexture)
    Texture2D gtexture = GetCustomImage(0);
    float4    texColor = gtexture.Sample(sampler1, input.TexCoord);

    // [STEP 2] Alpha test - discard fully transparent pixels
    if (texColor.a < 0.01)
    {
        discard;
    }

    // [STEP 3] Apply vertex color (includes alpha for water transparency)
    float4 finalColor = texColor * input.Color;

    // [STEP 5] Output final color with alpha
    // Alpha blending handled by render state (BlendMode::ALPHA)
    output.Color = finalColor;

    // [STEP 6] Output lightmap for composite lighting
    // This ensures water receives proper day/night lighting in composite pass
    output.Lightmap = float4(input.LightmapCoord.x, input.LightmapCoord.y, 0.0, 1.0);

    return output;
}
