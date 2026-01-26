/**
 * @file gbuffers_skytextured.ps.hlsl
 * @brief Sun/Moon Texture Rendering - Pixel Shader (Minecraft Vanilla Style)
 * @date 2025-12-08
 *
 * [REFACTOR] Simplified to match Minecraft vanilla architecture:
 * - Direct texture sampling from customImage0
 * - Simple alpha discard for transparency
 * - ColorModulator for brightness control (matches Minecraft DynamicTransforms)
 *
 * Reference:
 * - Minecraft position_tex.fsh:
 *   vec4 color = texture(Sampler0, texCoord0);
 *   if (color.a == 0.0) discard;
 *   fragColor = color * ColorModulator;
 */

#include "../core/core.hlsl"
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
 * @brief Pixel Shader Main Entry - Minecraft Vanilla Architecture
 * @param input Interpolated vertex data from VS
 * @return PSOutput Sun/moon texture with alpha blending
 */
PSOutput main(PSInput input)
{
    PSOutput output;

    // [STEP 1] Sample sun/moon texture from customImage0
    Texture2D celestialTexture = GetCustomImage(0);
    float4    texColor         = celestialTexture.Sample(sampler1, input.TexCoord);

    // [STEP 2] Discard fully transparent pixels (Minecraft style)
    if (texColor.a == 0.0)
    {
        discard;
    }

    // [STEP 3] Apply ColorModulator (brightness control)
    // ColorModulator is uploaded via CelestialConstantBuffer
    // Matches Minecraft DynamicTransforms.ColorModulator behavior
    float4 finalColor = texColor * colorModulator;

    // [STEP 4] Output final color with alpha
    output.Color0 = finalColor;

    return output;
}
