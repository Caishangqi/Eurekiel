/**
 * @file gbuffers_clouds.ps.hlsl
 * @brief [REWRITE] Cloud Rendering - Pixel Shader (Sodium Architecture)
 * @date 2025-12-02
 *
 * Architecture Design:
 * - Sodium CloudRenderer.java architecture (CPU-side texture sampling)
 * - Pixel Shader is a simple pass-through (no texture sampling)
 * - All color calculations done on CPU side (texture * brightness)
 * - Alpha test for cloud shape formation (threshold: 0.1)
 *
 * Features:
 * - Direct output of vertex color (no texture sampling)
 * - Alpha discard for transparency (threshold: 0.1)
 * - Single render target: colortex0 (main color)
 *
 * CPU-Side Calculations (done before this shader):
 * - clouds.png texture sampling (256x256 RGBA)
 * - Color modulation by brightness (face-dependent)
 * - Vertex color = textureColor * brightness
 *
 * Shader Responsibility:
 * - Alpha test: discard if alpha < 0.1
 * - Output vertex color directly
 */

#include "../core/Common.hlsl"

// [RENDERTARGETS] 0
// Output to colortex0 (main color)

/**
 * @brief Pixel Shader Output Structure
 * ShaderCodeGenerator reads RENDERTARGETS comment and generates this struct
 */
struct PSOutput
{
    float4 Color : SV_Target0; // colortex0: Cloud color (RGBA)
};

/**
 * @brief Pixel Shader Main Entry
 * @param input Interpolated vertex data from Vertex Shader
 * @return PSOutput Single render target output
 *
 * Key Points:
 * - input.Color contains pre-calculated RGBA (texture * brightness)
 * - No texture sampling (done on CPU side)
 * - Simple alpha test for cloud shape formation
 * - Direct color output
 */
PSOutput main(VSOutput input)
{
    PSOutput output;

    // [STEP 1] Alpha Test - Form Cloud Shapes
    // Discard fragments with alpha below 0.1 to create cloud edges
    // This threshold matches Sodium CloudRenderer behavior
    if (input.Color.a < 0.1)
    {
        discard;
    }

    // [STEP 2] Direct Color Output
    // Color already contains: textureColor * brightness (CPU-calculated)
    // No need for texture sampling or additional calculations
    output.Color = input.Color;

    return output;
}
