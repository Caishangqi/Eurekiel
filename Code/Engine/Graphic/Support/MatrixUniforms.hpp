#pragma once

// 基于 Iris 源码: net.irisshaders.iris.uniforms.MatrixUniforms (引用)
// 文档验证: 2025-09-14 通过CommonUniforms.java第125行确认存在
// 
// 教学要点:
// 1. 在Iris中通过 MatrixUniforms.addMatrixUniforms() 调用
// 2. 负责管理所有变换矩阵相关的uniform变量
// 3. 包含MVP矩阵、逆矩阵、上一帧矩阵等用于高级渲染效果

#include <memory>

namespace enigma::graphic
{
    /**
     * @brief 矩阵Uniform管理器
     * 
     * 对应 Iris 中通过 MatrixUniforms.addMatrixUniforms() 调用的矩阵uniform系统
     * 负责管理所有与变换矩阵相关的着色器uniform变量，包括：
     * - 模型、视图、投影矩阵 (MVP)
     * - 逆矩阵和转置矩阵
     * - 上一帧矩阵 (用于运动模糊、TAA等)
     * - 阴影矩阵和光照变换矩阵
     * 
     * TODO: 基于 Iris 实现分析完整的矩阵uniform逻辑
     * - 实现 addMatrixUniforms() 方法 (对应 Iris 的同名方法)
     * - 支持多重阴影级联矩阵
     * - 支持时间相关的矩阵插值
     * - 集成到 UniformHolder 系统中
     */
    class MatrixUniforms final
    {
    private:
        // 禁止实例化 - 静态工具类
        MatrixUniforms()                                 = delete;
        ~MatrixUniforms()                                = delete;
        MatrixUniforms(const MatrixUniforms&)            = delete;
        MatrixUniforms& operator=(const MatrixUniforms&) = delete;

    public:
        /**
         * @brief 添加矩阵相关uniform (对应Iris的addMatrixUniforms)
         * @param uniforms Uniform持有器
         * @param directives 包指令配置
         * 
         * TODO: 实现矩阵uniform注册，包括：
         * - gbufferModelView, gbufferProjection (G-Buffer MVP)
         * - gbufferModelViewInverse, gbufferProjectionInverse (G-Buffer逆矩阵)
         * - gbufferPreviousModelView, gbufferPreviousProjection (上一帧矩阵)
         * - shadowModelView, shadowProjection (阴影变换矩阵)
         * - shadowModelViewInverse, shadowProjectionInverse (阴影逆矩阵)
         * - modelViewMatrix, projectionMatrix (通用MVP)
         * - normalMatrix (法线变换矩阵)
         * - textureMatrix (纹理变换矩阵)
         */
        static void AddMatrixUniforms(/* UniformHolder& uniforms,
                                       PackDirectives& directives */);

        // ========================================================================
        // 矩阵计算和获取方法
        // ========================================================================

        /**
         * @brief 获取当前模型视图矩阵
         * @return 4x4模型视图变换矩阵
         * 
         * TODO: 实现模型视图矩阵计算，基于：
         * - 相机位置和朝向
         * - 世界变换
         * - 渲染阶段特定的变换
         */
        // static Matrix4f GetModelViewMatrix();

        /**
         * @brief 获取当前投影矩阵
         * @return 4x4投影变换矩阵
         * 
         * TODO: 实现投影矩阵计算，支持：
         * - 透视投影 (标准游戏视角)
         * - 正交投影 (UI、2D元素)
         * - 自定义FOV和纵横比
         */
        // static Matrix4f GetProjectionMatrix();

        /**
         * @brief 获取MVP矩阵 (Model-View-Projection)
         * @return 完整的MVP变换矩阵
         * 
         * TODO: 实现MVP组合矩阵计算
         */
        // static Matrix4f GetMVPMatrix();

        /**
         * @brief 获取逆MVP矩阵
         * @return MVP矩阵的逆矩阵
         * 
         * TODO: 实现逆矩阵计算，用于：
         * - 屏幕空间到世界空间变换
         * - 深度缓冲区重构世界位置
         * - 后处理效果中的坐标转换
         */
        // static Matrix4f GetInverseMVPMatrix();

        /**
         * @brief 获取上一帧的MVP矩阵
         * @return 上一帧的MVP矩阵
         * 
         * TODO: 实现前一帧矩阵缓存，用于：
         * - 运动模糊效果
         * - 时域抗锯齿 (TAA)
         * - 运动向量计算
         */
        // static Matrix4f GetPreviousMVPMatrix();

        /**
         * @brief 获取法线变换矩阵
         * @return 法线向量变换矩阵
         * 
         * TODO: 实现法线矩阵计算：
         * - 通常是模型视图矩阵的逆转置
         * - 确保法线在非均匀缩放下的正确性
         */
        // static Matrix3f GetNormalMatrix();

        // ========================================================================
        // 阴影相关矩阵
        // ========================================================================

        /**
         * @brief 获取阴影模型视图矩阵
         * @return 从光源视角的模型视图矩阵
         * 
         * TODO: 实现阴影矩阵计算，支持：
         * - 定向光阴影 (平行光)
         * - 点光源阴影 (立方体贴图)
         * - 聚光灯阴影
         */
        // static Matrix4f GetShadowModelViewMatrix();

        /**
         * @brief 获取阴影投影矩阵
         * @return 从光源视角的投影矩阵
         * 
         * TODO: 实现阴影投影矩阵，考虑：
         * - 级联阴影映射 (CSM)
         * - 阴影距离和精度优化
         */
        // static Matrix4f GetShadowProjectionMatrix();

        /**
         * @brief 获取世界空间到阴影空间的变换矩阵
         * @return 完整的阴影变换矩阵
         * 
         * TODO: 实现阴影坐标变换，用于：
         * - 阴影映射采样
         * - 阴影衰减计算
         */
        // static Matrix4f GetShadowMatrix();

        // ========================================================================
        // 纹理和UV变换矩阵
        // ========================================================================

        /**
         * @brief 获取纹理变换矩阵
         * @return 纹理坐标变换矩阵
         * 
         * TODO: 实现纹理矩阵，支持：
         * - 纹理滚动和动画
         * - UV坐标的缩放和旋转
         * - 多重纹理混合
         */
        // static Matrix3f GetTextureMatrix();

        // ========================================================================
        // 矩阵缓存和管理
        // ========================================================================

        /**
         * @brief 更新所有矩阵缓存
         * 
         * TODO: 实现矩阵更新逻辑：
         * - 缓存当前帧矩阵为下一帧的"上一帧"
         * - 重新计算所有变换矩阵
         * - 触发uniform更新
         */
        static void UpdateMatrixCache();

        /**
         * @brief 设置自定义模型矩阵
         * @param modelMatrix 自定义模型变换矩阵
         * 
         * TODO: 支持对象级别的矩阵覆盖
         */
        // static void SetModelMatrix(const Matrix4f& modelMatrix);

        /**
         * @brief 重置为默认矩阵状态
         * 
         * TODO: 重置所有矩阵为单位矩阵或默认状态
         */
        static void ResetToDefault();
    };
} // namespace enigma::graphic
