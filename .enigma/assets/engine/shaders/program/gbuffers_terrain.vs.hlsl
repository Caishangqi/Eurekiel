/**
 * @file gbuffers_terrain.vs.hlsl
 * @brief Terrain Vertex Shader - TerrainVertex Layout (44 bytes)
 * @date 2025-12-25
 *
 * Input Layout (matches TerrainVertex):
 * - POSITION (float3, offset 0)
 * - COLOR (R8G8B8A8_UNORM -> float4, offset 12)
 * - TEXCOORD0 (float2, offset 16) - UV coordinates
 * - NORMAL (float3, offset 24)
 * - TEXCOORD1 (float2, offset 36) - Lightmap coordinates (blocklight, skylight)
 *
 * Output: G-Buffer data for gbuffers_terrain.ps.hlsl
 *
 * Reference: Engine/Voxel/World/TerrainVertexLayout.hpp
 */

#include "../core/Common.hlsl"
#include "../include/common_uniforms.hlsl"

// [RENDERTARGETS] 0,1,2
// Output: colortex0 (Albedo), colortex1 (Lightmap), colortex2 (Normal)

// ============================================================================
// Terrain-Specific Vertex Structures
// ============================================================================
// [IMPORTANT] These differ from Common.hlsl VSInput/VSOutput:
// - No Tangent/Bitangent (terrain uses face normals)
// - Has LightmapCoord (LIGHTMAP) for blocklight/skylight

/**
 * @brief Vertex shader input - matches TerrainVertex (44 bytes)
 */
struct VSInput_Terrain
{
    float3 Position : POSITION; // Vertex position (world space)
    float4 Color : COLOR0; // Vertex color (R8G8B8A8_UNORM unpacked)
    float2 TexCoord : TEXCOORD0; // UV coordinates
    float3 Normal : NORMAL; // Normal vector
    float2 LightmapCoord: LIGHTMAP; // Lightmap (x=blocklight, y=skylight)
};

/**
 * @brief Vertex shader output / pixel shader input
 */
struct VSOutput_Terrain
{
    float4 Position : SV_POSITION; // Clip space position
    float4 Color : COLOR0; // Vertex color (passed through)
    float2 TexCoord : TEXCOORD0; // UV coordinates
    float3 Normal : NORMAL; // World normal (normalized)
    float2 LightmapCoord: LIGHTMAP; // Lightmap (blocklight, skylight)
    float3 WorldPos : TEXCOORD2; // World position (for fog, etc.)
};

/**
 * @brief Terrain vertex shader main entry
 * @param input TerrainVertex data from VBO
 * @return Transformed vertex with G-Buffer data
 */
VSOutput_Terrain main(VSInput_Terrain input)
{
    VSOutput_Terrain output;

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

    return output;
}
