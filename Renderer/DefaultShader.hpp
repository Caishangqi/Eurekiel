#pragma once
static auto rawShader = R"(Texture2D    diffuseTexture : register(t0);
SamplerState diffuseSampler : register(s0);

cbuffer CameraConstants : register(b2)
{
    float4x4 WorldToCameraTransform;
    float4x4 CameraToRenderTransform;
    float4x4 RenderToClipTransform;
};

cbuffer ModelConstants : register(b3)
{
    float4x4 ModelToWorldTransform;
    float4   ModelColor;
};

struct vs_input_t
{
    float3 modelSpacePosition : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct v2p_t
{
    float4 clipSpacePosition : SV_Position;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

float GetFractionWithinRange(float value, float rangeStart, float rangeEnd)
{
    float range  = rangeEnd - rangeStart;
    float result = 0.f;
    if (range != 0.f)
    {
        result = (value - rangeStart) / range;
    }
    return result;
}

float Interpolate(float start, float end, float fractionTowardEnd)
{
    return start + (end - start) * fractionTowardEnd;
}

float RangeMap(float inValue, float inStart, float inEnd, float outStart, float outEnd)
{
    const float t = GetFractionWithinRange(inValue, inStart, inEnd);
    return Interpolate(outStart, outEnd, t);
}

v2p_t VertexMain(vs_input_t input)
{
    float4 modelPos  = float4(input.modelSpacePosition, 1.0);
    float4 worldPos  = mul(ModelToWorldTransform, modelPos);
    float4 cameraPos = mul(WorldToCameraTransform, worldPos);
    float4 renderPos = mul(CameraToRenderTransform, cameraPos);
    float4 clipPos   = mul(RenderToClipTransform, renderPos);

    v2p_t v2p;
    v2p.clipSpacePosition = clipPos;
    v2p.color             = input.color;
    v2p.uv                = input.uv;
    return v2p;
}

float4 PixelMain(v2p_t input) : SV_Target0
{
    float4 textureColor = diffuseTexture.Sample(diffuseSampler, input.uv);
    float4 vertexColor  = input.color;
    float4 color        = ModelColor * vertexColor * textureColor;
    clip(color.a - 0.01f);
    return float4(color);
}

)";

class DefaultShader
{
};
