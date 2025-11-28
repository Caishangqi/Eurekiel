/**
 * @file gbuffers_skytextured.ps.hlsl
 * @brief Sun/Moon Texture Rendering - Pixel Shader (Minecraft Style)
 * @date 2025-11-26
 *
 * Features:
 * - Samples sun/moon texture from atlas (customImage0)
 * - UV coordinates calculated on CPU side using SpriteAtlas::GetSprite(index).GetUVBounds()
 * - Applies alpha blending for soft edges
 * - Uses celestial angle to modulate brightness
 *
 * Design Philosophy:
 * - CPU-side UV calculation (follows design.md line 156)
 * - GPU only performs texture sampling and brightness modulation
 * - Consistent with SceneUnitTest_SpriteAtlas implementation pattern
 */

#include "../core/Common.hlsl"
#include "../include/celestial_uniforms.hlsl"

// [RENDERTARGETS] 0
// Output to colortex0 (sky color with sun/moon)

/**
 * @brief Pixel Shader Output Structure
 */
struct PSOutput
{
    float4 Color0 : SV_Target0; // colortex0 (sun/moon texture)
};

/**
 * @brief Sun/Moon Texture Atlas Indices
 * Assumes atlas stored in customImage0 slot (configurable)
 */
static const uint CELESTIAL_ATLAS_SLOT = 0; // customImage0

/**
 * @brief Pixel Shader Main Entry
 * @param input Interpolated vertex data from VS
 * @return PSOutput Sun/moon texture with alpha blending
 */
PSOutput main(PSInput input)
{
    PSOutput output;

    // [STEP 1] Use CPU-calculated UV directly (no GPU-side offset calculation)
    // CPU side uses SpriteAtlas::GetSprite(index).GetUVBounds() to calculate correct UV
    // All atlas UV calculations are done on CPU side before vertex submission
    float2 atlasUV = input.TexCoord;

    // [STEP 2] Sample Celestial Texture from Atlas
    Texture2D celestialAtlas = GetCustomImage(CELESTIAL_ATLAS_SLOT);
    float4 texColor = celestialAtlas.Sample(pointSampler, atlasUV);

    // [STEP 3] Calculate Brightness Modulation based on celestial angle
    // Brightness fades based on time of day
    float brightness = saturate(1.0 - abs(celestialAngle - 0.25) * 2.0);

    // [STEP 4] Apply Brightness and Alpha Blending
    float3 finalColor = texColor.rgb * brightness;
    float alpha = texColor.a;

    // [STEP 5] Output Final Color
    output.Color0 = float4(finalColor, alpha);

    return output;
}
