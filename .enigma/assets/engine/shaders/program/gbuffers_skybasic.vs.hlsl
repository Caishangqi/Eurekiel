/**
 * @file gbuffers_skybasic.vs.hlsl
 * @brief Sky with Void Gradient - Vertex Shader (Minecraft Style)
 * @date 2025-11-28
 *
 * [FIX] Sky geometry now follows camera rotation but ignores translation
 * This ensures sky always surrounds the player regardless of position
 *
 * Features:
 * - Renders sky dome as TRIANGLE_LIST (24 vertices)
 * - Removes translation from gbufferModelView (rotation only)
 * - Forces far plane: z = 1.0 in NDC
 *
 * Transform Chain:
 * 1. World Position (sky geometry centered at origin)
 * 2. Apply rotation only from gbufferModelView (ignore translation)
 * 3. Camera to Render: cameraToRenderTransform
 * 4. Render to Clip: gbufferProjection
 * 5. Far plane override: z = 1.0
 */

#include "../core/Common.hlsl"

// [RENDERTARGETS] 0
// Output to colortex0 (sky color)

/**
 * @brief Vertex Shader Main Entry
 * @param input Vertex data from sky dome geometry
 * @return VSOutput Transformed vertex for rasterization
 */
VSOutput main(VSInput input)
{
    VSOutput output;

    // Sky geometry is centered at origin gbufferModelView already transformed in CPU, rotation makes it follow camera orientation
    float4 worldPos  = float4(input.Position, 1.0);
    float4 cameraPos = mul(gbufferView, worldPos);

    // [STEP 3] Camera to Render Transform (Player rotation)
    float4 renderPos = mul(gbufferRenderer, cameraPos);

    // [STEP 4] Render to Clip Transform (Projection)
    float4 clipPos = mul(gbufferProjection, renderPos);

    // [STEP 5] Force Far Plane: z = 1.0 in NDC
    // This makes sky render behind all geometry
    clipPos.z = clipPos.w; // z/w = 1.0 in NDC

    output.Position  = clipPos;
    output.Color     = input.Color;
    output.TexCoord  = input.TexCoord;
    output.Normal    = input.Normal;
    output.Tangent   = input.Tangent;
    output.Bitangent = input.Bitangent;
    output.WorldPos  = worldPos.xyz;

    return output;
}
