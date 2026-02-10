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

// =============================================================================
// [SHADOW SPACE CONVERSIONS]
// =============================================================================

/**
 * @brief Transform world position to shadow UV coordinates
 * @param worldPos World-space position
 * @param shadowViewMatrix Shadow view matrix (shadowView)
 * @param rendererMatrix Renderer matrix (gbufferRenderer) - DX12 API correction
 * @param shadowProjMatrix Shadow projection matrix (shadowProjection)
 * @return Shadow UV coordinates (xy: [0,1], z: depth for comparison)
 *
 * Transform chain (must match shadow.vs.hlsl):
 *   World -> ShadowView -> gbufferRenderer -> ShadowProjection -> NDC -> UV
 *
 * Usage in composite.ps.hlsl:
 *   float3 shadowUV = WorldToShadowUV(worldPos, shadowView, gbufferRenderer, shadowProjection);
 *   float shadowDepth = shadowtex1.Sample(sampler, shadowUV.xy).r;
 *   bool inShadow = shadowUV.z > shadowDepth;
 */
float3 WorldToShadowUV(float3 worldPos, float4x4 shadowViewMatrix, float4x4 rendererMatrix, float4x4 shadowProjMatrix)
{
    // [STEP 1] World -> Shadow View space
    float4 shadowViewPos = mul(shadowViewMatrix, float4(worldPos, 1.0));

    // [STEP 2] Shadow View -> Render space (DX12 API correction)
    // CRITICAL: This must match shadow.vs.hlsl transform chain!
    float4 shadowRenderPos = mul(rendererMatrix, shadowViewPos);

    // [STEP 3] Render -> Shadow Clip space
    float4 shadowClipPos = mul(shadowProjMatrix, shadowRenderPos);

    // [STEP 4] Perspective divide -> NDC
    float3 shadowNDC = shadowClipPos.xyz / shadowClipPos.w;

    // [STEP 5] NDC -> UV (xy: [-1,1] -> [0,1], z: depth unchanged)
    float2 shadowUV = shadowNDC.xy * 0.5 + 0.5;
    shadowUV.y      = 1.0 - shadowUV.y; // Flip Y for texture sampling

    return float3(shadowUV, shadowNDC.z);
}


#endif // LIB_SPACE_CONVERSION
