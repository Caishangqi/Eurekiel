/**
 * @file gbuffers_skybasic.vs.hlsl
 * @brief Sky with Void Gradient - Vertex Shader (Minecraft Style)
 * @date 2025-11-30
 *
 * [SIMPLIFIED] CPU-side vertex transformation approach (following Minecraft/Iris)
 * Vertices are pre-transformed on CPU with pure rotation matrix, GPU only does Projection
 *
 * Features:
 * - Renders sky dome as TRIANGLE_LIST (24 vertices)
 * - Vertices arrive already in camera space (CPU-side transformed)
 * - Forces far plane: z = 1.0 in NDC
 *
 * Transform Chain (simplified):
 * 1. Render Position (input.Position - already transformed on CPU including cameraToRenderTransform)
 * 2. Render to Clip: gbufferProjection
 * 3. Far plane override: z = 1.0
 *
 * [OLD] Previous GPU-side approach (removed):
 * - modelMatrix, viewRotationOnly construction removed
 * - CPU now handles: modelMatrix * skyViewMatrix transformation
 */

#include "../core/Common.hlsl"

// [RENDERTARGETS] 0
// Output to colortex0 (sky color)

/**
 * @brief Vertex Shader Main Entry
 * @param input Vertex data from sky dome geometry (pre-transformed on CPU)
 * @return VSOutput Transformed vertex for rasterization
 */
VSOutput main(VSInput input)
{
    VSOutput output;

    // ==========================================================================
    // [SIMPLIFIED] CPU-Side Vertex Transform Approach
    // ==========================================================================
    // Following Minecraft/Iris approach:
    // - Vertices are transformed on CPU with pure rotation matrix (no translation)
    // - GPU only applies Projection transform
    //
    // CPU-side transformation (in SkyRenderPass.cpp):
    // - Sky Dome: TransformVertexArray3D(vertices, skyViewMatrix * cameraToRenderTransform)
    // - Strip: TransformVertexArray3D(vertices, celestialRotation * skyViewMatrix * cameraToRenderTransform)
    //
    // Transform chain:
    // - CPU: localPos → (celestialRotation) → skyViewMatrix → cameraToRenderTransform → renderPos
    // - GPU: renderPos → gbufferProjection → clipPos
    //
    // Benefits:
    // - Simpler shader (no viewRotationOnly construction, no cameraToRenderTransform)
    // - Matches Minecraft/Iris implementation exactly
    // - modelMatrix is IDENTITY, so we can skip it entirely
    // ==========================================================================

    // [STEP 1] Input position is already in RENDER space (CPU pre-transformed with cameraToRenderTransform)
    float4 renderPos = float4(input.Position, 1.0);

    // [STEP 2] Render to Clip Transform (Projection only)
    // Note: cameraToRenderTransform is already applied on CPU side
    float4 clipPos = mul(gbufferProjection, renderPos);

    // [STEP 3] Force Far Plane: z = 1.0 in NDC
    // This makes sky render behind all geometry
    clipPos.z = clipPos.w; // z/w = 1.0 in NDC

    output.Position  = clipPos;
    output.Color     = input.Color;
    output.TexCoord  = input.TexCoord;
    output.Normal    = input.Normal;
    output.Tangent   = input.Tangent;
    output.Bitangent = input.Bitangent;
    output.WorldPos  = renderPos.xyz; // [NOTE] Now in render space (DirectX coordinate system)

    return output;
}
