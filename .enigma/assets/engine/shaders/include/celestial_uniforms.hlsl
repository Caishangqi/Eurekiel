/**
 * @file celestial_uniforms.hlsl
 * @brief Celestial Data Constant Buffer - For Sky and Cloud Rendering
 * @date 2025-11-26
 * @version v1.0
 *
 * Key Points:
 * 1. Uses cbuffer to store celestial angles, cloud time, sun/moon positions
 * 2. Register b15 - User-defined area (>= 15)
 * 3. PerFrame update frequency - Updated once per frame
 * 4. Field order must match CelestialConstantBuffer.hpp exactly
 * 5. float3 maps to C++ Vec3 (HLSL compiler handles padding automatically)
 *
 * Architecture Benefits:
 * - High performance: Root CBV direct access, no indirection
 * - Memory aligned: 16-byte aligned, GPU friendly
 * - Minecraft compatible: Celestial angle system follows vanilla spec
 *
 * Usage Scenarios:
 * - gbuffers_skybasic.hlsl - Basic sky rendering
 * - gbuffers_skytextured.hlsl - Sun/moon texture rendering
 * - gbuffers_clouds.hlsl - Cloud rendering
 * - composite.ps.hlsl - Post-processing composition
 *
 * C++ Counterpart: Code/Game/Framework/RenderPass/ConstantBuffer/CelestialConstantBuffer.hpp
 */

/**
 * @brief CelestialUniforms - Celestial Data Constant Buffer (48 bytes)
 *
 * Field Descriptions:
 * - celestialAngle: Celestial angle (0.0-1.0), maps to Minecraft worldTime % 24000
 *   - 0.0 = Sunrise (tick 0)
 *   - 0.25 = Noon (tick 6000)
 *   - 0.5 = Sunset (tick 12000)
 *   - 0.75 = Midnight (tick 18000)
 *
 * - compensatedCelestialAngle: Compensated angle (celestialAngle + 0.25)
 *   - Used for smooth sunrise/sunset transitions
 *   - Prevents sudden sky color changes
 *
 * - cloudTime: Cloud time (tick * 0.03)
 *   - Used for cloud animation offset
 *   - Controls cloud movement speed
 *
 * - sunPosition: Sun position (world coordinates)
 *   - Calculated from celestialAngle via matrix transforms
 *   - Used for lighting and sunrise/sunset gradients
 *
 * - moonPosition: Moon position (world coordinates)
 *   - Opposite to sun position (180 degree offset)
 *   - Used for night lighting
 *
 * Memory Layout (16-byte aligned):
 * [0-15]   celestialAngle, compensatedCelestialAngle, cloudTime, padding
 * [16-31]  sunPosition (float3), padding
 * [32-47]  moonPosition (float3), padding
 */
cbuffer CelestialUniforms: register(b15)
{
    float  celestialAngle;             // Celestial angle (0.0-1.0)
    float  compensatedCelestialAngle;  // Compensated angle (+0.25)
    float  cloudTime;                  // Cloud time (tick * 0.03)
    // float _padding1;                // [REMOVED] HLSL compiler auto-pads, no explicit declaration needed

    float3 sunPosition;                // Sun position (world coordinates)
    // float _padding2;                // [REMOVED] HLSL compiler auto-pads

    float3 moonPosition;               // Moon position (world coordinates)
    // float _padding3;                // [REMOVED] HLSL compiler auto-pads
};


/**
 * Usage Example:
 *
 * // In gbuffers_skytextured.ps.hlsl:
 * #include "include/celestial_uniforms.hlsl"
 *
 * float4 main(PSInput input) : SV_TARGET
 * {
 *     // Calculate sky color based on celestial angle
 *     float sunFactor = saturate(1.0 - abs(celestialAngle - 0.25) * 4.0);
 *     float3 skyColor = lerp(nightColor, dayColor, sunFactor);
 *
 *     // Use sun position for lighting calculations
 *     float3 lightDir = normalize(sunPosition);
 *
 *     return float4(skyColor, 1.0);
 * }
 */
