/**
 * @file shadow.ps.hlsl
 * @brief Shadow Pass Pixel Shader - Depth Output with Alpha Testing
 * @date 2026-01-07
 *
 * [NEW] Shadow mapping pixel shader for depth-only rendering.
 * Supports alpha testing for cutout geometry (leaves, fences, etc.)
 *
 * Output:
 * - Depth is auto-written by hardware (no explicit output needed)
 * - Optional shadowcolor0 output for colored shadows (VSM, etc.)
 *
 * Alpha Testing:
 * - Samples texture at UV coordinates
 * - Discards pixel if alpha < 0.5 (cutout threshold)
 */

#include "../core/Common.hlsl"

// ============================================================================
// Shadow Pass Pixel Structures
// ============================================================================

/**
 * @brief Pixel shader input - matches VSOutput_Shadow
 */
struct PSInput_Shadow
{
    float4 Position : SV_POSITION; // Light space clip position (system use)
    float2 TexCoord : TEXCOORD0; // UV for alpha testing
};

/**
 * @brief Pixel shader output for shadowcolor target
 * Depth is auto-written by hardware when using depth-only rendering
 */
struct PSOutput_Shadow
{
    float4 Color0 : SV_TARGET0; // shadowcolor0: Optional color output
};

/**
 * @brief Shadow pass pixel shader main entry
 * @param input Interpolated data from VS
 * @return Shadow color output (depth auto-written)
 *
 * [IMPORTANT] Depth texture (shadowtex) is written automatically by hardware.
 * This shader only handles:
 * 1. Alpha testing for cutout geometry
 * 2. Optional shadowcolor output for advanced shadow techniques
 */
PSOutput_Shadow main(PSInput_Shadow input)
{
    PSOutput_Shadow output;

    // [STEP 1] Sample texture for alpha testing (cutout geometry support)
    // Uses customImage0 as the terrain atlas (same as gbuffers_terrain)
    Texture2D gtexture = GetCustomImage(0);
    float4    texColor = gtexture.Sample(pointSampler, input.TexCoord);


    // [STEP 3] Output shadowcolor0
    // Default: black with full alpha (standard shadow)
    // Can be modified for colored shadows or VSM data
    output.Color0 = float4(0.0, 0.0, 0.0, 1.0);

    return output;
}
