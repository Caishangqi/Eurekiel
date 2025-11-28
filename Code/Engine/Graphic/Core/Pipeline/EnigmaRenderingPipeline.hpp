#pragma once

#include "IShaderRenderingPipeline.hpp"
#include "WorldRenderingPhase.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <atomic>

namespace enigma::graphic
{
    class D12RenderTargets;
    class BufferFlipper;
    class CommandListManager;
    class CompositeRenderer;
    class ShadowRenderer;
    class DebugRenderer;
    class ShaderPackManager;
    class UniformManager;
}

namespace enigma::graphic
{
    /**
     * @brief Iris兼容的Enigma着色器渲染管线
     * 
     * 这个类直接对应Iris源码中的IrisRenderingPipeline.java，是整个
     * 着色器渲染系统的核心协调器。它管理多个专门的子渲染器，实现
     * 复杂的多阶段渲染管线。
     * 
     * 基于Iris源码的关键发现：
     * - IrisRenderingPipeline有4个CompositeRenderer成员变量
     * - beginRenderer, prepareRenderer, deferredRenderer, compositeRenderer
     * - 还有ShadowRenderer用于阴影渲染
     * - 延迟光照通过deferredRenderer CompositeRenderer处理
     * 
     * 教学目标：
     * - 理解复杂渲染管线的组织和协调
     * - 学习多子渲染器的管理模式
     * - 掌握阶段分发和任务协调的设计
     * - 体会现代图形管线的模块化设计
     * 
     * Iris架构对应：
     * Java: public class IrisRenderingPipeline implements WorldRenderingPipeline, ShaderRenderingPipeline
     * C++:  class EnigmaRenderingPipeline : public IShaderRenderingPipeline
     * 
     * 核心成员变量（基于Iris源码）：
     * - CompositeRenderer beginRenderer     (处理begin阶段)
     * - CompositeRenderer prepareRenderer   (处理prepare阶段)
     * - CompositeRenderer deferredRenderer  (处理延迟光照)
     * - CompositeRenderer compositeRenderer (处理后处理)
     * - ShadowRenderer shadowRenderer       (处理阴影渲染)
     */
    class EnigmaRenderingPipeline final : public IShaderRenderingPipeline
    {
    private:
        // ===========================================
        // 多子渲染器架构 - 对应Iris源码结构
        // ===========================================

        /// Begin阶段合成渲染器 - 对应Iris beginRenderer
        std::unique_ptr<CompositeRenderer> m_beginRenderer;

        /// Prepare阶段合成渲染器 - 对应Iris prepareRenderer
        std::unique_ptr<CompositeRenderer> m_prepareRenderer;

        /// 延迟光照合成渲染器 - 对应Iris deferredRenderer
        /// 关键发现：延迟光照在Iris中是通过CompositeRenderer实现的！
        std::unique_ptr<CompositeRenderer> m_deferredRenderer;

        /// 后处理合成渲染器 - 对应Iris compositeRenderer
        std::unique_ptr<CompositeRenderer> m_compositeRenderer;

        /// 阴影渲染器 - 对应Iris shadowRenderer
        std::unique_ptr<ShadowRenderer> m_shadowRenderer;

        /// 调试渲染器 - Enigma扩展，用于开发和测试 (Milestone 2.6新增)
        /// 严格按照Iris的5参数模式构造：(this, CompositePass::DEBUG, nullptr, renderTargets, TextureStage::DEBUG)
        std::unique_ptr<DebugRenderer> m_debugRenderer;

        // ===========================================
        // 核心系统组件
        // ===========================================

        /// 渲染目标管理器 - 统一管理所有colortex0-15
        std::shared_ptr<D12RenderTargets> m_renderTargets;

        /// 缓冲区翻转器 - 全局乒乓缓冲管理
        std::shared_ptr<BufferFlipper> m_bufferFlipper;

        /// 命令列表管理器
        CommandListManager* m_commandManager;

        /// 着色器包管理器
        std::shared_ptr<ShaderPackManager> m_shaderPackManager;

        /// Uniform变量管理器
        std::shared_ptr<UniformManager> m_uniformManager;

        // ===========================================
        // 管线状态管理
        // ===========================================

        /// 当前渲染阶段
        WorldRenderingPhase m_currentPhase = WorldRenderingPhase::NONE;

        /// 管线是否处于激活状态
        std::atomic<bool> m_isActive{false};

        /// 管线是否已初始化
        bool m_isInitialized = false;

        /// 是否启用调试模式
        bool m_debugMode = false;

        // ===========================================
        // 渲染配置
        // ===========================================

        /// 当前着色器包名称
        std::string m_currentShaderPackName;

        /// 着色器包是否已启用
        bool m_shaderPackEnabled = false;

        /// 着色器渲染距离（-1表示默认）
        float m_shaderRenderDistance = -1.0f;

        /// 是否禁用原版雾效
        bool m_disableVanillaFog = false;

        /// 是否禁用原版定向阴影
        bool m_disableDirectionalShading = false;

        // ===========================================
        // 帧更新和回调管理
        // ===========================================

        /// 帧更新监听器列表
        std::vector<std::pair<size_t, std::function<void()>>> m_frameUpdateListeners;

        /// 下一个回调ID
        std::atomic<size_t> m_nextCallbackId{1};

        // ===========================================
        // 性能统计
        // ===========================================

        struct PipelineStats
        {
            uint32_t framesRendered   = 0;
            uint32_t phaseSwitches    = 0;
            uint32_t bufferFlips      = 0;
            float    totalFrameTime   = 0.0f;
            float    averageFrameTime = 0.0f;
        } m_stats;

    public:
        /**
         * @brief 构造函数
         *
         * 初始化Enigma渲染管线，创建所有子渲染器和核心系统组件。
         *
         * @param commandManager 命令列表管理器（非拥有指针，由D3D12RenderSystem管理生命周期）
         *
         * 教学重点：
         * - EnigmaRenderingPipeline不拥有CommandListManager，只是使用它
         * - D3D12RenderSystem负责CommandListManager的生命周期管理
         * - 这种设计避免了循环依赖和所有权混乱
         */
        explicit EnigmaRenderingPipeline(CommandListManager* commandManager);

        /**
         * @brief 析构函数
         * 
         * 自动清理所有子渲染器和资源，智能指针确保安全释放。
         */
        ~EnigmaRenderingPipeline() override;

        // 禁用拷贝和移动，确保管线的唯一性
        EnigmaRenderingPipeline(const EnigmaRenderingPipeline&)            = delete;
        EnigmaRenderingPipeline& operator=(const EnigmaRenderingPipeline&) = delete;
        EnigmaRenderingPipeline(EnigmaRenderingPipeline&&)                 = delete;
        EnigmaRenderingPipeline& operator=(EnigmaRenderingPipeline&&)      = delete;

        // ===========================================
        // IWorldRenderingPipeline接口实现
        // ===========================================

        /**
         * @brief 开始世界渲染
         * 
         * 初始化整个着色器渲染管线：
         * 1. 准备所有渲染目标
         * 2. 初始化子渲染器
         * 3. 设置全局uniform变量
         * 4. 开始性能统计
         */
        void BeginWorldRendering() override;

        /**
         * @brief 结束世界渲染
         * 
         * 完成渲染管线的所有收尾工作：
         * 1. 执行最终的composite pass
         * 2. 提交所有GPU命令
         * 3. 更新性能统计
         * 4. 准备下一帧
         */
        void EndWorldRendering() override;

        /**
         * @brief 设置当前渲染阶段
         * 
         * 这是EnigmaRenderingPipeline的核心方法，负责将不同的渲染阶段
         * 分发给对应的子渲染器处理。基于Iris的复杂分发逻辑。
         * 
         * @param phase 目标渲染阶段
         * 
         * 教学重点：状态机模式和命令分发模式的结合
         */
        void SetPhase(WorldRenderingPhase phase) override;

        void                BeginPass(uint32_t passIndex = 0) override;
        void                EndPass() override;
        void                BeginLevelRendering() override;
        void                RenderShadows() override;
        void                EndLevelRendering() override;
        bool                ShouldDisableVanillaFog() const override;
        bool                ShouldDisableDirectionalShading() const override;
        float               GetShaderRenderDistance() const override;
        WorldRenderingPhase GetCurrentPhase() const override;
        bool                IsActive() const override;
        void                OnFrameUpdate() override;
        void                Reload() override;
        void                Destroy() override;

        // ===========================================
        // IShaderRenderingPipeline接口实现
        // ===========================================

        std::shared_ptr<ShaderPackManager> GetShaderPackManager() const override;
        std::shared_ptr<UniformManager>    GetUniformManager() const override;
        bool                               UseProgram(const std::string& programName) override;
        bool                               HasProgram(const std::string& programName) const override;
        bool                               ReloadShaders() override;
        void                               AddFrameUpdateListener(std::function<void()> callback) override;
        void                               RemoveFrameUpdateListener(size_t callbackId) override;
        void*                              GetColorTexture(uint32_t index) const override;
        void*                              GetDepthTexture() const override;
        void                               FlipBuffers() override;
        std::string                        GetCurrentShaderPackName() const override;
        bool                               IsShaderPackEnabled() const override;
        std::string                        GetShaderPackVersion() const override;
        bool                               SetShaderPackOption(const std::string& optionName, const std::string& value) override;
        std::string                        GetShaderPackOption(const std::string& optionName) const override;
        void                               SetDebugMode(bool enable) override;
        std::string                        GetRenderingStats() const override;

        // ===========================================
        // Enigma特有的高级功能
        // ===========================================

        /**
         * @brief 加载着色器包
         * 
         * 加载指定的着色器包并重新配置所有子渲染器。
         * 
         * @param shaderPackPath 着色器包路径
         * @return true如果加载成功
         */
        bool LoadShaderPack(const std::string& shaderPackPath);

        /**
         * @brief 获取指定的子渲染器
         * 
         * 返回指定类型的子渲染器，用于高级控制和调试。
         * 
         * @param rendererType 渲染器类型名称
         * @return 渲染器指针，nullptr表示不存在
         */
        CompositeRenderer* GetSubRenderer(const std::string& rendererType) const;

        /**
         * @brief 获取阴影渲染器
         * 
         * 返回阴影渲染器的指针，用于阴影相关的配置。
         * 
         * @return 阴影渲染器指针
         */
        ShadowRenderer* GetShadowRenderer() const { return m_shadowRenderer.get(); }

        /**
         * @brief 强制重新配置所有子渲染器
         * 
         * 当着色器包或设置发生重大变化时，重新配置所有子渲染器。
         */
        void ReconfigureAllRenderers();

        /**
         * @brief 设置渲染质量级别
         * 
         * 根据质量级别调整各种渲染参数和效果。
         * 
         * @param qualityLevel 质量级别 (0-4, 4为最高)
         */
        void SetRenderingQuality(uint32_t qualityLevel);

        /**
         * @brief 启用或禁用特定的渲染阶段
         *
         * 用于调试和性能优化，可以选择性禁用某些阶段。
         *
         * @param phase 渲染阶段
         * @param enabled 是否启用
         */
        void SetPhaseEnabled(WorldRenderingPhase phase, bool enabled);

        /**
         * @brief 执行Debug阶段 - 使用debugRenderer (Milestone 2.6新增)
         *
         * 这是Enigma扩展的调试渲染阶段，用于验证渲染管线核心功能。
         *
         * 教学要点：
         * - Iris中SetPhase()只设置标志位，不执行渲染
         * - 渲染逻辑由各Renderer的RenderAll()方法处理
         * - ExecuteDebugStage()是为了更好的代码组织而创建的封装
         * - 外部调用：SetPhase(DEBUG) + ExecuteDebugStage()
         */
        void ExecuteDebugStage();

    private:
        // ===========================================
        // 内部初始化方法
        // ===========================================

        /**
         * @brief 初始化渲染目标
         *
         * 创建所有必要的渲染目标和深度缓冲区。
         *
         * @return true如果初始化成功
         */
        bool InitializeRenderTargets();

        // ===========================================
        // 阶段执行方法 - 对应Iris的具体实现
        // ===========================================

        /// 执行Setup阶段 - 对应Iris setup1-99
        void ExecuteSetupStage();

        /// 执行Begin阶段 - 对应Iris begin1-99，使用beginRenderer
        void ExecuteBeginStage();

        /// 执行Shadow阶段 - 使用shadowRenderer
        void ExecuteShadowStage();

        /// 执行ShadowComp阶段 - 对应Iris shadowcomp1-99
        void ExecuteShadowCompStage();

        /// 执行Prepare阶段 - 对应Iris prepare1-99，使用prepareRenderer
        void ExecutePrepareStage();

        /// 执行GBuffer不透明阶段 - 填充G-Buffer
        void ExecuteGBufferOpaqueStage();

        /// 执行延迟光照阶段 - 使用deferredRenderer处理deferred1-99
        void ExecuteDeferredStage();

        /// 执行GBuffer半透明阶段
        void ExecuteGBufferTranslucentStage();

        /// 执行Composite阶段 - 使用compositeRenderer处理composite1-99
        void ExecuteCompositeStage();

        /// 执行Final阶段 - 最终输出
        void ExecuteFinalStage();

        // ===========================================
        // 内部辅助方法
        // ===========================================

        /**
         * @brief 更新所有uniform变量
         * 
         * 更新所有子渲染器需要的uniform变量。
         */
        void UpdateAllUniforms();

        /**
         * @brief 验证管线状态
         * 
         * 检查管线状态的一致性和正确性。
         * 
         * @return true如果状态正常
         */
        bool ValidatePipelineState() const;

        /**
         * @brief 处理着色器编译错误
         * 
         * 统一处理着色器编译和链接错误。
         * 
         * @param error 错误信息
         */
        void HandleShaderError(const std::string& error);

        /**
         * @brief 调用所有帧更新监听器
         * 
         * 通知所有注册的监听器进行帧更新。
         */
        void NotifyFrameUpdateListeners();

        /**
         * @brief 更新性能统计
         *
         * 更新管线的性能统计信息。
         *
         * @param frameTime 当前帧时间
         */
        void UpdatePerformanceStats(float frameTime);
    };
} // namespace enigma::graphic
