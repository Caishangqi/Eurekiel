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

        /**
         * @brief GBuffer模型视图矩阵
         * @type mat4
         * @iris gbufferModelView
         *
         * 教学要点:
         * - 将模型空间（player space）转换到视图空间（view space）
         * - 用于gbuffers_*着色器
         */
        alignas(16) Mat44 gbufferModelView;

        /**
         * @brief GBuffer模型视图逆矩阵
         * @type mat4
         * @iris gbufferModelViewInverse
         *
         * 教学要点: 将视图空间转换回模型空间
         */
        alignas(16) Mat44 gbufferModelViewInverse;

        /**
         * @brief GBuffer投影矩阵
         * @type mat4
         * @iris gbufferProjection
         *
         * 教学要点:
         * - 将视图空间转换到裁剪空间（clip space）
         * - 包含透视除法参数
         */
        alignas(16) Mat44 gbufferProjection;

        /**
         * @brief GBuffer投影逆矩阵
         * @type mat4
         * @iris gbufferProjectionInverse
         *
         * 教学要点:
         * - 将裁剪/屏幕空间转换回视图空间
         * - 用于延迟渲染的位置重建
         */
        alignas(16) Mat44 gbufferProjectionInverse;

        /**
         * @brief 上一帧GBuffer模型视图矩阵
         * @type mat4
         * @iris gbufferPreviousModelView
         *
         * 教学要点:
         * - 用于运动模糊、TAA等时序效果
         * - 存储上一帧的gbufferModelView
         */
        alignas(16) Mat44 gbufferPreviousModelView;

        /**
         * @brief 上一帧GBuffer投影矩阵
         * @type mat4
         * @iris gbufferPreviousProjection
         *
         * 教学要点: 用于时序抗锯齿等需要历史投影的效果
         */
        alignas(16) Mat44 gbufferPreviousProjection;

        // =========================================================================
        // Shadow矩阵（阴影Pass）
        // =========================================================================

        /**
         * @brief 阴影模型视图矩阵
         * @type mat4
         * @iris shadowModelView
         *
         * 教学要点:
         * - 将模型空间转换到阴影视图空间
         * - 用于shadow着色器
         */
        alignas(16) Mat44 shadowModelView;

        /**
         * @brief 阴影模型视图逆矩阵
         * @type mat4
         * @iris shadowModelViewInverse
         *
         * 教学要点: 将阴影视图空间转换回模型空间
         */
        alignas(16) Mat44 shadowModelViewInverse;

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

        // =========================================================================
        // 通用矩阵（当前几何体）
        // =========================================================================

        /**
         * @brief 当前模型视图矩阵
         * @type mat4
         * @iris modelViewMatrix
         *
         * 教学要点:
         * - 当前几何体的模型空间到视图空间变换
         * - 可能与gbufferModelView不同（例如手持物品）
         */
        alignas(16) Mat44 modelViewMatrix;

        /**
         * @brief 当前模型视图逆矩阵
         * @type mat4
         * @iris modelViewMatrixInverse
         *
         * 教学要点: 当前几何体的视图空间到模型空间变换
         */
        alignas(16) Mat44 modelViewMatrixInverse;

        /**
         * @brief 当前投影矩阵
         * @type mat4
         * @iris projectionMatrix
         *
         * 教学要点: 当前几何体的视图空间到裁剪空间变换
         */
        alignas(16) Mat44 projectionMatrix;

        /**
         * @brief 当前投影逆矩阵
         * @type mat4
         * @iris projectionMatrixInverse
         *
         * 教学要点: 当前几何体的裁剪/屏幕空间到视图空间变换
         */
        alignas(16) Mat44 projectionMatrixInverse;

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

        // =========================================================================
        // 辅助矩阵
        // =========================================================================

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

        /**
         * @brief 默认构造函数 - 初始化为单位矩阵
         */
        MatricesUniforms()
            : gbufferModelView(Mat44::IDENTITY)
              , gbufferModelViewInverse(Mat44::IDENTITY)
              , gbufferProjection(Mat44::IDENTITY)
              , gbufferProjectionInverse(Mat44::IDENTITY)
              , gbufferPreviousModelView(Mat44::IDENTITY)
              , gbufferPreviousProjection(Mat44::IDENTITY)
              , shadowModelView(Mat44::IDENTITY)
              , shadowModelViewInverse(Mat44::IDENTITY)
              , shadowProjection(Mat44::IDENTITY)
              , shadowProjectionInverse(Mat44::IDENTITY)
              , modelViewMatrix(Mat44::IDENTITY)
              , modelViewMatrixInverse(Mat44::IDENTITY)
              , projectionMatrix(Mat44::IDENTITY)
              , projectionMatrixInverse(Mat44::IDENTITY)
              , normalMatrix(Mat44::IDENTITY)
              , textureMatrix(Mat44::IDENTITY)
        {
        }
    };
#pragma warning(pop)

    // 编译期验证: 检查结构体大小
    // 16个Mat44矩阵 × 64字节 = 1024字节 (严格遵循Iris官方规范)
    static_assert(sizeof(MatricesUniforms) == 16 * 64,
                  "MatricesUniforms must contain exactly 16 Mat44 matrices (1024 bytes)");

    // 编译期验证: 确保不超过合理大小限制（2KB）
    static_assert(sizeof(MatricesUniforms) <= 2048,
                  "MatricesUniforms exceeds 2KB, consider splitting");
} // namespace enigma::graphic
