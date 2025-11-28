/**
 * @file gbuffers_clouds.ps.hlsl
 * @brief Cloud Rendering - Pixel Shader (Minecraft Vanilla Style)
 * @date 2025-11-26
 *
 * Features:
 * - Samples 256x256 clouds.png texture
 * - Alpha blending for semi-transparent clouds
 * - Outputs to colortex0 (color), colortex6 (normal), colortex3 (specular)
 *
 * Texture Sampling:
 * - clouds.png: 256x256 tileable cloud texture
 * - UV coordinates animated by Vertex Shader (scrolling effect)
 * - Alpha channel controls cloud transparency
 *
 * Output Targets:
 * - colortex0: Cloud color with alpha
 * - colortex6: Normal data (flat normal for clouds)
 * - colortex3: Specular data (no specular for clouds)
 */

#include "../core/Common.hlsl"
#include "../include/celestial_uniforms.hlsl"

// [RENDERTARGETS] 0 6 3
// Output to colortex0 (main color), colortex6 (normal), colortex3 (specular)

/**
 * @brief Pixel Shader Output Structure
 */
struct PSOutput
{
    float4 Color    : SV_Target0; // colortex0: Cloud color (RGB) + Alpha
    float4 Normal   : SV_Target1; // colortex6: Normal data
    float4 Specular : SV_Target2; // colortex3: Specular data
};

/**
 * @brief Pixel Shader Main Entry
 * @param input Interpolated vertex data from Vertex Shader
 * @return PSOutput Multi-render-target output
 *
 * Rendering:
 * 1. Sample clouds.png texture using animated UV coordinates
 * 2. Apply vertex color modulation (for tinting)
 * 3. Output color and alpha to colortex0
 * 4. Output flat normal to colortex6
 * 5. Output zero specular to colortex3 (clouds are diffuse only)
 */
PSOutput main(VSOutput input)
{
    PSOutput output;

    // [STEP 1] Sample Cloud Texture (256x256 clouds.png)
    // Texture slot: Use bindless texture access or fixed slot
    // For now, assume clouds.png is bound to a known slot
    Texture2D cloudsTexture = customImage0; // Placeholder: Replace with actual slot
    //SamplerState linearSampler = SamplerDescriptorHeap[0]; // Linear sampler with wrap mode

    float4 cloudColor = cloudsTexture.Sample(linearSampler, input.TexCoord);

    // [STEP 2] Apply Vertex Color Modulation
    // Vertex color can be used for tinting or brightness adjustment
    cloudColor *= input.Color;

    // [STEP 3] Output to colortex0 (Main Color + Alpha)
    // Alpha channel controls cloud transparency (0.0 = transparent, 1.0 = opaque)
    output.Color = cloudColor;

    // [STEP 4] Output to colortex6 (Normal Data)
    // Clouds use flat upward-facing normal (+Z in engine coordinates)
    float3 cloudNormal = float3(0.0, 0.0, 1.0); // +Z up
    output.Normal = float4(cloudNormal * 0.5 + 0.5, 1.0); // Encode normal to [0,1] range

    // [STEP 5] Output to colortex3 (Specular Data)
    // Clouds are diffuse only, no specular reflection
    output.Specular = float4(0.0, 0.0, 0.0, 1.0);

    return output;
}
