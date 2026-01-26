#include "../core/core.hlsl"

struct VSOutput_PCU
{
    float4 Position : SV_POSITION; // Clipping space position
    float4 Color : COLOR0; // Vertex color (after unpacking)
    float2 TexCoord : TEXCOORD0; // Texture coordinates
};

struct VSInput_PCU
{
    float3 Position : POSITION; // Vertex position (world space)
    float4 Color : COLOR0; // Vertex color (R8G8B8A8_UNORM)
    float2 TexCoord : TEXCOORD0; // Texture coordinates
};

VSOutput_PCU main(VSInput_PCU input)
{
    VSOutput_PCU output;

    // 1. 顶点位置变换
    float4 localPos  = float4(input.Position, 1.0);
    float4 worldPos  = mul(modelMatrix, localPos);
    float4 cameraPos = mul(gbufferView, worldPos);
    float4 renderPos = mul(gbufferRenderer, cameraPos);
    float4 clipPos   = mul(gbufferProjection, renderPos);

    output.Position = clipPos;
    output.Color    = input.Color;
    output.TexCoord = input.TexCoord;

    return output;
}
