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

    // ==========================================================================
    // [FIX] Sky Geometry Transform - Follow Camera Rotation, Ignore Translation
    // ==========================================================================
    // Problem: Sky sphere was fixed in world space, player could "fly out" of it
    // Solution: Remove translation from gbufferModelView, keep only rotation
    //
    // HLSL Matrix Layout (row-major with row-vector multiplication):
    // - mul(matrix, vector) = row-vector × matrix
    // - Translation is in the 4th COLUMN: matrix[0][3], matrix[1][3], matrix[2][3]
    // - matrix[3] is the 4th ROW, NOT the translation!
    //
    // To remove translation, we must zero out the 4th column elements:
    // ==========================================================================

    // [STEP 1] Create rotation-only view matrix by zeroing translation column
    float4x4 viewRotationOnly = gbufferModelView;

    viewRotationOnly[0][3] = 0.0; // Zero X translation (in row 0, column 3)
    viewRotationOnly[1][3] = 0.0; // Zero Y translation (in row 1, column 3)
    viewRotationOnly[2][3] = 0.0; // Zero Z translation (in row 2, column 3)
    // viewRotationOnly[3] remains as (0,0,0,1) - homogeneous coordinate row

    // [STEP 2] Transform sky geometry (rotation only, no translation)
    // Sky geometry is centered at origin, rotation makes it follow camera orientation
    float4 worldPos  = float4(input.Position, 1.0);
    float4 cameraPos = mul(viewRotationOnly, worldPos);

    // [STEP 3] Camera to Render Transform (Player rotation)
    float4 renderPos = mul(cameraToRenderTransform, cameraPos);

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
