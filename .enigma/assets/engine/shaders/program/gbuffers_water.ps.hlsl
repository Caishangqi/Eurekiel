/**
 * @file gbuffers_water.ps.hlsl
 * @brief Water Pixel Shader - Translucent Rendering with Alpha Blending
 * @date 2026-01-17
 *
 * Phase 1: Simple translucent water rendering
 * - Sample block atlas texture
 * - Apply vertex color with alpha
 * - Output to colortex0 only (forward rendering style)
 * - Alpha blending handled by render state (BlendMode::ALPHA)
 * - BlockID read from vertex data (input.entityId), not Uniform buffer
 *
 * Output Layout:
 * - colortex0 (SV_TARGET0): Albedo RGB + Alpha
 *
 * Reference:
 * - gbuffers_terrain.ps.hlsl: Texture sampling pattern
 * - TerrainTranslucentRenderPass.cpp: Render state setup
 */

#include "../core/Common.hlsl"

// [RENDERTARGETS] 0
// Output: colortex0 (Albedo with alpha)

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
 * @brief Pixel shader output - Single render target
 */
struct PSOutput_Water
{
    float4 Color : SV_TARGET0; // colortex0: Albedo (RGB) + Alpha
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

    // [STEP 4] Optional: Access entityId for future water-specific effects
    // Phase 1: entityId available but unused
    // Future: Can use entityId to identify water blocks for special effects
    // uint blockId = input.entityId;

    // [STEP 5] Output final color with alpha
    // Alpha blending handled by render state (BlendMode::ALPHA)
    output.Color = finalColor;

    return output;
}
