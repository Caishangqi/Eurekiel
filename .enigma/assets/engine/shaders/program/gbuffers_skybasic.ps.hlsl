/**
 * @file gbuffers_skybasic.ps.hlsl
 * @brief Sky with Void Gradient - Pixel Shader (Minecraft Style)
 * @date 2025-12-07
 * @version v2.2
 *
 * [NEW v2.2] Added STARS phase for star field rendering
 * [NEW v2.1] SUNSET phase now uses colorModulator for GPU-side coloring
 * This matches Minecraft/Iris ColorModulator behavior exactly
 *
 * Features:
 * - Uses renderStage uniform to differentiate rendering phases
 * - SKY phase: Uses CPU-calculated skyColor uniform
 * - SUNSET phase: vertexColor * colorModulator (Minecraft/Iris style)
 *   - Vertex color: pure white (255,255,255), alpha gradient
 *   - colorModulator: sunriseColor from CPU (CelestialUniforms)
 * - STARS phase: Star field with colorModulator brightness control
 * - VOID phase: Uses darkened skyColor for void dome
 * - Applies Void gradient when camera is below minBuildHeight
 *
 * Render Stage Values (from common_uniforms.hlsl):
 * - RENDER_STAGE_SKY (1): Sky dome rendering
 * - RENDER_STAGE_SUNSET (2): Sunset/sunrise strip rendering
 * - RENDER_STAGE_STARS (6): Star field rendering
 * - RENDER_STAGE_VOID (7): Void plane rendering
 *
 * Reference:
 * - Iris CommonUniforms.java:108 - uniform1i("renderStage", ...)
 * - Iris VanillaTransformer.java:76-79 - iris_Color * iris_ColorModulator
 * - Iris WorldRenderingPhase.java - SKY, SUNSET, STARS, VOID phases
 * - Minecraft LevelRenderer.java:1556-1560 - Star rendering
 * - Minecraft FogRenderer.java:45-150
 */

#include "../core/core.hlsl"
#include "../include/common_uniforms.hlsl"
#include "../include/celestial_uniforms.hlsl"

struct PSOutput
{
    float4 Color0 : SV_Target0; // colortex0 (sky color)
};

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

    /*RENDERTARGETS: 0,3,7*/
    /*BLEND:ADD*/
    /*DEPTHTEST:GREATER*/

    /*DEPTHWRITE:OFF*/
    /*ALPHATEST:0.5*/
    /*CULLFACE:BACK*/
    /*CULLFACE:FRONT*/

#ifdef DEBUG_VISUALIZE_GBUFFERS
#endif


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
        // [SKY DOME] Use CPU-calculated vertex color with fog blending (Iris-style)
        // ---------------------------------------------------------------------
        // Reference: Iris FogMode.OFF for SKY_BASIC - fog is pre-blended on CPU
        //
        // CPU-side fog blending (GenerateSkyDiscWithFog):
        // - Center vertex (zenith): pure skyColor
        // - Edge vertices (horizon): pure fogColor
        // - GPU interpolation creates smooth gradient between them
        //
        // This replaces the old uniform-based approach for natural sky appearance
        finalColor = input.Color.rgb;
    }
    else if (renderStage == RENDER_STAGE_SUNSET)
    {
        // ---------------------------------------------------------------------
        // [SUNSET STRIP] Vertex color * colorModulator (Minecraft/Iris style)
        // ---------------------------------------------------------------------
        // Reference: Minecraft LevelRenderer.java renderSky() -> getSunriseColor()
        //            Iris VanillaTransformer.java: gl_Color = iris_Color * iris_ColorModulator
        //
        // Flow:
        //   1. CPU: Vertex color = pure white (255,255,255), alpha gradient (center=255, edge=0)
        //   2. CPU: colorModulator = sunriseColor (from getSunriseColor())
        //   3. GPU: finalColor = white * colorModulator = actual sunset color
        //
        // Alpha handling:
        //   - input.Color.a: Vertex alpha (center=1.0 opaque, edge=0.0 transparent)
        //   - colorModulator.a: Sunrise color alpha intensity
        //   - Final alpha = input.Color.a * colorModulator.a (matches Minecraft)
        // ---------------------------------------------------------------------
        float3 vertexColor = input.Color.rgb;
        float  vertexAlpha = input.Color.a;

        // Apply colorModulator (matches Minecraft: fragColor = color * ColorModulator)
        finalColor = vertexColor * colorModulator.rgb;

        // Discard fully transparent pixels (Minecraft behavior)
        float finalAlpha = vertexAlpha * colorModulator.a;
        if (finalAlpha < 0.001)
        {
            discard;
        }

        // Output with proper alpha for blending
        output.Color0 = float4(finalColor, finalAlpha);
        return output;
    }
    else if (renderStage == RENDER_STAGE_STARS)
    {
        // ---------------------------------------------------------------------
        // [STARS] Star field rendering with brightness from colorModulator
        // ---------------------------------------------------------------------
        // Reference: Minecraft LevelRenderer.java:1556-1560
        //   RenderSystem.setShaderColor(starBrightness, starBrightness, starBrightness, starBrightness)
        //
        // Flow:
        //   1. CPU: Vertex color = white (star color)
        //   2. CPU: colorModulator.a = starBrightness (calculated from time/weather)
        //   3. GPU: Apply brightness as alpha for additive blending
        //
        // Stars use additive blending: finalColor added to existing sky color
        // Alpha controls how much the star contributes to the final image
        // ---------------------------------------------------------------------
        float3 starColor      = input.Color.rgb;
        float  starBrightness = colorModulator.a; // Star brightness from CPU

        // Apply brightness to star color
        finalColor = starColor * starBrightness;

        // Output with alpha for blending (additive blend uses alpha as intensity)
        output.Color0 = float4(finalColor, starBrightness);
        return output;
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
        float3 cameraWorldPos = gbufferViewInverse[3].xyz;
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
