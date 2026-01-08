#pragma once

#include "Engine/Math/Mat44.hpp"

namespace enigma::graphic
{
    /**
     * @brief Matrices Uniforms - 矩阵数据
     *
     * Iris官方文档参考:
     * https://shaders.properties/current/reference/uniforms/overview/#matrices
     *
     * 教学要点:
     * 1. 此结构体对应Iris "Matrices" Uniform分类
     * 2. 存储在GPU StructuredBuffer，通过matricesBufferIndex访问
     * 3. 包含所有Iris定义的变换矩阵（GBuffer, Shadow, 通用等）
     * 4. 所有矩阵使用alignas(16)确保16字节对齐
     * 5. 所有字段名称和语义与Iris完全兼容
     *
     * HLSL访问示例:
     * ```hlsl
     * StructuredBuffer<MatricesUniforms> matricesBuffer =
     *     ResourceDescriptorHeap[matricesBufferIndex];
     * float4x4 mvp = matricesBuffer[0].gbufferModelView;
     * float4x4 proj = matricesBuffer[0].gbufferProjection;
     * ```
     *
     * @note 所有Mat44矩阵使用alignas(16)确保HLSL float4x4对齐要求
     */
#pragma warning(push)
#pragma warning(disable: 4324) // 结构体因alignas而填充 - 预期行为
    struct MatricesUniforms
    {
        // =========================================================================
        // GBuffer矩阵（主渲染Pass）
        // =========================================================================
        alignas(16) Mat44 gbufferView;
        alignas(16) Mat44 gbufferViewInverse;
        alignas(16) Mat44 gbufferProjection;
        alignas(16) Mat44 gbufferProjectionInverse;
        alignas(16) Mat44 gbufferRenderer;
        alignas(16) Mat44 shadowView;
        alignas(16) Mat44 shadowViewInverse;

        /**
         * @brief 阴影投影矩阵
         * @type mat4
         * @iris shadowProjection
         *
         * 教学要点:
         * - 将阴影视图空间转换到阴影裁剪空间
         * - 通常使用正交投影
         */
        alignas(16) Mat44 shadowProjection;

        /**
         * @brief 阴影投影逆矩阵
         * @type mat4
         * @iris shadowProjectionInverse
         *
         * 教学要点: 将阴影裁剪/屏幕空间转换回阴影视图空间
         */
        alignas(16) Mat44 shadowProjectionInverse;

        /**
         * @brief 法线矩阵（3x3存储在4x4中）
         * @type mat3 (stored as mat4)
         * @iris normalMatrix
         *
         * 教学要点:
         * - 用于变换法线向量
         * - 通常为ModelView矩阵的逆转置
         * - HLSL中使用前3x3部分
         */
        alignas(16) Mat44 normalMatrix;

        /**
         * @brief 纹理矩阵
         * @type mat4
         * @iris textureMatrix
         *
         * 教学要点:
         * - 用于纹理坐标变换（gl_TextureMatrix[0]）
         * - 主要用于 gbuffers_armor_glint 的纹理滚动效果
         * - 也可能被模组应用到其他几何体
         * - 等价于 GLSL 中的 gl_TextureMatrix[0]
         *
         * HLSL使用示例:
         * ```hlsl
         * float2 coord = mul(textureMatrix, float4(vaUV0, 0.0, 1.0)).xy;
         * ```
         */
        alignas(16) Mat44 textureMatrix;
    };
#pragma warning(pop)
    static_assert(sizeof(MatricesUniforms) <= 2048, "MatricesUniforms exceeds 2KB, consider splitting");
} // namespace enigma::graphic
