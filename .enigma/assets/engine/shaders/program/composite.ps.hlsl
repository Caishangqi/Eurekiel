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


struct PSOutput
{
    float4 color0 : SV_Target0; // colortex4
};

PSOutput main(PSInput input)
{
    PSOutput output;

    // [STEP 1] Sample depth to detect sky/cloud pixels
    // Reference: ComplementaryReimagined deferred1.glsl Line 146
    // Sky pixels have depth = 1.0 (far plane)
    // Terrain pixels have depth < 1.0
    Texture2D<float4> depthTex = depthtex0;
    float             depth    = depthTex.Sample(linearSampler, input.TexCoord).r;

    // [STEP 2] Sample albedo color
    float4 albedo = colortex0.Sample(pointSampler, input.TexCoord);

    // [STEP 3] Sky detection - skip lighting for sky pixels
    // Reference: ComplementaryReimagined deferred1.glsl Line 175, 240
    // if (z0 < 1.0) { terrain } else { sky }
    if (depth >= 1.0)
    {
        // Sky - no lighting applied, direct output
        output.color0 = albedo;
        return output;
    }

    // [STEP 4] Sample lightmap for terrain
    float blockLight = colortex1.Sample(pointSampler, input.TexCoord).r;
    float skyLight   = colortex1.Sample(pointSampler, input.TexCoord).g;

    // [STEP 5] Cloud detection - skip lighting if no lightmap data
    // Clouds don't write to colortex1, so lightmap values are 0
    if (blockLight == 0.0 && skyLight == 0.0)
    {
        // Cloud or unlit pixel - no lighting applied
        output.color0 = albedo;
        return output;
    }

    // [STEP 6] Apply lighting to terrain
    float lightIntensity = CalculateLightIntensity(blockLight, skyLight, skyBrightness);
    float finalLight     = max(lightIntensity, 0.03);
    output.color0        = float4(albedo.rgb * finalLight, albedo.a);

    return output;
}
