/**
 * @file gbuffers_skybasic.ps.hlsl
 * @brief Sky with Void Gradient - Pixel Shader (Minecraft Style)
 * @date 2025-11-26
 *
 * Features:
 * - Renders sky color with horizon-to-zenith gradient
 * - Applies Void gradient when camera is below minBuildHeight
 * - Applies time-based sky brightness (CPU-calculated)
 * - Replicates Minecraft FogRenderer.setupColor() behavior
 *
 * Void Gradient Algorithm:
 * 1. Extract camera world position from gbufferModelViewInverse column 4
 * 2. Calculate voidDarkness = (cameraZ - minBuildHeight) * clearColorScale
 * 3. Apply quadratic falloff: voidDarkness = voidDarkness^2
 * 4. Darken sky color: skyColor *= voidDarkness
 *
 * Reference: Minecraft FogRenderer.java:45-150
 */

#include "../core/Common.hlsl"
#include "../include/celestial_uniforms.hlsl"

// [RENDERTARGETS] 0
// Output to colortex0 (sky color)

/**
 * @brief Pixel Shader Output Structure
 * ShaderCodeGenerator reads RENDERTARGETS comment and generates this struct
 */
struct PSOutput
{
    float4 Color0 : SV_Target0; // colortex0 (sky color)
};

/**
 * @brief Minecraft Void Gradient Constants
 */
static const float MIN_BUILD_HEIGHT  = -64.0; // Minecraft min build height
static const float CLEAR_COLOR_SCALE = 0.03125; // 1.0 / 32.0

/**
 * @brief Sky Color Palette (Minecraft Style)
 */
static const float3 SKY_ZENITH_COLOR  = float3(0.47, 0.65, 1.0); // Top (blue #78A7FF)
static const float3 SKY_HORIZON_COLOR = float3(0.75, 0.85, 1.0); // Horizon (light blue #C0D8FF)

/**
 * @brief Pixel Shader Main Entry
 * @param input Interpolated vertex data from VS
 * @return PSOutput Sky color with Void gradient
 */
PSOutput main(PSInput input)
{
    PSOutput output;

    // [STEP 1] Extract Camera World Position from gbufferModelViewInverse
    // Column-major layout: 4th column (index [3]) contains translation
    float3 cameraWorldPos = gbufferModelViewInverse[3].xyz;
    float  cameraZ        = cameraWorldPos.z; // Camera Z-axis (vertical height)

    // [STEP 2] Calculate Sky Color Gradient (Horizon → Zenith)
    // Interpolate based on world position height
    float  heightFactor = saturate((input.WorldPos.z + 16.0) / 512.0); // Normalized height
    float3 skyColor     = lerp(SKY_HORIZON_COLOR, SKY_ZENITH_COLOR, heightFactor);

    // [STEP 3] Calculate Void Gradient (Minecraft Algorithm)
    // Only active when camera is below minBuildHeight
    float voidDarkness = (cameraZ - MIN_BUILD_HEIGHT) * CLEAR_COLOR_SCALE;
    voidDarkness       = saturate(voidDarkness); // Clamp to [0, 1]
    voidDarkness       = voidDarkness * voidDarkness; // Quadratic falloff for smoothness

    // [STEP 3.5] Apply Sky Brightness (Time-based)
    // Minecraft formula: brightness = cos(celestialAngle * 2π) * 2 + 0.5
    // CPU-side calculated and passed via CelestialUniforms
    skyColor *= skyBrightness;

    // [STEP 4] Apply Void Gradient to Sky Color
    skyColor *= voidDarkness;

    // [STEP 5] Output Final Color
    output.Color0 = float4(skyColor, input.Color.a);

    return output;
}
