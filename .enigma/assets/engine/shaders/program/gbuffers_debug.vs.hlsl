#include "../core/Common.hlsl"

VSOutput main(VSInput input)
{
    // 标准顶点变换 (MVP + 颜色解包)
    // - 从 UniformBuffer 获取 gbufferMVP 矩阵
    // - 将 uint 颜色解包为 float4
    // - 传递所有顶点属性到像素着色器
    return StandardVertexTransform(input);
    // TODO: Fix the entry point
}
