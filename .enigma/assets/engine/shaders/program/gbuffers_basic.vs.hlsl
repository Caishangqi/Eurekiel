/**
 * @file gbuffers_basic.vs.hlsl
 * @brief Iris 兼容 Fallback 着色器 - gbuffers_basic (顶点着色器)
 * @date 2025-10-04
 *
 * 教学要点:
 * 1. 最基础的 Fallback 着色器 - 仅渲染纯色几何体
 * 2. 无纹理采样 - 只使用顶点颜色
 * 3. 对应 Iris ProgramId::Basic - Fallback 链的最底层
 * 4. 所有着色器都包含 Common.hlsli - 使用固定布局
 *
 * Iris Fallback 链:
 * - Water → Terrain → TexturedLit → Textured → **Basic** (最终回退)
 * - Entities → TexturedLit → Textured → **Basic**
 * - Clouds → Textured → **Basic**
 *
 * 使用场景:
 * - Shader Pack 未提供任何着色器时的默认回退
 * - 调试渲染 - 验证几何体正确性
 * - 纯色几何体渲染 (UI 元素、调试线框等)
 */

#include "../core/core.hlsl"

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
