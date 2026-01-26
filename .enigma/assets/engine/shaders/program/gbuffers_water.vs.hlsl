/**
 * @file gbuffers_water.vs.hlsl
 * @brief Water Vertex Shader - TerrainVertex Layout (54 bytes)
 * @date 2026-01-17
 *
 * Input Layout (matches TerrainVertex):
 * - POSITION (float3, offset 0)
 * - COLOR (R8G8B8A8_UNORM -> float4, offset 12)
 * - TEXCOORD0 (float2, offset 16) - UV coordinates
 * - NORMAL (float3, offset 24)
 * - TEXCOORD1 (float2, offset 36) - Lightmap coordinates (blocklight, skylight)
 * - TEXCOORD2 (uint16, offset 44) - Block entity ID (mc_Entity)
 * - TEXCOORD3 (float2, offset 46) - Texture center (mc_midTexCoord)
 *
 * Phase 1: No TBN matrix calculation (normals already in world space)
 *
 * Output: G-Buffer data for gbuffers_water.ps.hlsl
 *
 * Reference: Engine/Voxel/World/TerrainVertexLayout.hpp
 */

#include "../core/core.hlsl"
#include "../include/common_uniforms.hlsl"

// [RENDERTARGETS] 0,1,2
// Output: colortex0 (Albedo), colortex1 (Lightmap), colortex2 (Normal)

// ============================================================================
// Water-Specific Vertex Structures
// ============================================================================
// [IMPORTANT] Phase 1: No TBN matrix calculation
// - Normal is already transformed to world space
// - entityId and midTexCoord passed through unchanged

/**
 * @brief Vertex shader input - matches TerrainVertex (54 bytes)
 */
struct VSInput_Water
{
    float3 Position : POSITION; // Vertex position (local space)
    float4 Color : COLOR0; // Vertex color (R8G8B8A8_UNORM unpacked)
    float2 TexCoord : TEXCOORD0; // UV coordinates
    float3 Normal : NORMAL; // Normal vector
    float2 LightmapCoord: LIGHTMAP; // Lightmap (x=blocklight, y=skylight)
    uint   entityId : TEXCOORD2; // Block entity ID (mc_Entity)
    float2 midTexCoord : TEXCOORD3; // Texture center (mc_midTexCoord)
};

/**
 * @brief Vertex shader output / pixel shader input
 */
struct VSOutput_Water
{
    float4 Position : SV_POSITION; // Clip space position
    float4 Color : COLOR0; // Vertex color (passed through)
    float2 TexCoord : TEXCOORD0; // UV coordinates
    float3 Normal : NORMAL; // World normal (normalized)
    float2 LightmapCoord: LIGHTMAP; // Lightmap (blocklight, skylight)
    float3 WorldPos : TEXCOORD2; // World position (for fog, etc.)
    uint   entityId : TEXCOORD3; // Block entity ID (pass-through)
    float2 midTexCoord : TEXCOORD4; // Texture center (pass-through)
};

/**
 * @brief Water vertex shader main entry
 * @param input TerrainVertex data from VBO
 * @return Transformed vertex with G-Buffer data
 */
VSOutput_Water main(VSInput_Water input)
{
    VSOutput_Water output;

    // [STEP 1] Transform position: Model -> World -> View -> Clip
    float4 localPos  = float4(input.Position, 1.0);
    float4 worldPos  = mul(modelMatrix, localPos);
    float4 viewPos   = mul(gbufferView, worldPos);
    float4 renderPos = mul(gbufferRenderer, viewPos);
    float4 clipPos   = mul(gbufferProjection, renderPos);

    output.Position = clipPos;
    output.WorldPos = worldPos.xyz;

    // [STEP 2] Transform normal to world space
    output.Normal = normalize(mul((float3x3)modelMatrix, input.Normal));

    // [STEP 3] Pass through vertex attributes
    output.Color         = input.Color;
    output.TexCoord      = input.TexCoord;
    output.LightmapCoord = input.LightmapCoord;
    output.entityId      = input.entityId;
    output.midTexCoord   = input.midTexCoord;

    return output;
}
