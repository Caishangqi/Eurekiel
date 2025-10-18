/**
 * @file gbuffers_textured.ps.hlsl
 * @brief Iris 兼容 Fallback 着色器 - gbuffers_textured (像素着色器)
 * @date 2025-10-04
 *
 * 教学要点:
 * 1. 纹理渲染 - 采样纹理并与顶点颜色混合
 * 2. 使用 Bindless 访问纹理 - 通过 ResourceDescriptorHeap[index]
 * 3. 默认纹理索引 = 0 (Atlas 纹理或 Shader Pack 提供的纹理)
 * 4. 对应 Iris ProgramId::Textured
 *
 * 纹理绑定机制:
 * - Shader Pack 可通过 C++ 代码设置纹理索引
 * - 默认使用 index 0 (通常是方块纹理 Atlas)
 * - 真正的 Bindless - 无需预绑定，运行时动态指定
 *
 * 输出目标:
 * - SV_Target0 (colortex0) - 纹理颜色 × 顶点颜色
 */

#include "Common.hlsl"

/**
 * @brief 像素着色器主函数
 * @param input 像素输入 (PSInput from Common.hlsli)
 * @return float4 - 纹理采样结果 × 顶点颜色
 *
 * 教学要点:
 * 1. 从 ResourceDescriptorHeap 获取纹理 (Bindless 访问)
 * 2. 使用 linearSampler 采样纹理 (Common.hlsli 定义)
 * 3. 纹理颜色与顶点颜色相乘 (Minecraft 标准混合方式)
 * 4. 支持透明度 (Alpha 通道)
 */
float4 main(PSInput input) : SV_Target0
{
    // 1. 获取纹理 (默认使用索引 0)
    // 注意: 实际项目中可通过 Root Constants 传递纹理索引
    // 这里简化为固定索引 0 (通常是方块纹理 Atlas)
    Texture2D mainTexture = ResourceDescriptorHeap[0];

    // 2. 采样纹理
    // - 使用 linearSampler (线性过滤，防止像素化)
    // - UV 坐标已经过硬件插值
    float4 texColor = mainTexture.Sample(linearSampler, input.TexCoord);

    // 3. 纹理颜色与顶点颜色混合
    // - Minecraft 标准: texColor * vertexColor
    // - 顶点颜色用于 AO (Ambient Occlusion) 和色调调整
    float4 finalColor = texColor * input.Color;

    return finalColor;
}

struct PSOutput
{
    float4 color : SV_Target0;
    float4 normal : SV_Target1;
    float4 specular : SV_Target2;
};


/**
 * Iris 注释示例:
 * RENDERTARGETS: 0
 *
 * 高级用法:
 * - 如果需要访问多个纹理,可添加额外的采样:
 *   Texture2D normalMap = ResourceDescriptorHeap[1];
 *   Texture2D specularMap = ResourceDescriptorHeap[2];
 *
 * - 如果需要写入多个 RT:
 *   RENDERTARGETS: 0,1,2
 */
