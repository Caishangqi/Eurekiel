cbuffer Matrices : register(b7)
{
    float4x4 gbufferView;
    float4x4 gbufferViewInverse;
    float4x4 gbufferProjection;
    float4x4 gbufferProjectionInverse;
    float4x4 gbufferRenderer;
    float4x4 gbufferRendererInverse;

    float4x4 shadowView;
    float4x4 shadowViewInverse;
    float4x4 shadowProjection;
    float4x4 shadowProjectionInverse;

    float4x4 normalMatrix;
    float4x4 textureMatrix;
};
