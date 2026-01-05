/**
 * @file gbuffers_skytextured.vs.hlsl
 * @brief Sun/Moon Billboard Rendering - Vertex Shader (Minecraft Vanilla Style)
 * @date 2025-12-08
 *
 * [REFACTOR] Simplified to match Minecraft vanilla architecture:
 * - CPU calculates celestial rotation matrix (uploaded via PerObjectUniforms.modelMatrix)
 * - VS only applies matrix transform chain (like vanilla position_tex.vsh)
 * - No GPU-side billboard calculation or celestialType branching
 *
 * Reference:
 * - Minecraft position_tex.vsh: gl_Position = ProjMat * ModelViewMat * vec4(Position, 1.0)
 * - Iris CelestialUniforms.java:119-133 getCelestialPosition()
 *
 * Transform Chain:
 * 1. modelMatrix: Celestial rotation (YP(-90) × ZP(sunPathRotation) × XP(skyAngle×360))
 * 2. gbufferModelView: View rotation ONLY (translation removed at line 141)
 * 3. cameraToRenderTransform: Coordinate system conversion
 * 4. gbufferProjection: Projection to clip space
 */

#include "../core/Common.hlsl"

// [RENDERTARGETS] 0
// Output to colortex0 (sky color with sun/moon)

/**
 * @brief Vertex Shader Main Entry - Minecraft Vanilla Architecture
 * @param input Vertex data (Position = quad vertices in XY plane, UV = texture coords)
 * @return VSOutput Transformed billboard vertex
 *
 * Input.Position: Fixed quad vertices (e.g., Sun 30×30 units, Moon 20×20 units)
 * Input.TexCoord: UV coordinates (0,0) to (1,1)
 */
VSOutput main(VSInput input)
{
    VSOutput output;

    // [STEP 1] Apply celestial rotation (CPU-calculated, uploaded via PerObjectUniforms)
    // This matrix rotates the quad to the correct sky position based on time of day
    float4 rotatedPos = mul(modelMatrix, float4(input.Position, 1.0));

    // [STEP 2] Apply view rotation ONLY (translation removed - see SkyRenderPass.cpp:141)
    // This ensures sun/moon stay at infinite distance, unaffected by player movement
    float4 viewPos = mul(gbufferView, rotatedPos);

    // [STEP 3] Apply camera-to-render coordinate transform
    float4 renderPos = mul(gbufferRenderer, viewPos);

    // [STEP 4] Apply projection
    float4 clipPos = mul(gbufferProjection, renderPos);

    // [FIX] Force far plane depth (z = 1.0 in NDC)
    // Sun/moon render behind all geometry
    clipPos.z = clipPos.w; // z/w = 1.0 after perspective divide

    // [OUTPUT] Pass through interpolated data
    output.Position  = clipPos;
    output.Color     = input.Color;
    output.TexCoord  = input.TexCoord;
    output.Normal    = input.Normal;
    output.Tangent   = input.Tangent;
    output.Bitangent = input.Bitangent;
    output.WorldPos  = viewPos.xyz; // View space position (for debugging)

    return output;
}
