/**
 * @file gbuffers_skybasic.vs.hlsl
 * @brief Sky with Void Gradient - Vertex Shader (Minecraft Style)
 * @date 2025-11-26
 *
 * Features:
 * - Renders sky dome as 2-triangle TRIANGLE_FAN (10 vertices)
 * - Uses MatricesUniforms 4-stage transform chain
 * - Forces far plane: z = 1.0 in NDC
 *
 * Transform Chain:
 * 1. World → Camera: gbufferModelView
 * 2. Camera → Render: cameraToRenderTransform
 * 3. Render → Clip: gbufferProjection
 * 4. Far plane override: z = 1.0
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

    // [STEP 1] World → Camera Transform
    float4 worldPos = float4(input.Position, 1.0);
    float4 cameraPos = mul(worldPos, gbufferModelView);

    // [STEP 2] Camera → Render Transform (Player rotation)
    float4 renderPos = mul(cameraPos, cameraToRenderTransform);

    // [STEP 3] Render → Clip Transform (Projection)
    float4 clipPos = mul(renderPos, gbufferProjection);

    // [STEP 4] Force Far Plane: z = 1.0 in NDC
    // This makes sky render behind all geometry
    clipPos.z = clipPos.w; // z/w = 1.0 → NDC depth

    output.Position = clipPos;
    output.Color = input.Color;
    output.TexCoord = input.TexCoord;
    output.Normal = input.Normal;
    output.Tangent = input.Tangent;
    output.Bitangent = input.Bitangent;
    output.WorldPos = worldPos.xyz;

    return output;
}
