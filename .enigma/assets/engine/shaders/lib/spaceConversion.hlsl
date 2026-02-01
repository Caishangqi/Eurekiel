/**
 * @file spaceConversion.hlsl
 * @brief Space transformation math library - No cbuffer dependencies
 *
 * Design: Pure functions with explicit matrix parameters
 * Usage: #include "../lib/spaceConversion.hlsl"
 *
 * Coordinate Spaces:
 * - Screen Space: UV [0,1] x [0,1], Depth [0,1]
 * - NDC (Normalized Device Coordinates): XY [-1,1] x [-1,1], Z [0,1] (DX12)
 * - View Space: Camera at origin, -Z forward
 * - World Space: Global coordinates
 */

#ifndef LIB_SPACE_CONVERSION
#define LIB_SPACE_CONVERSION

// =============================================================================
// [PROJECTION UTILITIES]
// =============================================================================

/**
 * @brief Apply projection matrix and perform perspective divide
 * @param position Position to project (typically view-space)
 * @param projMatrix Projection matrix
 * @return NDC coordinates after perspective divide
 */
float3 ProjectAndDivide(float3 position, float4x4 projMatrix)
{
    float4 clipPos = mul(projMatrix, float4(position, 1.0));
    return clipPos.xyz / clipPos.w;
}

// =============================================================================
// [SPACE CONVERSIONS]
// =============================================================================

/**
 * @brief Convert NDC position to view space
 * @param ndcPos NDC position (xy: [-1,1], z: [0,1] for DX12)
 * @param projMatrixInverse Inverse projection matrix (gbufferProjectionInverse)
 * @param rendererMatrixInverse Inverse renderer matrix (gbufferRendererInverse) - DX12 API correction
 * @return View-space position
 *
 * Note: DX12 uses Z range [0,1], OpenGL uses [-1,1]
 * Note: gbufferRendererInverse is required for DX12 API coordinate system correction
 *       (different rendering APIs have different XYZ conventions)
 */
float3 NDCToView(float3 ndcPos, float4x4 projMatrixInverse, float4x4 rendererMatrixInverse)
{
    // Step 1: NDC -> Render space (undo projection)
    float4 renderPos = mul(projMatrixInverse, float4(ndcPos, 1.0));
    renderPos.xyz    /= renderPos.w;

    // Step 2: Render space -> View space (undo DX12 API correction)
    float4 viewPos = mul(rendererMatrixInverse, float4(renderPos.xyz, 1.0));
    return viewPos.xyz;
}

/**
 * @brief Convert view-space position to NDC
 * @param viewPos View-space position
 * @param projMatrix Projection matrix
 * @return NDC position
 */
float3 ViewToNDC(float3 viewPos, float4x4 projMatrix)
{
    return ProjectAndDivide(viewPos, projMatrix);
}

/**
 * @brief Convert screen UV + depth to NDC
 * @param uv Screen UV coordinates [0,1]
 * @param depth Depth buffer value [0,1]
 * @return NDC position
 */
float3 ScreenToNDC(float2 uv, float depth)
{
    // UV [0,1] -> NDC [-1,1], flip Y for DX convention
    float2 ndcXY = uv * 2.0 - 1.0;
    ndcXY.y      = -ndcXY.y; // Flip Y: UV origin is top-left, NDC origin is bottom-left
    return float3(ndcXY, depth);
}

// =============================================================================
// [VIEW POSITION RECONSTRUCTION]
// =============================================================================

/**
 * @brief Reconstruct view-space position from screen UV and depth
 * @param uv Screen UV coordinates [0,1]
 * @param depth Depth buffer value [0,1]
 * @param projMatrixInverse Inverse projection matrix (gbufferProjectionInverse)
 * @param rendererMatrixInverse Inverse renderer matrix (gbufferRendererInverse) - DX12 API correction
 * @return View-space position
 *
 * Usage in composite.ps.hlsl:
 *   float3 viewPos = ReconstructViewPosition(input.uv, depthAll,
 *                                            gbufferProjectionInverse, gbufferRendererInverse);
 *   float viewDistance = length(viewPos);
 *
 * Reference: Common pattern in deferred rendering for fog/lighting calculations
 */
float3 ReconstructViewPosition(float2 uv, float depth, float4x4 projMatrixInverse, float4x4 rendererMatrixInverse)
{
    float3 ndcPos = ScreenToNDC(uv, depth);
    return NDCToView(ndcPos, projMatrixInverse, rendererMatrixInverse);
}

/**
 * @brief Calculate view-space distance from camera
 * @param uv Screen UV coordinates [0,1]
 * @param depth Depth buffer value [0,1]
 * @param projMatrixInverse Inverse projection matrix (gbufferProjectionInverse)
 * @param rendererMatrixInverse Inverse renderer matrix (gbufferRendererInverse) - DX12 API correction
 * @return Distance from camera (length of view-space position)
 *
 * Convenience function for fog calculations
 */
float GetViewDistance(float2 uv, float depth, float4x4 projMatrixInverse, float4x4 rendererMatrixInverse)
{
    float3 viewPos = ReconstructViewPosition(uv, depth, projMatrixInverse, rendererMatrixInverse);
    return length(viewPos);
}

#endif // LIB_MATH_HLSL
