/**
 * @file gbuffers_skybasic.ps.hlsl
 * @brief Sky with Void Gradient - Pixel Shader (Minecraft Style)
 * @date 2025-12-06
 * @version v2.0
 *
 * [NEW] Now uses renderStage to differentiate between SKY, SUNSET, and VOID phases
 * This mirrors Iris WorldRenderingPhase + CommonUniforms behavior
 *
 * Features:
 * - Uses renderStage uniform to differentiate rendering phases
 * - SKY phase: Uses CPU-calculated skyColor uniform
 * - SUNSET phase: Uses vertex color (passed from CPU via GenerateSunriseStrip)
 * - VOID phase: Uses darkened skyColor for void dome
 * - Applies Void gradient when camera is below minBuildHeight
 *
 * Render Stage Values (from common_uniforms.hlsl):
 * - RENDER_STAGE_SKY (1): Sky dome rendering
 * - RENDER_STAGE_SUNSET (2): Sunset/sunrise strip rendering
 * - RENDER_STAGE_VOID (7): Void plane rendering
 *
 * Reference:
 * - Iris CommonUniforms.java:108 - uniform1i("renderStage", ...)
 * - Iris WorldRenderingPhase.java - SKY, SUNSET, VOID phases
 * - Minecraft FogRenderer.java:45-150
 */

#include "../core/Common.hlsl"
#include "../include/common_uniforms.hlsl"

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
 * @brief Pixel Shader Main Entry
 * @param input Interpolated vertex data from VS (includes color for SUNSET phase)
 * @return PSOutput Sky color based on current render stage
 */
PSOutput main(PSInput input)
{
    PSOutput output;
    float3   finalColor;

    // ==========================================================================
    // [STEP 1] Determine color based on renderStage
    // ==========================================================================
    // Reference: Iris WorldRenderingPhase.java + ShaderKey.java
    // - SKY_BASIC uses POSITION format (no vertex color) -> use skyColor uniform
    // - SKY_BASIC_COLOR uses POSITION_COLOR format -> use vertex color (sunset strip)
    // ==========================================================================

    if (renderStage == RENDER_STAGE_SKY)
    {
        // ---------------------------------------------------------------------
        // [SKY DOME] Use CPU-calculated sky color from CommonUniforms
        // ---------------------------------------------------------------------
        // Reference: Iris CommonUniforms.java:167 getSkyColor()
        // skyColor is calculated on CPU based on time, weather, dimension
        finalColor = skyColor;
    }
    else if (renderStage == RENDER_STAGE_SUNSET)
    {
        // ---------------------------------------------------------------------
        // [SUNSET STRIP] Use vertex color (passed from CPU)
        // ---------------------------------------------------------------------
        // Reference: Minecraft LevelRenderer.java renderSky() -> getSunriseColor()
        // Sunset strip vertices contain pre-calculated sunrise/sunset colors
        // Alpha channel controls intensity (used for blending)
        finalColor = input.Color.rgb * input.Color.a;
    }
    else if (renderStage == RENDER_STAGE_VOID)
    {
        // ---------------------------------------------------------------------
        // [VOID DOME] Use darkened sky color for void plane
        // ---------------------------------------------------------------------
        // Reference: Minecraft LevelRenderer.java:1599-1607 darkBuffer
        // Void dome is rendered when camera is below horizon height
        finalColor = skyColor * 0.0; // Pure black for void (Minecraft behavior)
    }
    else
    {
        // ---------------------------------------------------------------------
        // [FALLBACK] Default to sky color
        // ---------------------------------------------------------------------
        finalColor = skyColor;
    }

    // ==========================================================================
    // [STEP 2] Apply Void Gradient (only for SKY phase)
    // ==========================================================================
    // Reference: Minecraft FogRenderer.java setupColor()
    // Only apply void gradient when rendering sky dome, not sunset strip
    // ==========================================================================

    if (renderStage == RENDER_STAGE_SKY)
    {
        // Extract camera world position from gbufferModelViewInverse
        float3 cameraWorldPos = gbufferModelViewInverse[3].xyz;
        float  cameraZ        = cameraWorldPos.z; // Camera Z-axis (vertical height)

        // Calculate Void Gradient (Minecraft Algorithm)
        // Only active when camera is below minBuildHeight
        float voidDarkness = (cameraZ - MIN_BUILD_HEIGHT) * CLEAR_COLOR_SCALE;
        voidDarkness       = saturate(voidDarkness); // Clamp to [0, 1]
        voidDarkness       = voidDarkness * voidDarkness; // Quadratic falloff

        // Apply Void Gradient to Sky Color
        finalColor *= voidDarkness;
    }

    // ==========================================================================
    // [STEP 3] Output Final Color
    // ==========================================================================
    output.Color0 = float4(finalColor, 1.0);

    return output;
}
