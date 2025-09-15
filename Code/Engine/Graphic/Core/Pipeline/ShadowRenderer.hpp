#pragma once

#include "WorldRenderingPhase.hpp"
#include <memory>
#include <array>

// Forward declarations
namespace enigma::graphic
{
    class D12RenderTargets;
    class CommandListManager;
    class D12Texture;
    class D12Buffer;
}

namespace Engine::Math
{
    struct Matrix4f;
    struct Vector3f;
    struct Frustum;
}

namespace enigma::graphic
{
    class ShaderPackManager;
    class UniformManager;
}

namespace enigma::graphic
{
    /**
     * @brief Iris兼容的阴影渲染器
     * 
     * 这个类对应Iris源码中的ShadowRenderer，专门负责处理阴影贴图
     * 的生成和管理。在Iris架构中，它是IrisRenderingPipeline的重要
     * 成员变量之一。
     * 
     * 教学目标：
     * - 理解级联阴影贴图(CSM)技术
     * - 学习阴影渲染的专门处理
     * - 掌握光源视角的渲染管理
     * - 体会阴影系统的复杂性
     * 
     * Iris架构对应：
     * Java: ShadowRenderer (IrisRenderingPipeline的成员)
     * C++:  class ShadowRenderer
     * 
     * 技术特点：
     * - 级联阴影贴图支持
     * - 多光源阴影处理
     * - 动态阴影距离调整
     * - 阴影采样优化
     */
    class ShadowRenderer final
    {
    public:
        /**
         * @brief 阴影级联配置
         * 
         * 定义单个阴影级联的参数
         */
        struct ShadowCascade
        {
            /// 级联索引 (0-3)
            uint32_t index = 0;

            /// 近裁剪面距离
            float nearPlane = 0.1f;

            /// 远裁剪面距离
            float farPlane = 100.0f;

            /// 阴影贴图分辨率
            uint32_t resolution = 1024;

            /// 是否启用此级联
            bool enabled = true;
        };

    private:
        // ===========================================
        // 核心资源
        // ===========================================

        /// 命令列表管理器
        std::shared_ptr<CommandListManager> m_commandManager;

        /// 着色器包管理器
        std::shared_ptr<ShaderPackManager> m_shaderManager;

        /// Uniform变量管理器
        std::shared_ptr<UniformManager> m_uniformManager;

        // ===========================================
        // 阴影渲染目标
        // ===========================================

        /// 阴影深度纹理
        std::unique_ptr<D12Texture> m_shadowDepthTexture;

        /// 阴影颜色纹理（某些效果需要）
        std::unique_ptr<D12Texture> m_shadowColorTexture;

        /// 级联阴影贴图数组
        std::array<std::unique_ptr<D12Texture>, 4> m_cascadeShadowMaps;

        // ===========================================
        // 阴影配置
        // ===========================================

        /// 阴影级联配置数组
        std::array<ShadowCascade, 4> m_shadowCascades;

        /// 当前激活的级联数量
        uint32_t m_activeCascadeCount = 3;

        /// 阴影贴图分辨率
        uint32_t m_shadowMapResolution = 2048;

        /// 阴影距离
        float m_shadowDistance = 128.0f;

        /// 是否启用阴影
        bool m_shadowsEnabled = true;

        // ===========================================
        // 光源和矩阵
        // ===========================================

        /// 光源方向（通常是太阳方向）
        Engine::Math::Vector3f m_lightDirection;

        /// 阴影视图矩阵数组（每个级联一个）
        std::array<Engine::Math::Matrix4f, 4> m_shadowViewMatrices;

        /// 阴影投影矩阵数组（每个级联一个）
        std::array<Engine::Math::Matrix4f, 4> m_shadowProjectionMatrices;

        /// 光源视图投影矩阵
        Engine::Math::Matrix4f m_lightViewProjectionMatrix;

        // ===========================================
        // 渲染状态
        // ===========================================

        /// 是否已初始化
        bool m_isInitialized = false;

        /// 当前渲染的级联索引
        int32_t m_currentCascadeIndex = -1;

        /// 调试模式
        bool m_debugMode = false;

    public:
        /**
         * @brief 构造函数
         * 
         * @param commandManager 命令列表管理器
         * @param shaderManager 着色器包管理器  
         * @param uniformManager Uniform变量管理器
         */
        ShadowRenderer(
            std::shared_ptr<CommandListManager> commandManager,
            std::shared_ptr<ShaderPackManager>  shaderManager,
            std::shared_ptr<UniformManager>     uniformManager);

        /**
         * @brief 析构函数
         */
        ~ShadowRenderer();

        // 禁用拷贝和移动
        ShadowRenderer(const ShadowRenderer&)            = delete;
        ShadowRenderer& operator=(const ShadowRenderer&) = delete;
        ShadowRenderer(ShadowRenderer&&)                 = delete;
        ShadowRenderer& operator=(ShadowRenderer&&)      = delete;

        // ===========================================
        // 初始化和配置
        // ===========================================

        /**
         * @brief 初始化阴影渲染器
         * 
         * @return true如果初始化成功
         */
        bool Initialize();

        /**
         * @brief 配置阴影级联
         * 
         * @param cascades 级联配置数组
         * @param cascadeCount 级联数量
         */
        void ConfigureCascades(const ShadowCascade* cascades, uint32_t cascadeCount);

        /**
         * @brief 设置阴影贴图分辨率
         * 
         * @param resolution 分辨率（通常是2的幂次）
         */
        void SetShadowMapResolution(uint32_t resolution);

        /**
         * @brief 设置阴影距离
         * 
         * @param distance 阴影渲染距离
         */
        void SetShadowDistance(float distance);

        // ===========================================
        // 主要渲染方法
        // ===========================================

        /**
         * @brief 开始阴影渲染Pass
         * 
         * 准备阴影渲染的所有资源和状态
         */
        void BeginShadowPass();

        /**
         * @brief 渲染所有阴影级联
         * 
         * 按顺序渲染所有启用的阴影级联
         */
        void RenderAllCascades();

        /**
         * @brief 渲染指定级联
         * 
         * @param cascadeIndex 级联索引 (0-3)
         */
        void RenderCascade(uint32_t cascadeIndex);

        /**
         * @brief 结束阴影渲染Pass
         * 
         * 完成阴影渲染的清理工作
         */
        void EndShadowPass();

        // ===========================================
        // 光源管理
        // ===========================================

        /**
         * @brief 设置光源方向
         * 
         * @param direction 归一化的光源方向向量
         */
        void SetLightDirection(const Engine::Math::Vector3f& direction);

        /**
         * @brief 更新光源矩阵
         * 
         * 根据当前相机和光源方向更新所有阴影矩阵
         * 
         * @param cameraPosition 相机位置
         * @param cameraDirection 相机方向
         * @param cameraFov 相机视野角度
         */
        void UpdateLightMatrices(const Engine::Math::Vector3f& cameraPosition,
                                 const Engine::Math::Vector3f& cameraDirection,
                                 float                         cameraFov);

        // ===========================================
        // 资源访问
        // ===========================================

        /**
         * @brief 获取阴影深度纹理
         * 
         * @return 阴影深度纹理指针
         */
        D12Texture* GetShadowDepthTexture() const;

        /**
         * @brief 获取指定级联的阴影贴图
         * 
         * @param cascadeIndex 级联索引
         * @return 级联阴影贴图指针
         */
        D12Texture* GetCascadeShadowMap(uint32_t cascadeIndex) const;

        /**
         * @brief 获取光源视图投影矩阵
         * 
         * @return 光源视图投影矩阵
         */
        const Engine::Math::Matrix4f& GetLightViewProjectionMatrix() const;

        /**
         * @brief 获取指定级联的阴影矩阵
         * 
         * @param cascadeIndex 级联索引
         * @return 阴影矩阵
         */
        Engine::Math::Matrix4f GetCascadeShadowMatrix(uint32_t cascadeIndex) const;

        // ===========================================
        // 状态查询
        // ===========================================

        /**
         * @brief 检查阴影是否启用
         * 
         * @return true如果阴影已启用
         */
        bool IsShadowEnabled() const { return m_shadowsEnabled; }

        /**
         * @brief 启用或禁用阴影
         * 
         * @param enabled 是否启用阴影
         */
        void SetShadowEnabled(bool enabled);

        /**
         * @brief 获取激活的级联数量
         * 
         * @return 级联数量
         */
        uint32_t GetActiveCascadeCount() const { return m_activeCascadeCount; }

        /**
         * @brief 获取阴影距离
         * 
         * @return 阴影渲染距离
         */
        float GetShadowDistance() const { return m_shadowDistance; }

        // ===========================================
        // 调试和优化
        // ===========================================

        /**
         * @brief 启用调试模式
         * 
         * @param enable 是否启用调试
         */
        void SetDebugMode(bool enable);

        /**
         * @brief 获取阴影渲染统计
         * 
         * @return 统计信息字符串
         */
        std::string GetShadowStats() const;

        /**
         * @brief 销毁阴影渲染器资源
         */
        void Destroy();

    private:
        // ===========================================
        // 内部辅助方法
        // ===========================================

        /**
         * @brief 创建阴影渲染目标
         * 
         * @return true如果创建成功
         */
        bool CreateShadowRenderTargets();

        /**
         * @brief 计算级联分割距离
         * 
         * @param cascadeIndex 级联索引
         * @return 分割距离
         */
        float CalculateCascadeSplitDistance(uint32_t cascadeIndex) const;

        /**
         * @brief 计算级联包围盒
         * 
         * @param cascadeIndex 级联索引
         * @param cameraPosition 相机位置
         * @param cameraDirection 相机方向
         * @param cameraFov 相机视野角度
         */
        void CalculateCascadeBounds(uint32_t                      cascadeIndex,
                                    const Engine::Math::Vector3f& cameraPosition,
                                    const Engine::Math::Vector3f& cameraDirection,
                                    float                         cameraFov);

        /**
         * @brief 设置阴影渲染状态
         * 
         * @param cascadeIndex 级联索引
         */
        void SetupShadowRenderState(uint32_t cascadeIndex);

        /**
         * @brief 渲染阴影投射物
         * 
         * @param cascadeIndex 级联索引
         */
        void RenderShadowCasters(uint32_t cascadeIndex);

        /**
         * @brief 清理阴影渲染状态
         */
        void CleanupShadowRenderState();
    };
} // namespace enigma::graphic
