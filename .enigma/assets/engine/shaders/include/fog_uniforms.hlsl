/**
 * @file fog_uniforms.hlsl
 * @brief Fog Uniforms Constant Buffer - Iris FogUniforms.java Compatible
 * @date 2026-01-22
 * @version v1.0
 *
 * [Iris Reference]
 *   Source: FogUniforms.java (lines 19-53)
 *   Path: Iris-multiloader-new/common/src/main/java/net/irisshaders/iris/uniforms/FogUniforms.java
 *
 * [Iris Fields]
 *   Line 21/24: fogMode     - Fog mode (0=off, 9729=linear, 2048=exp, 2049=exp2)
 *   Line 22/36: fogShape    - Fog shape (0=sphere, 1=cylinder, -1=disabled)
 *   Line 39:    fogDensity  - Fog density for exp/exp2 modes
 *   Line 45:    fogStart    - Linear fog start distance
 *   Line 47:    fogEnd      - Linear fog end distance
 *   Line 51:    fogColor    - Fog color (RGB)
 *
 * [Memory Layout] 32 bytes (2 rows x 16 bytes)
 *   Row 0 [0-15]:  fogColor (float3), fogDensity
 *   Row 1 [16-31]: fogStart, fogEnd, fogMode, fogShape
 *
 * C++ Counterpart: Code/Game/Framework/RenderPass/ConstantBuffer/FogUniforms.hpp
 */

// =============================================================================
// [FOG MODE MACROS] OpenGL Fog Mode Constants
// =============================================================================
// These match OpenGL fog mode enum values used by Minecraft/Iris
// =============================================================================

#define FOG_MODE_OFF     0      // Fog disabled
#define FOG_MODE_LINEAR  9729   // GL_LINEAR - linear interpolation
#define FOG_MODE_EXP     2048   // GL_EXP - exponential falloff
#define FOG_MODE_EXP2    2049   // GL_EXP2 - exponential squared falloff

// =============================================================================
// [FOG SHAPE MACROS] Fog Shape Constants
// =============================================================================

#define FOG_SHAPE_DISABLED -1   // Fog shape disabled
#define FOG_SHAPE_SPHERE    0   // Spherical fog (default)
#define FOG_SHAPE_CYLINDER  1   // Cylindrical fog

// =============================================================================
// FogUniforms Constant Buffer (32 bytes = 2 rows x 16 bytes)
// =============================================================================

cbuffer FogUniforms : register(b2, space1)
{
    // ==================== Row 0: Fog Color + Density (16 bytes) ====================
    // @iris fogColor (FogUniforms.java:51-53)
    // RGB fog color from RenderSystem.getShaderFogColor()
    // Underwater: Minecraft sets this to blue tint
    float3 fogColor;

    // @iris fogDensity (FogUniforms.java:39-43)
    // Fog density for exponential fog modes (0.0+)
    float fogDensity;

    // ==================== Row 1: Fog Parameters (16 bytes) ====================
    // @iris fogStart (FogUniforms.java:45)
    // Linear fog start distance
    float fogStart;

    // @iris fogEnd (FogUniforms.java:47)
    // Linear fog end distance (fully opaque)
    float fogEnd;

    // @iris fogMode (FogUniforms.java:21/24-34)
    // Use FOG_MODE_* macros
    int fogMode;

    // @iris fogShape (FogUniforms.java:22/36)
    // Use FOG_SHAPE_* macros
    int fogShape;
};

// =============================================================================
// [HELPER FUNCTIONS] Fog Calculation Utilities
// =============================================================================

/**
 * @brief Calculate linear fog factor
 * @param distance Distance from camera to fragment
 * @return Fog factor (0.0 = no fog, 1.0 = full fog)
 */
float CalculateLinearFog(float distance)
{
    return saturate((distance - fogStart) / (fogEnd - fogStart));
}

/**
 * @brief Calculate exponential fog factor
 * @param distance Distance from camera to fragment
 * @return Fog factor (0.0 = no fog, 1.0 = full fog)
 */
float CalculateExpFog(float distance)
{
    return 1.0 - exp(-fogDensity * distance);
}

/**
 * @brief Calculate exponential squared fog factor
 * @param distance Distance from camera to fragment
 * @return Fog factor (0.0 = no fog, 1.0 = full fog)
 */
float CalculateExp2Fog(float distance)
{
    float f = fogDensity * distance;
    return 1.0 - exp(-f * f);
}

/**
 * @brief Calculate fog factor based on current fog mode
 * @param distance Distance from camera to fragment
 * @return Fog factor (0.0 = no fog, 1.0 = full fog)
 */
float CalculateFogFactor(float distance)
{
    if (fogMode == FOG_MODE_OFF)
    {
        return 0.0;
    }
    else if (fogMode == FOG_MODE_LINEAR)
    {
        return CalculateLinearFog(distance);
    }
    else if (fogMode == FOG_MODE_EXP)
    {
        return CalculateExpFog(distance);
    }
    else if (fogMode == FOG_MODE_EXP2)
    {
        return CalculateExp2Fog(distance);
    }
    return 0.0;
}

/**
 * @brief Apply fog to a color
 * @param color Original fragment color
 * @param distance Distance from camera to fragment
 * @return Color with fog applied
 */
float3 ApplyFog(float3 color, float distance)
{
    float fogFactor = CalculateFogFactor(distance);
    return lerp(color, fogColor, fogFactor);
}

/**
 * Usage Example:
 *
 * #include "../include/fog_uniforms.hlsl"
 *
 * float4 main(PSInput input) : SV_TARGET
 * {
 *     float3 baseColor = SampleTexture(input.uv);
 *
 *     // Calculate distance from camera
 *     float dist = length(input.worldPos - cameraPosition);
 *
 *     // Apply fog
 *     float3 finalColor = ApplyFog(baseColor, dist);
 *
 *     return float4(finalColor, 1.0);
 * }
 */
