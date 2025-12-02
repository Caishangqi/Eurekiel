/**
 * @file gbuffers_clouds.vs.hlsl
 * @brief Cloud Rendering - Vertex Shader (Minecraft Vanilla Style)
 * @date 2025-11-26
 *
 * Features:
 * - Renders 32x32 cell cloud mesh grid
 * - Cloud animation driven by cloudTime from CelestialUniforms
 * - UV offset animation (not vertex position animation)
 * - Uses MatricesUniforms 4-stage transform chain
 *
 * Cloud Animation Algorithm:
 * 1. Get cloudTime from CelestialUniforms (increments over time)
 * 2. Apply UV offset: UV += cloudTime * cloudSpeed
 * 3. UV wraps automatically (256x256 texture tiles seamlessly)
 *
 * Transform Chain:
 * 1. World � Camera: gbufferModelView
 * 2. Camera � Render: cameraToRenderTransform
 * 3. Render � Clip: gbufferProjection
 */

#include "../core/Common.hlsl"
#include "../include/celestial_uniforms.hlsl"

// [RENDERTARGETS] 0
// Output to colortex0 (main color)

/**
 * @brief Vertex Shader Main Entry
 * @param input Vertex data from cloud mesh geometry
 * @return VSOutput Transformed vertex with animated UV coordinates
 *
 * Cloud Animation:
 * - UV coordinates are offset by cloudTime to create scrolling effect
 * - Cloud speed is controlled by CLOUD_SPEED constant (default: 0.03)
 * - UV wraps seamlessly due to 256x256 tileable texture
 */
VSOutput main(VSInput input)
{
    VSOutput output;

    // [STEP 1] World � Camera Transform
    float4 worldPos  = float4(input.Position, 1.0);
    float4 cameraPos = mul(gbufferModelView, worldPos);

    // [STEP 2] Camera � Render Transform (Player rotation)
    float4 renderPos = mul(cameraToRenderTransform, cameraPos);

    // [STEP 3] Render � Clip Transform (Projection)
    float4 clipPos = mul(gbufferProjection, renderPos);

    output.Position  = clipPos;
    output.Color     = input.Color;
    output.Normal    = input.Normal;
    output.Tangent   = input.Tangent;
    output.Bitangent = input.Bitangent;
    output.WorldPos  = worldPos.xyz;

    // [STEP 4] Apply Cloud Animation to UV Coordinates
    // Cloud animation: UV offset based on cloudTime
    // cloudTime increments continuously, creating scrolling effect
    const float CLOUD_SPEED = 0.03; // Cloud animation speed multiplier
    float2      uvOffset    = float2(cloudTime * CLOUD_SPEED, 0.0); // Scroll in X direction
    output.TexCoord         = input.TexCoord + uvOffset;

    // Note: UV wrapping is handled automatically by sampler
    // 256x256 clouds.png texture tiles seamlessly

    return output;
}
