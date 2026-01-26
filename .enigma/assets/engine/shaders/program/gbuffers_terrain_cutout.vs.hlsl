/**
 * @file gbuffers_terrain_cutout.vs.hlsl
 * @brief Terrain Cutout Vertex Shader - Alpha-tested blocks (leaves, grass)
 * @date 2026-01-19
 *
 * Iris Pipeline Reference:
 * - Phase: TERRAIN_CUTOUT / TERRAIN_CUTOUT_MIPPED
 * - Fallback: gbuffers_terrain_cutout -> gbuffers_terrain
 * - Vertex Format: Same as gbuffers_terrain (TerrainVertex, 54 bytes)
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
 * [NOTE] Vertex shader is identical to gbuffers_terrain.vs.hlsl
 * The difference is in the pixel shader (alpha test)
 *
 * Reference: Engine/Voxel/World/TerrainVertexLayout.hpp
 */

#include "../core/core.hlsl"
#include "../include/common_uniforms.hlsl"

// [RENDERTARGETS] 0,1,2
// Output: colortex0 (Albedo), colortex1 (Lightmap), colortex2 (Normal)

// ============================================================================
// Terrain Cutout Vertex Structures (same as solid terrain)
// ============================================================================

/**
 * @brief Vertex shader input - matches TerrainVertex (54 bytes)
 */
struct VSInput_TerrainCutout
{
    float3 Position : POSITION; // Vertex position (world space)
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
struct VSOutput_TerrainCutout
{
    float4 Position : SV_POSITION; // Clip space position
    float4 Color : COLOR0; // Vertex color (passed through)
    float2 TexCoord : TEXCOORD0; // UV coordinates
    float3 Normal : NORMAL; // World normal (normalized)
    float2 LightmapCoord: LIGHTMAP; // Lightmap (blocklight, skylight)
    float3 WorldPos : TEXCOORD2; // World position (for fog, etc.)
    uint   entityId : TEXCOORD5; // Block entity ID (pass-through)
    float2 midTexCoord : TEXCOORD6; // Texture center (pass-through)
};

/**
 * @brief Terrain cutout vertex shader main entry
 * @param input TerrainVertex data from VBO
 * @return Transformed vertex with G-Buffer data
 */
VSOutput_TerrainCutout main(VSInput_TerrainCutout input)
{
    VSOutput_TerrainCutout output;

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
