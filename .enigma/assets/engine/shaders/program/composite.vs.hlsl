#include "../core/Common.hlsl"
// Composite should not have vertex shader
VSOutput main(VSInput input)
{
    VSOutput output;

    // 1. Vertex position transformation
    float4 localPos = float4(input.Position, 1.0);

    output.Position = localPos;
    output.WorldPos = localPos.xyz;
    output.Color    = input.Color;
    output.TexCoord = input.TexCoord;

    // 4. Normal transformation
    output.Normal = normalize(mul(normalMatrix, float4(input.Normal, 0.0)).xyz);

    // 5. Tangents and secondary tangents
    output.Tangent   = normalize(mul(gbufferView, float4(input.Tangent, 0.0)).xyz);
    output.Bitangent = normalize(mul(gbufferView, float4(input.Bitangent, 0.0)).xyz);

    return output;
}
