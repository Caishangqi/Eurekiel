/**
 * @file gbuffers_textured.vs.hlsl
 * @brief Iris 兼容 Fallback 着色器 - gbuffers_textured (顶点着色器)
 * @date 2025-10-04
 *
 * 教学要点:
 * 1. 纹理渲染 Fallback - 支持纹理采样的基础着色器
 * 2. 对应 Iris ProgramId::Textured - Fallback 链的中间层
 * 3. 传递 UV 坐标到像素着色器用于纹理采样
 * 4. 与 gbuffers_basic 唯一区别：像素着色器会采样纹理
 *
 * Iris Fallback 链:
 * - Water → Terrain → TexturedLit → **Textured** → Basic
 * - Entities → TexturedLit → **Textured** → Basic
 * - Clouds → **Textured** → Basic
 *
 * 使用场景:
 * - 大多数几何体的默认渲染 (地形、实体、天空等)
 * - 需要纹理但不需要光照的场景
 */

#include "../core/core.hlsl"

/**
 * @brief 顶点着色器主函数
 * @param input 顶点输入 (VSInput from Common.hlsli)
 * @return VSOutput - 包含 UV 坐标的顶点输出
 *
 * 教学要点:
 * 1. 与 gbuffers_basic.vs 完全相同 - 复用 StandardVertexTransform()
 * 2. UV 坐标已自动传递 (在 StandardVertexTransform 中处理)
 * 3. 区别在像素着色器 - gbuffers_textured.ps 会使用 UV 采样纹理
 */
VSOutput main(VSInput input)
{
    // 标准顶点变换
    // - MVP 变换
    // - 颜色解包
    // - UV 坐标传递 (关键: 供像素着色器使用)
    return StandardVertexTransform(input);
    //
}
