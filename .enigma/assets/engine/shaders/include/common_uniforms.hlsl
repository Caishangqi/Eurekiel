/**
 * @file common_uniforms.hlsl
 * @brief Common Uniforms Constant Buffer - For General Rendering
 * @date 2025-12-06
 * @version v2.0
 *
 * Key Points:
 * 1. Uses cbuffer to store common rendering data (sky color, fog, weather, render stage)
 * 2. Register b16 - User-defined area (>= 15)
 * 3. PerFrame update frequency - Updated once per frame (renderStage updates per draw call)
 * 4. Field order must match CommonConstantBuffer.hpp exactly
 * 5. float3 maps to C++ Vec3 (HLSL compiler handles padding automatically)
 *
 * Architecture Benefits:
 * - High performance: Root CBV direct access, no indirection
 * - Memory aligned: 16-byte aligned, GPU friendly
 * - Iris compatible: Mirrors Iris CommonUniforms for shader pack compatibility
 *
 * Usage Scenarios:
 * - gbuffers_skybasic.hlsl - Sky color rendering (use renderStage to differentiate SKY vs SUNSET)
 * - gbuffers_terrain.hlsl - Terrain fog blending
 * - composite.ps.hlsl - Post-processing effects
 * - deferred.ps.hlsl - Deferred lighting
 *
 * C++ Counterpart: Code/Game/Framework/RenderPass/ConstantBuffer/CommonConstantBuffer.hpp
 *
 * Reference:
 * - Iris CommonUniforms.java (line 108): uniform1i("renderStage", ...)
 * - Iris CommonUniforms.java (line 167): .uniform3d(PER_FRAME, "skyColor", CommonUniforms::getSkyColor)
 * - Iris WorldRenderingPhase.java - renderStage enum values
 * - Minecraft ClientLevel.getSkyColor() - CPU-side sky color calculation
 */

// =============================================================================
// [USER-FRIENDLY MACROS] Render Stage Constants
// =============================================================================
// These macros mirror Iris WorldRenderingPhase.java enum ordinal values
// Use these instead of magic numbers for better readability
//
// Usage:
//   if (renderStage == RENDER_STAGE_SKY) { /* sky dome */ }
//   if (renderStage == RENDER_STAGE_SUNSET) { /* sunset strip */ }
// =============================================================================

// ==================== Sky Rendering Phases ====================
#define RENDER_STAGE_NONE              0   // No specific phase
#define RENDER_STAGE_SKY               1   // Sky dome (POSITION format)
#define RENDER_STAGE_SUNSET            2   // Sunset/sunrise strip (POSITION_COLOR format)
#define RENDER_STAGE_CUSTOM_SKY        3   // Custom sky (FabricSkyboxes, etc.)
#define RENDER_STAGE_SUN               4   // Sun billboard
#define RENDER_STAGE_MOON              5   // Moon billboard
#define RENDER_STAGE_STARS             6   // Star field
#define RENDER_STAGE_VOID              7   // Void plane (below world)

// ==================== Terrain Rendering Phases ====================
#define RENDER_STAGE_TERRAIN_SOLID         8   // Solid terrain blocks
#define RENDER_STAGE_TERRAIN_CUTOUT_MIPPED 9   // Cutout terrain with mipmapping
#define RENDER_STAGE_TERRAIN_CUTOUT        10  // Cutout terrain without mipmapping
#define RENDER_STAGE_TERRAIN_TRANSLUCENT   17  // Translucent terrain (water, ice)

// ==================== Entity Rendering Phases ====================
#define RENDER_STAGE_ENTITIES          11  // Entity rendering
#define RENDER_STAGE_BLOCK_ENTITIES    12  // Block entity rendering

// ==================== Special Rendering Phases ====================
#define RENDER_STAGE_DESTROY           13  // Block breaking animation
#define RENDER_STAGE_OUTLINE           14  // Block selection outline
#define RENDER_STAGE_DEBUG             15  // Debug rendering
#define RENDER_STAGE_HAND_SOLID        16  // Hand rendering (solid)
#define RENDER_STAGE_TRIPWIRE          18  // Tripwire rendering
#define RENDER_STAGE_PARTICLES         19  // Particle rendering
#define RENDER_STAGE_CLOUDS            20  // Cloud rendering
#define RENDER_STAGE_RAIN_SNOW         21  // Weather effects
#define RENDER_STAGE_WORLD_BORDER      22  // World border effect
#define RENDER_STAGE_HAND_TRANSLUCENT  23  // Hand rendering (translucent)

// =============================================================================
// [HELPER MACROS] Render Stage Checks
// =============================================================================
// Convenience macros for common render stage checks
// =============================================================================

#define IS_SKY_PHASE(stage)     ((stage) >= RENDER_STAGE_SKY && (stage) <= RENDER_STAGE_VOID)
#define IS_TERRAIN_PHASE(stage) ((stage) == RENDER_STAGE_TERRAIN_SOLID || \
                                 (stage) == RENDER_STAGE_TERRAIN_CUTOUT_MIPPED || \
                                 (stage) == RENDER_STAGE_TERRAIN_CUTOUT || \
                                 (stage) == RENDER_STAGE_TERRAIN_TRANSLUCENT)

// =============================================================================
// CommonUniforms Constant Buffer (80 bytes)
// =============================================================================
/**
 * @brief CommonUniforms - Common Rendering Data Constant Buffer
 *
 * Field Descriptions:
 * - skyColor: CPU-calculated sky color based on time, weather, dimension
 *   - Source: Minecraft ClientLevel.getSkyColor(position, tickDelta)
 *   - [IMPORTANT] This is the PRIMARY sky color - use this instead of hardcoded values!
 *
 * - fogColor: CPU-calculated fog color for horizon blending
 *   - Source: Minecraft FogRenderer.setupColor()
 *
 * - rainStrength/wetness/thunderStrength: Weather parameters (0.0-1.0)
 *
 * - screenBrightness/nightVision/blindness/darknessFactor: Player effects (0.0-1.0)
 *
 * - renderStage: Current rendering phase (WorldRenderingPhase ordinal)
 *   - Source: Iris CommonUniforms.java:108 - uniform1i("renderStage", ...)
 *   - Use RENDER_STAGE_* macros for comparison
 *
 * Memory Layout (16-byte aligned):
 * [0-15]   skyColor (float3), padding
 * [16-31]  fogColor (float3), padding
 * [32-47]  rainStrength, wetness, thunderStrength, padding
 * [48-63]  screenBrightness, nightVision, blindness, darknessFactor
 * [64-79]  renderStage (int), padding[3]
 */
cbuffer CommonUniforms : register(b8)
{
    // ==================== Sky Color (16 bytes) ====================
    float3 skyColor; // CPU-calculated sky color [IMPORTANT: Use this!]
    float  _padding1; // HLSL compiler auto-pads

    // ==================== Fog Color (16 bytes) ====================
    float3 fogColor; // CPU-calculated fog color
    float  _padding2; // HLSL compiler auto-pads

    // ==================== Weather Parameters (16 bytes) ====================
    float rainStrength; // Rain intensity (0.0-1.0)
    float wetness; // Wetness factor (smoothed)
    float thunderStrength; // Thunder intensity (0.0-1.0)
    float _padding3; // HLSL compiler auto-pads

    // ==================== Player State (16 bytes) ====================
    float screenBrightness; // Gamma setting (0.0-1.0)
    float nightVision; // Night vision effect (0.0-1.0)
    float blindness; // Blindness effect (0.0-1.0)
    float darknessFactor; // Darkness effect (0.0-1.0)

    // ==================== Render Stage (16 bytes) ====================
    // [NEW] Current rendering phase for shader program differentiation
    // Use RENDER_STAGE_* macros for comparison (e.g., renderStage == RENDER_STAGE_SKY)
    int renderStage; // WorldRenderingPhase ordinal
    int _padding4[3]; // HLSL compiler auto-pads
};


/**
 * Usage Example:
 *
 * // In gbuffers_skybasic.ps.hlsl:
 * #include "../include/common_uniforms.hlsl"
 *
 * float4 main(PSInput input) : SV_TARGET
 * {
 *     float3 finalColor;
 *
 *     // [NEW] Use renderStage to differentiate sky dome vs sunset strip
 *     if (renderStage == RENDER_STAGE_SKY)
 *     {
 *         // Sky dome: use CPU-calculated sky color
 *         finalColor = skyColor;
 *     }
 *     else if (renderStage == RENDER_STAGE_SUNSET)
 *     {
 *         // Sunset strip: use vertex color (passed from CPU)
 *         finalColor = input.color.rgb;
 *     }
 *
 *     // Apply weather effects
 *     finalColor = lerp(finalColor, fogColor, rainStrength * 0.5);
 *
 *     // Apply player effects
 *     finalColor *= (1.0 - blindness);
 *     finalColor += nightVision * 0.1;
 *
 *     return float4(finalColor, 1.0);
 * }
 *
 * // [BAD] Don't hardcode sky colors like this:
 * // static const float3 SKY_COLOR = float3(0.47, 0.65, 1.0); // Wrong!
 */
