/**
 * @file fog.hlsl
 * @brief Standalone fog calculation library - No cbuffer dependencies
 *
 * Design: Pure functions with explicit parameters
 * Usage: #include "../lib/fog.hlsl"
 */

#ifndef LIB_FOG_HLSL
#define LIB_FOG_HLSL

// =============================================================================
// [FOG MODE CONSTANTS]
// =============================================================================
#define FOG_MODE_OFF     0
#define FOG_MODE_LINEAR  9729   // GL_LINEAR
#define FOG_MODE_EXP     2048   // GL_EXP
#define FOG_MODE_EXP2    2049   // GL_EXP2

// =============================================================================
// [FOG COLOR CONSTANTS] - Used for sky occlusion when in fluids
// =============================================================================
static const float3 LAVA_FOG_COLOR        = float3(0.6, 0.1, 0.0);
static const float3 POWDER_SNOW_FOG_COLOR = float3(0.9, 0.9, 0.95);

// =============================================================================
// [CORE FOG FUNCTIONS] - Pure, no cbuffer dependency
// =============================================================================

/**
 * @brief Linear fog factor calculation
 * @param distance Fragment distance from camera
 * @param fogStart Fog start distance
 * @param fogEnd Fog end distance (fully opaque)
 * @return Fog factor [0.0 = clear, 1.0 = full fog]
 */
float LinearFog(float distance, float fogStart, float fogEnd)
{
    return saturate((distance - fogStart) / (fogEnd - fogStart));
}

/**
 * @brief Exponential fog factor calculation
 * @param distance Fragment distance from camera
 * @param density Fog density coefficient
 * @return Fog factor [0.0 = clear, 1.0 = full fog]
 */
float ExpFog(float distance, float density)
{
    return 1.0 - exp(-density * distance);
}

/**
 * @brief Exponential squared fog factor calculation (Minecraft default)
 * @param distance Fragment distance from camera
 * @param density Fog density coefficient
 * @return Fog factor [0.0 = clear, 1.0 = full fog]
 */
float Exp2Fog(float distance, float density)
{
    float f = density * distance;
    return 1.0 - exp(-f * f);
}

/**
 * @brief Calculate fog factor based on mode
 * @param distance Fragment distance from camera
 * @param fogStart Linear fog start
 * @param fogEnd Linear fog end
 * @param density Exp/Exp2 density
 * @param fogMode FOG_MODE_* constant
 * @return Fog factor [0.0 = clear, 1.0 = full fog]
 */
float CalculateFog(float distance, float fogStart, float fogEnd, float density, int fogMode)
{
    if (fogMode == FOG_MODE_LINEAR)
        return LinearFog(distance, fogStart, fogEnd);
    else if (fogMode == FOG_MODE_EXP)
        return ExpFog(distance, density);
    else if (fogMode == FOG_MODE_EXP2)
        return Exp2Fog(distance, density);
    return 0.0; // FOG_MODE_OFF
}

/**
 * @brief Apply fog to color
 * @param color Original fragment color
 * @param fogColor Target fog color
 * @param fogFactor Fog intensity [0.0 - 1.0]
 * @return Fogged color
 */
float3 ApplyFog(float3 color, float3 fogColor, float fogFactor)
{
    return lerp(color, fogColor, fogFactor);
}

// =============================================================================
// [UNDERWATER FOG] - Minecraft-style underwater effect
// =============================================================================

/**
 * @brief Apply underwater fog effect (Minecraft vanilla style)
 * @param color Original fragment color
 * @param distance Fragment distance from camera
 * @param fogColor Underwater fog color (typically blue)
 * @param fogStart Underwater fog start distance
 * @param fogEnd Underwater fog end distance
 * @return Color with underwater fog applied
 *
 * Reference: Minecraft fog.glsl - linear_fog_value() + apply_fog()
 */
float3 ApplyUnderwaterFog(float3 color, float distance, float3 fogColor, float fogStart, float fogEnd)
{
    float fogFactor = 0;
    if (fogMode == FOG_MODE_LINEAR)
    {
        fogFactor = LinearFog(distance, fogStart, fogEnd);
    }
    else if (fogMode == FOG_MODE_EXP2)
    {
        fogFactor = Exp2Fog(distance, fogDensity);
    }
    else if (fogMode == FOG_MODE_EXP)
    {
        fogFactor = ExpFog(distance, fogDensity);
    }
    return ApplyFog(color, fogColor, fogFactor);
}

/**
 * @brief Apply lava fog effect
 * @param color Original fragment color
 * @param distance Fragment distance from camera
 * @return Color with lava fog applied (very short range, orange-red)
 */
float3 ApplyLavaFog(float3 color, float distance)
{
    // Lava has very short visibility (about 1-2 blocks)
    static const float LAVA_FOG_START = 0.0;
    static const float LAVA_FOG_END   = 2.0;

    float fogFactor = LinearFog(distance, LAVA_FOG_START, LAVA_FOG_END);
    return ApplyFog(color, LAVA_FOG_COLOR, fogFactor);
}

/**
 * @brief Apply powder snow fog effect
 * @param color Original fragment color
 * @param distance Fragment distance from camera
 * @return Color with powder snow fog applied (white, medium range)
 */
float3 ApplyPowderSnowFog(float3 color, float distance)
{
    static const float POWDER_SNOW_FOG_START = 0.0;
    static const float POWDER_SNOW_FOG_END   = 5.0;

    float fogFactor = LinearFog(distance, POWDER_SNOW_FOG_START, POWDER_SNOW_FOG_END);
    return ApplyFog(color, POWDER_SNOW_FOG_COLOR, fogFactor);
}

#endif // LIB_FOG_HLSL
