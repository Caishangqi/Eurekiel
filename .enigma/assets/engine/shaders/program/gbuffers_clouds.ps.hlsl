/**
 * @file gbuffers_clouds.ps.hlsl
 * @brief Cloud Rendering - Pixel Shader (Minecraft Vanilla Style)
 * @date 2025-11-26
 *
 * Features:
 * - Samples 256x256 clouds.png texture
 * - Alpha blending for semi-transparent clouds
 * - Alpha discard for cloud shape formation (threshold: 0.1)
 * - Outputs to colortex0 (color only)
 *
 * Texture Sampling:
 * - clouds.png: 256x256 tileable cloud texture
 * - UV coordinates animated by Vertex Shader (scrolling effect)
 * - wrapSampler (register s3) for infinite UV scrolling
 * - Alpha channel controls cloud transparency
 *
 * Output Targets:
 * - colortex0: Cloud color with alpha
 */

#include "../core/Common.hlsl"
#include "../include/celestial_uniforms.hlsl"

// [RENDERTARGETS] 0
// Output to colortex0 (main color)

/**
 * @brief Pixel Shader Output Structure
 */
struct PSOutput
{
    float4 Color : SV_Target0; // colortex0: Cloud color (RGB) + Alpha
};

/**
 * @brief Pixel Shader Main Entry
 * @param input Interpolated vertex data from Vertex Shader
 * @return PSOutput Single render target output
 *
 * Rendering:
 * 1. Sample clouds.png texture using animated UV coordinates with wrapSampler
 * 2. Apply alpha discard (threshold: 0.1) to form cloud shapes
 * 3. Apply vertex color modulation (for tinting)
 * 4. Output color and alpha to colortex0
 */
PSOutput main(VSOutput input)
{
    PSOutput output;

    // [STEP 1] Sample Cloud Texture (256x256 clouds.png)
    // Use wrapSampler (register s3) for infinite UV scrolling
    Texture2D cloudsTexture = customImage0;
    float4    cloudColor    = cloudsTexture.Sample(wrapSampler, input.TexCoord);

    // [STEP 2] Alpha Discard - Form Cloud Shapes
    // Discard fragments with alpha below 0.1 to create cloud shapes
    if (cloudColor.a < 0.1)
    {
        discard;
    }

    // [STEP 3] Apply Vertex Color Modulation
    // Vertex color can be used for tinting or brightness adjustment
    cloudColor *= input.Color;

    // [STEP 4] Output to colortex0 (Main Color + Alpha)
    // Alpha channel controls cloud transparency (0.0 = transparent, 1.0 = opaque)
    output.Color = cloudColor;

    return output;
}
