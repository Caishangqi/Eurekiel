/**
 * @file gbuffers_basic.ps.hlsl
 * @brief Iris compatible Fallback shader - gbuffers_basic (Pixel Shader)
 * @date 2025-10-04
 *
 * Basic pixel shader - outputs vertex color with texture sampling.
 * No lighting, no special effects.
 * GBuffers stage - writes to colortex0 (main color buffer).
 * Lighting handled in Deferred/Composite passes.
 */

#include "../core/core.hlsl"

// [RENDERTARGETS] 0
// Output to colortex0 (main color)

/**
 * @brief Pixel Shader Main
 * @param input Pixel input from vertex shader
 * @return float4 - Single render target output (colortex0)
 */
float4 main(PSInput input) : SV_Target0
{
    // Sample CustomImage0 texture (linear sampler + UV)
    float4 texColor = customImage0.Sample(sampler1, input.TexCoord);

    // Final color = texture * vertex color * model color
    return texColor * input.Color * modelColor;
}
