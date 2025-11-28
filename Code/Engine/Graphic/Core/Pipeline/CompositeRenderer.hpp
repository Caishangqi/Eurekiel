#pragma once

#include <memory>
#include <vector>
#include <string>

#include "CompositePass.hpp"
#include "Engine/Graphic/Target/BufferFlipper.hpp"
#include "Engine/Graphic/Shader/ShaderPack/Properties/PackDirectives.hpp"
#include "Engine/Graphic/Shader/ShaderPack/Texture/TextureStage.hpp"

namespace enigma::graphic
{
    class ShaderPackManager;
    class UniformManager;
    class D12RenderTargets;
    class CommandListManager;
    class D12Texture;
    class D12Buffer;
    class ShadowRenderTargets;
    class RenderTargets;
    class ShaderSource;
    class IWorldRenderingPipeline;

    /**
     * @brief Iris兼容的合成渲染器
     * 
     * 这个类直接对应Iris源码中的CompositeRenderer.java，是Iris架构
     * 中的核心组件之一。它负责执行composite和deferred程序，处理
     * 后处理效果和延迟光照计算。
     * 
     * 关键发现：在Iris架构中，延迟光照并不是独立的LightingPass类，
     * 而是集成在CompositeRenderer中通过deferred程序实现。
     * 
     * 教学目标：
     * - 理解现代渲染管线中的后处理架构
     * - 学习乒乓缓冲区技术的应用
     * - 掌握多Pass渲染的组织和管理
     * - 体会延迟光照与后处理的统一处理
     * 
     * Iris架构对应：
     * Java: public class CompositeRenderer
     * C++:  class CompositeRenderer
     * 
     * 技术特点：
     * - 支持composite1-99程序序列执行
     * - 支持deferred1-99程序（延迟光照）
     * - 集成BufferFlipper乒乓缓冲机制
     * - 动态Pass链管理
     * - 计算着色器支持
     */
    class CompositeRenderer final
    {
    public:

    private:
        // ===========================================
        // 核心资源管理
        // ===========================================

        /// 渲染目标管理器 - 管理所有colortex0-15
        std::shared_ptr<D12RenderTargets> m_renderTargets;

        /// 缓冲区翻转器 - 实现乒乓缓冲机制
        std::unique_ptr<BufferFlipper> m_bufferFlipper;

        /// 命令列表管理器
        std::shared_ptr<CommandListManager> m_commandManager;

        /// 着色器包管理器
        std::shared_ptr<ShaderPackManager> m_shaderManager;

        /// Uniform变量管理器
        std::shared_ptr<UniformManager> m_uniformManager;

        // ===========================================
        // Pass管理
        // ===========================================

        /// 当前正在执行的Pass索引
        int32_t m_currentPassIndex = -1;

        /// 是否支持计算着色器
        bool m_supportsComputeShaders = false;

        // ===========================================
        // 渲染状态
        // ===========================================

        /// 渲染器是否已初始化
        bool m_isInitialized = false;

        /// 当前帧的渲染统计
        struct RenderStats
        {
            uint32_t passesExecuted  = 0;
            uint32_t bufferFlips     = 0;
            uint32_t textureBinds    = 0;
            float    totalRenderTime = 0.0f;
        } m_renderStats;

    public:
        /// Do we currently support use to upload custom image or using bindless first upload texture
        /// to slot and when using shader, shader could directly read texture form descriptor heap?
        ///
        /// Things that we differed from Iris in parameter
        /// @computes we packed into shaderSources
        /// @holder we use bindless that do not need this ?
        /// @noiseTexture currently not supported or upload through bindless custom image id?
        /// @updateNotifier not used
        /// @centerDepthSampler not implemented need investigate what it is.
        /// @customTextureIds still using bindless to upload the index ?
        /// @customUniforms custom buffer upload, user defined buffer?
        /// 
        explicit CompositeRenderer(
            std::shared_ptr<IWorldRenderingPipeline>    pipeline,
            CompositePass                               compositePass,
            PackDirectives                              packDirectives,
            std::vector<std::shared_ptr<ShaderSource>>& shaderSources,
            std::shared_ptr<RenderTargets>              renderTargets,
            std::shared_ptr<BufferFlipper>              buffer,
            std::shared_ptr<ShadowRenderTargets>        shadowTargets,
            TextureStage                                textureState
        );

        /**
         * @brief 析构函数
         * 
         * 自动清理所有资源，智能指针确保安全释放。
         */
        ~CompositeRenderer();

        // 禁用拷贝和移动，确保渲染器的唯一性
        CompositeRenderer(const CompositeRenderer&)            = delete;
        CompositeRenderer& operator=(const CompositeRenderer&) = delete;
        CompositeRenderer(CompositeRenderer&&)                 = delete;
        CompositeRenderer& operator=(CompositeRenderer&&)      = delete;


        /**
         * @brief 配置合成Pass序列
         * 
         * 基于当前加载的着色器包配置所有的composite和deferred程序。
         * 这个方法会扫描着色器包中的所有程序并创建对应的Pass描述。
         * 
         * @param shaderPackPath 着色器包路径
         * @return 成功配置的Pass数量
         * 
         * 教学价值：动态配置系统的设计
         */
        uint32_t ConfigurePassesFromShaderPack(const std::string& shaderPackPath);


        /**
         * @brief 移除指定Pass
         * 
         * 从Pass序列中移除指定的Pass。
         * 
         * @param passName Pass名称
         * @return true如果移除成功
         */
        bool RemovePass(const std::string& passName);

        // ===========================================
        // 主要渲染方法
        // ===========================================

        /**
         * @brief 执行所有合成Pass
         * 
         * 这是CompositeRenderer的主要方法，按顺序执行所有配置的
         * composite和deferred程序。包括：
         * 1. 准备渲染目标
         * 2. 执行每个Pass
         * 3. 处理缓冲区翻转
         * 4. 更新统计信息
         * 
         * 对应Iris: render() 方法
         * 
         * 教学重点：多Pass渲染的组织和执行
         */
        void RenderAll();

        /**
         * @brief 执行延迟光照Pass
         * 
         * 专门执行deferred1-99程序，处理延迟光照计算。
         * 这是Iris架构中延迟光照的实际实现位置。
         * 
         * @param gBufferMask G-Buffer输入掩码
         * 
         * 关键发现：延迟光照在Iris中是CompositeRenderer的一部分！
         */
        void RenderDeferredLighting(uint32_t gBufferMask);

        /**
         * @brief 执行计算着色器Pass
         * 
         * 执行所有的计算着色器程序，用于高级的GPU计算效果。
         * 
         * 教学价值：现代GPU计算能力的利用
         */
        void RenderComputePasses();

        /**
         * @brief 执行单个Pass
         * 
         * 执行指定索引的Pass，包括所有必要的设置和清理。
         * 
         * @param passIndex Pass在序列中的索引
         * @return true如果执行成功
         */
        bool RenderPass(uint32_t passIndex);

        // ===========================================
        // 缓冲区管理
        // ===========================================

        /**
         * @brief 执行缓冲区翻转
         * 
         * 在前后缓冲区之间进行切换，这是乒乓缓冲技术的核心。
         * 对应Iris: BufferFlipper.flip()
         * 
         * 教学重点：乒乓缓冲区在多Pass渲染中的作用
         */
        void FlipBuffers();

        /**
         * @brief 获取当前渲染目标
         * 
         * 返回当前活跃的渲染目标纹理。
         * 
         * @param index 渲染目标索引 (0-15)
         * @return 当前渲染目标的指针
         */
        D12Texture* GetCurrentRenderTarget(uint32_t index) const;

        /**
         * @brief 获取备用渲染目标
         * 
         * 返回当前非活跃的渲染目标纹理，用于乒乓缓冲。
         * 
         * @param index 渲染目标索引 (0-15)
         * @return 备用渲染目标的指针
         */
        D12Texture* GetAlternateRenderTarget(uint32_t index) const;

        // ===========================================
        // 状态查询和控制
        // ===========================================

        /**
         * @brief 检查是否有计算着色器支持
         * 
         * 查询当前硬件和驱动是否支持计算着色器。
         * 
         * @return true如果支持计算着色器
         */
        bool HasComputeSupport() const { return m_supportsComputeShaders; }

        /**
         * @brief 获取配置的Pass数量
         * 
         * 返回当前配置的总Pass数量。
         * 
         * @return Pass总数
         */
        uint32_t GetPassCount() const;


        /**
         * @brief 检查指定Pass是否启用
         * 
         * 查询某个Pass当前是否处于启用状态。
         * 
         * @param passName Pass名称
         * @return true如果Pass已启用
         */
        bool IsPassEnabled(const std::string& passName) const;

        /**
         * @brief 启用或禁用指定Pass
         * 
         * 动态控制Pass的执行，用于调试和优化。
         * 
         * @param passName Pass名称
         * @param enabled 是否启用
         */
        void SetPassEnabled(const std::string& passName, bool enabled);

        // ===========================================
        // 统计和调试
        // ===========================================

        /**
         * @brief 获取渲染统计信息
         * 
         * 返回当前帧的渲染统计数据，用于性能分析。
         * 
         * @return 格式化的统计信息字符串
         */
        std::string GetRenderingStats() const;

        /**
         * @brief 重置渲染统计
         * 
         * 清零所有统计计数器，开始新的统计周期。
         */
        void ResetStats();

        /**
         * @brief 启用调试模式
         * 
         * 开启详细的调试信息输出和验证检查。
         * 
         * @param enable 是否启用调试
         */
        void SetDebugMode(bool enable);

        // ===========================================
        // 资源管理
        // ===========================================

        /**
         * @brief 重新加载所有Pass
         * 
         * 当着色器包更新时，重新配置所有Pass。
         */
        void ReloadPasses();

        /**
         * @brief 销毁渲染器资源
         * 
         * 释放所有GPU资源和系统资源。
         */
        void Destroy();

        /**
         * @brief 验证Pass配置
         * 
         * 检查当前Pass配置的有效性和一致性。
         * 
         * @return true如果配置有效
         */
        bool ValidateConfiguration() const;
    };
} // namespace enigma::graphic
