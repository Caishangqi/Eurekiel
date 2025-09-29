#pragma once

#include <memory>
#include <vector>
#include <string>

#include "Engine/Graphic/Resource/Buffer/BufferFlipper.hpp"


namespace enigma::graphic
{
    class ShaderPackManager;
    class UniformManager;
    class D12RenderTargets;
    class CommandListManager;
    class D12Texture;
    class D12Buffer;
}

namespace enigma::graphic
{
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
        /**
         * @brief 合成Pass的类型
         * 
         * 定义不同类型的合成Pass，对应Iris中的不同程序类型
         */
        enum class PassType : uint32_t
        {
            /// 标准的composite程序 (composite1, composite2, etc.)
            Composite = 0,
            /// 延迟光照程序 (deferred1, deferred2, etc.)
            Deferred,
            /// 计算着色器程序 (用于高级效果)
            Compute,
            /// 最终输出程序 (final)
            Final
        };

        /**
         * @brief 单个合成Pass的描述
         * 
         * 包含执行一个Pass所需的所有信息，对应Iris中的
         * CompositePass类的功能。
         */
        struct PassDescription
        {
            /// Pass的名称 (如: "composite1", "deferred3")
            std::string name;

            /// Pass的类型
            PassType type = PassType::Composite;

            /// Pass的索引号 (1-99)
            uint32_t index = 1;

            /// 是否启用此Pass
            bool enabled = true;

            /// 输入渲染目标掩码 (指定哪些RT作为输入)
            uint32_t inputTargetMask = 0;

            /// 输出渲染目标掩码 (指定渲染到哪些RT)
            uint32_t outputTargetMask = 0;

            /// 是否需要深度缓冲区
            bool requiresDepth = true;

            /// 是否需要在Pass后执行缓冲区翻转
            bool flipBuffers = true;

            /// 自定义uniform变量
            std::vector<std::string> customUniforms;

            /**
             * @brief 默认构造函数
             */
            PassDescription();

            /**
             * @brief 便利构造函数
             * 
             * @param passName Pass名称
             * @param passType Pass类型
             * @param passIndex Pass索引
             */
            PassDescription(const std::string& passName, PassType passType, uint32_t passIndex)
                : name(passName), type(passType), index(passIndex)
            {
            }
        };

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

        /// 所有注册的Pass描述列表
        std::vector<PassDescription> m_compositePasses;

        /// 延迟光照Pass描述列表
        std::vector<PassDescription> m_deferredPasses;

        /// 计算着色器Pass描述列表
        std::vector<PassDescription> m_computePasses;

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
        /**
         * @brief 构造函数
         * 
         * 初始化CompositeRenderer，设置所有必要的资源引用。
         * 
         * @param renderTargets 渲染目标管理器
         * @param commandManager 命令列表管理器
         * @param shaderManager 着色器包管理器
         * @param uniformManager Uniform变量管理器
         * 
         * 教学重点：依赖注入在复杂系统中的应用
         */
        CompositeRenderer(
            std::shared_ptr<D12RenderTargets>   renderTargets,
            std::shared_ptr<CommandListManager> commandManager,
            std::shared_ptr<ShaderPackManager>  shaderManager,
            std::shared_ptr<UniformManager>     uniformManager);

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

        // ===========================================
        // 初始化和配置
        // ===========================================

        /**
         * @brief 初始化渲染器
         * 
         * 执行所有必要的初始化操作：
         * 1. 创建BufferFlipper
         * 2. 检测硬件能力
         * 3. 设置默认渲染状态
         * 
         * @return true如果初始化成功
         */
        bool Initialize();

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
         * @brief 手动添加Pass
         * 
         * 允许手动添加自定义的Pass，用于测试或特殊效果。
         * 
         * @param passDesc Pass描述
         * @return true如果添加成功
         */
        bool AddPass(const PassDescription& passDesc);

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
         * @brief 获取指定Pass的信息
         * 
         * 返回指定索引Pass的详细描述信息。
         * 
         * @param index Pass索引
         * @return Pass描述的常量引用
         */
        const PassDescription& GetPassDescription(uint32_t index) const;

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

    private:
        // ===========================================
        // 内部辅助方法
        // ===========================================

        /**
         * @brief 设置Pass的渲染目标
         * 
         * 根据Pass描述配置对应的渲染目标绑定。
         * 
         * @param passDesc Pass描述
         */
        void SetupRenderTargets(const PassDescription& passDesc);

        /**
         * @brief 绑定Pass的纹理输入
         * 
         * 将指定的渲染目标作为纹理绑定到着色器。
         * 
         * @param passDesc Pass描述
         */
        void BindInputTextures(const PassDescription& passDesc);

        /**
         * @brief 更新Pass的Uniform变量
         * 
         * 设置Pass执行所需的所有uniform变量。
         * 
         * @param passDesc Pass描述
         */
        void UpdateUniforms(const PassDescription& passDesc);

        /**
         * @brief 执行Pass的渲染命令
         * 
         * 提交实际的GPU渲染命令。
         * 
         * @param passDesc Pass描述
         * @return true如果执行成功
         */
        bool ExecuteRenderCommands(const PassDescription& passDesc);

        /**
         * @brief 清理Pass资源
         * 
         * Pass执行完成后的资源清理。
         * 
         * @param passDesc Pass描述
         */
        void CleanupPassResources(const PassDescription& passDesc);

        /**
         * @brief 从着色器包解析Pass配置
         * 
         * 解析着色器包中的composite和deferred程序配置。
         * 
         * @param shaderPackPath 着色器包路径
         * @return 解析到的Pass描述列表
         */
        std::vector<PassDescription> ParsePassesFromShaderPack(const std::string& shaderPackPath);
    };
} // namespace enigma::graphic
