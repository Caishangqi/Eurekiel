/**
 * @file shadow.vs.hlsl
 * @brief Shadow Pass Vertex Shader - Light Space Transformation
 * @date 2026-01-07
 *
 * [NEW] Shadow mapping vertex shader for depth-only rendering.
 * Transforms vertices from world space to light space using
 * shadowView and shadowProjection matrices.
 *
 * Input Layout (matches TerrainVertex):
 * - POSITION (float3) - Vertex position
 * - TEXCOORD0 (float2) - UV for alpha testing
 *
 * Output: Light space position + UV for shadow.ps.hlsl
 */

#include "../core/Common.hlsl"

// ============================================================================
// Shadow Pass Vertex Structures
// ============================================================================

/**
 * @brief Shadow vertex shader input - minimal for depth-only pass
 */
struct VSInput_Shadow
{
    float3 Position : POSITION;
    float4 Color : COLOR0; // Unused but required for vertex layout
    float2 TexCoord : TEXCOORD0; // For alpha testing
    float3 Normal : NORMAL; // Unused but required for vertex layout
    float2 Lightmap : LIGHTMAP; // Unused but required for vertex layout
};

/**
 * @brief Shadow vertex shader output
 */
struct VSOutput_Shadow
{
    float4 Position : SV_POSITION; // Light space clip position
    float2 TexCoord : TEXCOORD0; // UV for alpha testing in PS
};

/**
 * @brief Shadow pass vertex shader main entry
 * @param input Vertex data from VBO
 * @return Light space transformed vertex
 *
 * Transform chain: Model -> World -> ShadowView -> ShadowClip
 * Uses shadowView and shadowProjection from Matrices cbuffer (b7)
 */
VSOutput_Shadow main(VSInput_Shadow input)
{
    VSOutput_Shadow output;

    // [STEP 1] Transform to world space
    float4 localPos = float4(input.Position, 1.0);
    float4 worldPos = mul(modelMatrix, localPos);

    // [STEP 2] Transform to shadow view space
    float4 shadowViewPos   = mul(shadowView, worldPos);
    float4 shadowRenderPos = mul(gbufferRenderer, shadowViewPos);
    float4 shadowClipPos   = mul(shadowProjection, shadowRenderPos);

    output.Position = shadowClipPos;
    output.TexCoord = input.TexCoord;

    return output;
}
