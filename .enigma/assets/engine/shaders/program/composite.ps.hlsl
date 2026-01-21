#include "../core/Common.hlsl"
#include "../include/celestial_uniforms.hlsl"
/**
 * @brief Calculate final lighting intensity using Minecraft vanilla formula
 * @param blockLight Block light value (0.0 - 1.0, from lightmap.r)
 * @param skyLight Sky light value (0.0 - 1.0, from lightmap.g)
 * @param skyBrightnessValue Time-based sky brightness (0.2 - 1.0, from celestial uniforms)
 * @return Final light intensity (0.0 - 1.0)
 *
 * Reference: Minecraft LevelLightEngine.getRawBrightness()
 * Formula: finalLight = max(blockLight, skyLight * skyBrightness)
 *
 * [IMPORTANT] This is NOT multiplication! It's max() operation.
 * - Block light is independent of time
 * - Sky light is scaled by skyBrightness (time-dependent)
 * - Final light = whichever is brighter
 */
float CalculateLightIntensity(float blockLight, float skyLight, float skyBrightnessValue)
{
    // Scale sky light by time-based brightness
    // skyBrightness: 1.0 at noon, 0.2 at midnight
    float effectiveSkyLight = skyLight * skyBrightnessValue;

    // Final light = max of block light and effective sky light
    // [IMPORTANT] This matches Minecraft's lighting model
    return max(blockLight, effectiveSkyLight);
}

float NDCDepthToViewDepth(float ndcDepth, float near, float far)
{
    return (near * far) / (far - ndcDepth * (far - near));
}

struct PSOutput
{
    float4 color0 : SV_Target0; // colortex4
};

PSOutput main(PSInput input)
{
    PSOutput output;

    // [STEP 1] Sample depth textures
    // [IMPORTANT] Use sampler1 (Point filtering) for depth textures!
    // Depth values must NOT be interpolated - use Point sampling only.
    // sampler0 = Linear (for color textures)
    // sampler1 = Point (for depth textures)
    //
    // [FIX] Correct depth texture semantics based on actual pipeline:
    // - depthtex0: Main depth buffer, written by ALL passes (opaque + translucent)
    // - depthtex1: Snapshot copied BEFORE translucent pass (opaque only)
    //
    // Pipeline flow:
    //   TerrainSolid   -> writes depthtex0
    //   TerrainCutout  -> writes depthtex0, then Copy(0,1) in EndPass
    //   TerrainTranslucent -> writes depthtex0 (now includes water)
    //   Composite      -> reads both for depth comparison
    float depthOpaque = depthtex1.Sample(sampler1, input.TexCoord).r; // Opaque only (snapshot before translucent)
    float depthAll    = depthtex0.Sample(sampler0, input.TexCoord).r; // All objects (main depth buffer)

    /*
    float linearDepthOpaque = NDCDepthToViewDepth(depthOpaque, 0.01, 1);
    float linearDepthAll = NDCDepthToViewDepth(depthAll, 0.01, 1);
    output.color0 = float4(linearDepthOpaque, linearDepthAll, 0.0, 1.0);
    return output;
    */

    // [STEP 2] Sample albedo color (use Linear sampler for color)
    float4 albedo = colortex0.Sample(sampler0, input.TexCoord);

    // [STEP 3] Pure sky detection - both depths are far plane
    // When depthAll >= 1.0: no geometry at all (pure sky)
    if (depthAll >= 1.0)
    {
        // Pure sky - no lighting applied, direct output
        output.color0 = albedo;
        return output;
    }


    // [STEP 5] Sample lightmap for terrain (use Linear sampler for color data)
    float blockLight = colortex1.Sample(sampler0, input.TexCoord).r;
    float skyLight   = colortex1.Sample(sampler0, input.TexCoord).g;

    // [STEP 6] Cloud detection - skip lighting if no lightmap data
    // Clouds don't write to colortex1, so lightmap values are 0
    if (blockLight == 0.0 && skyLight == 0.0)
    {
        // Cloud or unlit pixel - no lighting applied
        output.color0 = albedo;
        return output;
    }

    // [STEP 7] Apply lighting to terrain
    float lightIntensity = CalculateLightIntensity(blockLight, skyLight, skyBrightness);
    float finalLight     = max(lightIntensity, 0.03);
    output.color0        = float4(albedo.rgb * finalLight, albedo.a);

    return output;
}
