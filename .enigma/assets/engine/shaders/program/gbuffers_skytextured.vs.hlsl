/**
 * @file gbuffers_skytextured.vs.hlsl
 * @brief Sun/Moon Billboard Rendering - Vertex Shader (Minecraft Style)
 * @date 2025-11-26
 *
 * Features:
 * - Renders sun/moon as camera-facing billboards
 * - Extracts camera orientation from gbufferModelViewInverse
 * - Uses CelestialUniforms for sun/moon positions
 * - No CPU-side Billboard API (GPU-only calculation)
 *
 * Billboard Algorithm:
 * 1. Extract camera right/up vectors from gbufferModelViewInverse columns 0/1
 * 2. Get sun/moon center position from CelestialUniforms
 * 3. Calculate billboard world position: center + right*offsetX + up*offsetY
 * 4. Apply 4-stage transform chain to Clip space
 *
 * Transform Chain:
 * 1. Billboard World Position (calculated in this shader)
 * 2. World � Camera: gbufferModelView
 * 3. Camera � Render: cameraToRenderTransform
 * 4. Render to Clip: gbufferProjection
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

    // [STEP 1] Extract Camera Orientation from gbufferModelViewInverse
    // Column-major layout: Column 0 = Right, Column 1 = Up, Column 2 = Forward
    float3 cameraRight = gbufferModelViewInverse[0].xyz;
    float3 cameraUp = gbufferModelViewInverse[1].xyz;

    // [STEP 2] Get Sun/Moon Center Position from CelestialUniforms
    // Position.z determines which celestial body to render
    float celestialType = input.Position.z; // 0 = Sun, 1 = Moon
    float3 celestialCenter = lerp(sunPosition, moonPosition, celestialType);

    // [STEP 3] Calculate Billboard World Position
    // Billboard faces camera by using camera right/up vectors
    float billboardSize = 30.0; // Billboard size in world units
    float3 billboardOffset = cameraRight * input.Position.x * billboardSize
                           + cameraUp * input.Position.y * billboardSize;
    float3 billboardWorldPos = celestialCenter + billboardOffset;

    // [STEP 4] Apply 4-Stage Transform Chain
    // Stage 1: Billboard World Position (already calculated)
    float4 worldPos = float4(billboardWorldPos, 1.0);

    // Stage 2: World to Camera Transform
    float4 cameraPos = mul(gbufferModelView, worldPos);

    // Stage 3: Camera to Render Transform (Player rotation)
    float4 renderPos = mul(cameraToRenderTransform, cameraPos);

    // Stage 4: Render to Clip Transform (Projection)
    float4 clipPos = mul(gbufferProjection, renderPos);

    output.Position = clipPos;
    output.Color = input.Color;
    output.TexCoord = input.TexCoord;
    output.Normal = input.Normal;
    output.Tangent = input.Tangent;
    output.Bitangent = input.Bitangent;
    output.WorldPos = billboardWorldPos;

    return output;
}
