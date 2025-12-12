/**
 * @file gbuffers_clouds.vs.hlsl
 * @brief [REWRITE] Cloud Rendering - Vertex Shader (Sodium Architecture)
 * @date 2025-12-02
 *
 * Architecture Design:
 * - Sodium CloudRenderer.java architecture (CPU-side geometry generation)
 * - All color calculations and lighting done on CPU side
 * - Vertex Shader only performs standard MVP transformation
 * - No texture sampling - colors pre-calculated in vertex data
 *
 * Features:
 * - Standard 4-stage transform chain (World -> Camera -> Render -> Clip)
 * - Vertex color pass-through (contains pre-calculated brightness)
 * - No UV animation (cloud animation handled by CPU-side geometry rebuild)
 *
 * Transform Chain:
 * 1. World Position: input.Position (already contains cellOffset from CPU)
 * 2. World -> Camera: gbufferModelView
 * 3. Camera -> Render: cameraToRenderTransform
 * 4. Render -> Clip: gbufferProjection
 *
 * CPU-Side Calculations (done before this shader):
 * - Cloud cell geometry generation (12x12x4 cubes)
 * - Face culling (avoid Z-fighting)
 * - Brightness calculation (top: 1.0, bottom: 0.7, sides: 0.8-0.9)
 * - Texture sampling from clouds.png (256x256)
 * - Vertex color = textureColor * brightness
 */

#include "../core/Common.hlsl"

// [RENDERTARGETS] 0
// Output to colortex0 (main color)

/**
 * @brief Vertex Shader Main Entry
 * @param input Vertex data from CPU-generated cloud geometry
 * @return VSOutput Transformed vertex for rasterization
 *
 * Key Points:
 * - input.Position already contains cellOffset (cloud position + animation)
 * - input.Color contains pre-calculated RGBA (texture color * brightness)
 * - No need for celestial_uniforms.hlsl (animation done on CPU)
 * - Simple MVP transformation following standard pipeline
 */
VSOutput main(VSInput input)
{
    VSOutput output;

    // [STEP 1] 5-Stage Transform Chain (Standard Pipeline)
    // Local -> World -> Camera -> Render -> Clip
    // [FIX] Add modelMatrix transformation for smooth scrolling animation of clouds
    float4 localPos  = float4(input.Position, 1.0);
    float4 worldPos  = mul(modelMatrix, localPos);
    float4 cameraPos = mul(gbufferModelView, worldPos);
    float4 renderPos = mul(cameraToRenderTransform, cameraPos);
    float4 clipPos   = mul(gbufferProjection, renderPos);

    output.Position = clipPos;

    // [STEP 2] Pass-through Vertex Attributes
    // Color already contains: textureColor * brightness (CPU-calculated)
    output.Color     = input.Color;
    output.Normal    = input.Normal;
    output.Tangent   = input.Tangent;
    output.Bitangent = input.Bitangent;
    output.WorldPos  = worldPos.xyz;

    // [STEP 3] UV Coordinates (Unused in Sodium architecture)
    // Set to zero as texture sampling is done on CPU side
    output.TexCoord = float2(0.0, 0.0);

    return output;
}
