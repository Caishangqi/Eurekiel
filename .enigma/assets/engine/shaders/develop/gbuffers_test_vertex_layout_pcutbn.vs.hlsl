#include "../core/Common.hlsl"

/**
 * @brief 顶点着色器主函数
 * @param input 顶点输入 (VSInput from Common.hlsli)
 * @return VSOutput - 裁剪空间位置 + 顶点颜色
 *
 * 教学要点:
 * 1. 使用 Common.hlsli 提供的 StandardVertexTransform() 辅助函数
 * 2. 自动处理 MVP 变换、颜色解包、数据传递
 * 3. KISS 原则 - 极简实现，无额外计算
 */
VSOutput main(VSInput input)
{
    // 标准顶点变换 (MVP + 颜色解包)
    // - 从 UniformBuffer 获取 gbufferMVP 矩阵
    // - 将 uint 颜色解包为 float4
    // - 传递所有顶点属性到像素着色器
    return StandardVertexTransform(input);
    // TODO: Fix the entry point
}
