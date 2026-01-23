/**
 * @file common_uniforms.hlsl
 * @brief Common Uniforms Constant Buffer - Iris CommonUniforms Compatible
 * @date 2026-01-22
 * @version v3.0
 *
 * Key Points:
 * 1. Strictly matches Iris CommonUniforms.java (lines 130-171)
 * 2. Register b8, space1 - User-defined area
 * 3. PerFrame update frequency
 * 4. Field order must match CommonConstantBuffer.hpp exactly
 * 5. Total size: 144 bytes (9 rows * 16 bytes)
 *
 * Architecture Benefits:
 * - High performance: Root CBV direct access, no indirection
 * - Memory aligned: 16-byte aligned, GPU friendly
 * - Iris compatible: Mirrors Iris CommonUniforms for shader pack compatibility
 *
 * C++ Counterpart: Code/Game/Framework/RenderPass/ConstantBuffer/CommonConstantBuffer.hpp
 *
 * Reference:
 * - Iris CommonUniforms.java (lines 136-167): generalCommonUniforms()
 * - Iris CommonUniforms.java (line 108): renderStage from addDynamicUniforms()
 */

// =============================================================================
// [USER-FRIENDLY MACROS] Render Stage Constants
// =============================================================================
// These macros mirror Iris WorldRenderingPhase.java enum ordinal values
// Use these instead of magic numbers for better readability
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
#define IS_SKY_PHASE(stage)     ((stage) >= RENDER_STAGE_SKY && (stage) <= RENDER_STAGE_VOID)
#define IS_TERRAIN_PHASE(stage) ((stage) == RENDER_STAGE_TERRAIN_SOLID || \
                                 (stage) == RENDER_STAGE_TERRAIN_CUTOUT_MIPPED || \
                                 (stage) == RENDER_STAGE_TERRAIN_CUTOUT || \
                                 (stage) == RENDER_STAGE_TERRAIN_TRANSLUCENT)

// =============================================================================
// [isEyeInWater MACROS] Eye Submersion Type
// =============================================================================
#define EYE_IN_AIR         0   // Not in fluid
#define EYE_IN_WATER       1   // In water
#define EYE_IN_LAVA        2   // In lava
#define EYE_IN_POWDER_SNOW 3   // In powder snow

// =============================================================================
// CommonUniforms Constant Buffer (144 bytes = 9 rows * 16 bytes)
// =============================================================================
/**
 * @brief CommonUniforms - Iris CommonUniforms.java Compatible
 *
 * Iris CommonUniforms.java Variable Mapping:
 *   Line 136: hideGUI          -> hideGUI
 *   Line 137: isRightHanded    -> isRightHanded
 *   Line 138: isEyeInWater     -> isEyeInWater (0=air, 1=water, 2=lava, 3=powder_snow)
 *   Line 139: blindness        -> blindness
 *   Line 140: darknessFactor   -> darknessFactor
 *   Line 141: darknessLightFactor -> darknessLightFactor
 *   Line 142: nightVision      -> nightVision
 *   Line 143: is_sneaking      -> is_sneaking
 *   Line 144: is_sprinting     -> is_sprinting
 *   Line 145: is_hurt          -> is_hurt
 *   Line 146: is_invisible     -> is_invisible
 *   Line 147: is_burning       -> is_burning
 *   Line 148: is_on_ground     -> is_on_ground
 *   Line 151: screenBrightness -> screenBrightness
 *   Line 158: playerMood       -> playerMood
 *   Line 159: constantMood     -> constantMood
 *   Line 160: eyeBrightness    -> eyeBrightnessX, eyeBrightnessY
 *   Line 161: eyeBrightnessSmooth -> eyeBrightnessSmoothX, eyeBrightnessSmoothY
 *   Line 165: rainStrength     -> rainStrength
 *   Line 166: wetness          -> wetness
 *   Line 167: skyColor         -> skyColor
 *   Line 108: renderStage      -> renderStage (from addDynamicUniforms)
 *
 * Memory Layout (16-byte aligned):
 * [0-15]   Row 0: skyColor (float3), _pad0
 * [16-31]  Row 1: hideGUI, isRightHanded, isEyeInWater, _pad1
 * [32-47]  Row 2: blindness, darknessFactor, darknessLightFactor, nightVision
 * [48-63]  Row 3: is_sneaking, is_sprinting, is_hurt, is_invisible
 * [64-79]  Row 4: is_burning, is_on_ground, screenBrightness, _pad2
 * [80-95]  Row 5: playerMood, constantMood, _pad3, _pad4
 * [96-111] Row 6: eyeBrightnessX, eyeBrightnessY, eyeBrightnessSmoothX, eyeBrightnessSmoothY
 * [112-127] Row 7: rainStrength, wetness, _pad5, _pad6
 * [128-143] Row 8: renderStage, _pad7[3]
 */
cbuffer CommonUniforms : register(b8, space1)
{
    // ==================== Row 0: Sky Color (16 bytes) ====================
    // Iris CommonUniforms.java:167 - skyColor
    float3 skyColor; // CPU-calculated sky color [IMPORTANT: Use this!]
    float  _pad0;

    // ==================== Row 1: Player State Flags (16 bytes) ====================
    // Iris CommonUniforms.java:136-138
    int hideGUI; // Line 136: GUI hidden (F1 toggle)
    int isRightHanded; // Line 137: Main hand is RIGHT
    int isEyeInWater; // Line 138: 0=air, 1=water, 2=lava, 3=powder_snow
    int _pad1;

    // ==================== Row 2: Player Visual Effects (16 bytes) ====================
    // Iris CommonUniforms.java:139-142
    float blindness; // Line 139: Blindness effect (0.0-1.0)
    float darknessFactor; // Line 140: Darkness effect (0.0-1.0)
    float darknessLightFactor; // Line 141: Darkness light factor (0.0-1.0)
    float nightVision; // Line 142: Night vision effect (0.0-1.0)

    // ==================== Row 3: Player Action Flags (16 bytes) ====================
    // Iris CommonUniforms.java:143-146
    int is_sneaking; // Line 143: Player is crouching
    int is_sprinting; // Line 144: Player is sprinting
    int is_hurt; // Line 145: Player hurtTime > 0
    int is_invisible; // Line 146: Player is invisible

    // ==================== Row 4: Player Action Flags 2 (16 bytes) ====================
    // Iris CommonUniforms.java:147-148, 151
    int   is_burning; // Line 147: Player is on fire
    int   is_on_ground; // Line 148: Player is on ground
    float screenBrightness; // Line 151: Gamma setting (0.0+)
    float _pad2;

    // ==================== Row 5: Player Mood (16 bytes) ====================
    // Iris CommonUniforms.java:158-159
    float playerMood; // Line 158: Player mood (0.0-1.0)
    float constantMood; // Line 159: Constant mood baseline (0.0-1.0)
    float _pad3;
    float _pad4;

    // ==================== Row 6: Eye Brightness (16 bytes) ====================
    // Iris CommonUniforms.java:160-161
    // eyeBrightness: x = block light * 16, y = sky light * 16 (range 0-240)
    int eyeBrightnessX; // Line 160: Block light
    int eyeBrightnessY; // Line 160: Sky light
    int eyeBrightnessSmoothX; // Line 161: Smoothed block light
    int eyeBrightnessSmoothY; // Line 161: Smoothed sky light

    // ==================== Row 7: Weather (16 bytes) ====================
    // Iris CommonUniforms.java:165-166
    float rainStrength; // Line 165: Rain intensity (0.0-1.0)
    float wetness; // Line 166: Wetness factor (smoothed rainStrength)
    float _pad5;
    float _pad6;

    // ==================== Row 8: Render Stage (16 bytes) ====================
    // Iris CommonUniforms.java:108 (addDynamicUniforms)
    int renderStage; // WorldRenderingPhase ordinal
    int _pad7[3];
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
 *     // Use renderStage to differentiate sky dome vs sunset strip
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
 *     finalColor = lerp(finalColor, float3(0.5, 0.5, 0.6), rainStrength * 0.5);
 *
 *     // Apply player effects
 *     finalColor *= (1.0 - blindness);
 *     finalColor += nightVision * 0.1;
 *
 *     // Underwater effect
 *     if (isEyeInWater == EYE_IN_WATER)
 *     {
 *         finalColor *= float3(0.2, 0.5, 0.8); // Blue tint
 *     }
 *
 *     return float4(finalColor, 1.0);
 * }
 */
