/**
 * @file celestial_uniforms.hlsl
 * @brief Celestial Uniforms - Iris Compatible (CelestialUniforms.java:74-82)
 * @date 2026-01-02
 * @version v2.0
 *
 * [Iris Reference]
 *   Source: CelestialUniforms.java:74-82
 *   Path: Iris-multiloader-new/common/src/main/java/net/irisshaders/iris/uniforms/CelestialUniforms.java
 *
 * [Iris Fields] (Line 74-82)
 *   - sunAngle (celestialAngle)
 *   - sunPosition
 *   - moonPosition
 *   - shadowAngle
 *   - shadowLightPosition
 *   - upPosition
 *
 * [Memory Layout] 96 bytes (6 rows x 16 bytes)
 *   Row 0 [0-15]:   celestialAngle, compensatedCelestialAngle, cloudTime, skyBrightness
 *   Row 1 [16-31]:  sunPosition.xyz, shadowAngle
 *   Row 2 [32-47]:  moonPosition.xyz, _padding2
 *   Row 3 [48-63]:  shadowLightPosition.xyz, _padding3
 *   Row 4 [64-79]:  upPosition.xyz, _padding4
 *   Row 5 [80-95]:  colorModulator.xyzw
 *
 * C++ Counterpart: Code/Game/Framework/RenderPass/ConstantBuffer/CelestialConstantBuffer.hpp
 */

cbuffer CelestialUniforms: register(b9, space1)
{
    // ==================== Row 0: Angle Data (16 bytes) ====================
    float celestialAngle; // @iris sunAngle (0.0-1.0)
    float compensatedCelestialAngle; // Compensated angle (+0.25)
    float cloudTime; // Cloud time (tick * 0.03)
    float skyBrightness; // Sky brightness (0.0-1.0)

    // ==================== Row 1: Sun Position (16 bytes) ====================
    // @iris sunPosition
    float3 sunPosition; // Sun direction (view space), length 100
    float  shadowAngle; // @iris shadowAngle (0.0-0.5)

    // ==================== Row 2: Moon Position (16 bytes) ====================
    // @iris moonPosition
    float3 moonPosition; // Moon direction (view space), length 100
    float  CelestialUniforms_padding2;

    // ==================== Row 3: Shadow Light Position (16 bytes) ====================
    // @iris shadowLightPosition
    // Logic: isDay() ? sunPosition : moonPosition (CelestialUniforms.java:93-95)
    float3 shadowLightPosition; // Shadow light source position
    float  CelestialUniforms_padding3;

    // ==================== Row 4: Up Position (16 bytes) ====================
    // @iris upPosition
    float3 upPosition; // Up direction (view space), length 100
    float  CelestialUniforms_padding4;

    // ==================== Row 5: Color Modulator (16 bytes) ====================
    // @iris colorModulator (ExternallyManagedUniforms.java:49)
    float4 colorModulator; // Color modulator (RGBA), default (1,1,1,1)
};

/**
 * Usage Example:
 *
 * #include "../include/celestial_uniforms.hlsl"
 *
 * float4 main(PSInput input) : SV_TARGET
 * {
 *     // Use shadowLightPosition for shadow calculations
 *     float3 lightDir = normalize(shadowLightPosition);
 *
 *     // shadowAngle for shadow intensity
 *     float shadowIntensity = saturate(shadowAngle * 2.0);
 *
 *     return float4(litColor, 1.0);
 * }
 */
