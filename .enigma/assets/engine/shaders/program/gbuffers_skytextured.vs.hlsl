/**
 * @file gbuffers_skytextured.vs.hlsl
 * @brief Sun/Moon Billboard Rendering - Vertex Shader (Minecraft Style)
 * @date 2025-11-28
 *
 * [FIX] sunPosition/moonPosition are VIEW SPACE DIRECTION VECTORS (w=0)
 * Reference: Iris CelestialUniforms.java - Vector4f(0, y, 0, 0) with w=0
 *
 * The direction vector points from camera toward the celestial body.
 * Billboard is rendered at this direction, always facing the camera.
 *
 * Billboard Algorithm (View Space):
 * 1. Get celestial direction in view space from CelestialUniforms
 * 2. Calculate billboard right/up axes perpendicular to view direction
 * 3. Apply billboard offset in view space
 * 4. Project to clip space via gbufferProjection
 */

#include "../core/Common.hlsl"
#include "../include/celestial_uniforms.hlsl"

// [RENDERTARGETS] 0
// Output to colortex0 (sky color with sun/moon)

/**
 * @brief Vertex Shader Main Entry
 * @param input Vertex data (Position contains billboard offset in XY, celestial type in Z)
 * @return VSOutput Transformed billboard vertex
 *
 * Input.Position Encoding:
 * - Position.xy: Billboard offset (-0.5 to 0.5) relative to center
 * - Position.z: Celestial type (0 = Sun, 1 = Moon)
 */
VSOutput main(VSInput input)
{
    VSOutput output;

    // [STEP 1] Get Sun/Moon Direction from CelestialUniforms
    // These are VIEW SPACE direction vectors (length ~100)
    float  celestialType = input.Position.z; // 0 = Sun, 1 = Moon
    float3 celestialDir  = lerp(sunPosition, moonPosition, celestialType);

    // [STEP 2] Calculate Billboard axes in VIEW SPACE
    // The billboard should face the camera (which is at origin in view space)
    // Forward direction is from billboard toward camera = -normalize(celestialDir)
    float3 forward = -normalize(celestialDir);

    // Calculate right and up vectors for billboard
    // Use world up (0,1,0) in view space as reference, but handle edge cases
    float3 worldUp = float3(0.0, 1.0, 0.0);
    float3 right;
    float3 up;

    // Check if forward is nearly parallel to worldUp
    if (abs(dot(forward, worldUp)) < 0.999)
    {
        right = normalize(cross(worldUp, forward));
        up    = cross(forward, right);
    }
    else
    {
        // Edge case: use world right as fallback
        float3 worldRight = float3(1.0, 0.0, 0.0);
        up                = normalize(cross(forward, worldRight));
        right             = cross(up, forward);
    }

    // [STEP 3] Calculate Billboard View Position
    // celestialDir is already the center position in view space
    float  billboardSize   = 30.0; // Billboard size in view units
    float3 billboardOffset = right * input.Position.x * billboardSize
        + up * input.Position.y * billboardSize;
    float3 billboardViewPos = celestialDir + billboardOffset;

    // [STEP 4] Apply Transform Chain (View Space -> Clip Space)
    float4 viewPos = float4(billboardViewPos, 1.0);

    // Apply Camera to Render Transform (Player rotation)
    float4 renderPos = mul(cameraToRenderTransform, viewPos);

    // Apply Projection
    float4 clipPos = mul(gbufferProjection, renderPos);

    // [FIX] Force Far Plane: z = 1.0 in NDC
    // Sun/Moon should render behind all geometry
    clipPos.z = clipPos.w; // z/w = 1.0 in NDC

    output.Position  = clipPos;
    output.Color     = input.Color;
    output.TexCoord  = input.TexCoord;
    output.Normal    = input.Normal;
    output.Tangent   = input.Tangent;
    output.Bitangent = input.Bitangent;
    output.WorldPos  = billboardViewPos;

    return output;
}
