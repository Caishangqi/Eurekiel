#pragma once

#include <memory>
#include <vector>
#include <string>

#include "Engine/Graphic/Shader/ShaderPack/Properties/PackDirectives.hpp"

namespace enigma::graphic
{
    // Forward declarations - DebugRenderer需要的类型
    class ShaderPackManager;
    class UniformManager;
    class CommandListManager;
    class RenderTargets;
    class ShaderSource;
    class IWorldRenderingPipeline;
    class RenderCommandQueue;

    /**
     * @brief Iris兼容的调试渲染器 - MVP验证版本
     *
     * 严格按照Iris源码的CompositeRenderer构造模式设计：
     * new CompositeRenderer(this, CompositePass.DEBUG, programSet, renderTargets, TextureStage.DEBUG)
     *
     * 这个渲染器专门用于开发和测试DirectX 12渲染管线的核心功能：
     * - Bindless纹理注册和使用验证
     * - 基础几何体渲染 (三角形、四边形)
     * - Immediate模式指令执行验证
     * - Uniform系统测试
     * - 管线状态调试和可视化
     * - 性能分析和统计
     *
     * 管线执行位置：
     * EndFrame() 中的 WorldRenderingPhase::DEBUG 阶段
     * - 在OUTLINE之后执行 (覆盖在所有游戏内容之上)
     * - 在Composite之前执行 (可被后处理效果处理)
     * - 直接渲染到colortex0
     *
     * 设计原则：
     * - 遵循Iris的RAII构造模式
     * - 最小可行原型(MVP)验证
     * - 专注核心功能，忽略复杂特性
     * - 便于迭代开发和调试
     * - 为完整管线打基础
     *
     * 教学要点：
     * - 理解Iris渲染器的构造模式
     * - 学习调试渲染器的设计思路
     * - 掌握渲染管线的验证方法
     * - 体会自顶向下的开发策略
     * - 理解WorldRenderingPhase的Phase系统
     *
     * Iris架构对应：
     * Java: CompositeRenderer with DEBUG pass
     * C++:  DebugRenderer (specialized CompositeRenderer)
     */
    class DebugRenderer final
    {
    public:
        // ===========================================
        // RAII构造 - 严格遵循Iris模式
        // ===========================================

        /**
         * @brief 构造函数 - RAII模式，构造完成即可用
         *
         * 对应Iris CompositeRenderer构造模式的简化版本：
         * new CompositeRenderer(
         *     IrisRenderingPipeline pipeline,
         *     CompositePass.DEBUG,              // ❌ 固定为DEBUG，移除参数
         *     PackDirectives packDirectives,    // ✅ 保留
         *     List<ShaderSource> shaderSources, // ✅ 保留
         *     RenderTargets renderTargets,      // ✅ 保留
         *     BufferFlipper buffer,             // ❌ DebugRenderer不使用乒乓缓冲
         *     ShadowRenderTargets shadowTargets,// ❌ DebugRenderer不使用Shadow
         *     TextureStage.DEBUG                // ❌ 固定为DEBUG，移除参数
         * )
         *
         * DebugRenderer专注于MVP验证，移除了以下Iris参数：
         * - compositePass: 固定为CompositePass::DEBUG
         * - textureState: 固定为TextureStage::DEBUG
         * - buffer: DebugRenderer不使用双缓冲机制
         * - shadowTargets: DebugRenderer不涉及阴影渲染
         *
         * Bindless架构下的额外简化：
         * - ❌ holder (SamplerHolder) - Bindless不需要，采样器使用固定Sampler Heap
         * - ✅ noiseTexture - 通过Bindless custom image上传，固定index引用
         * - ✅ customTextureIds - Bindless index数组
         * - ✅ customUniforms - Root Constants或Structured Buffer
         * - ⚠️ centerDepthSampler - Iris中心深度采样优化 (TODO: Milestone 3.3)
         * - ✅ updateNotifier - 使用FrameUpdateListener机制
         *
         * @param pipeline 渲染管线实例 - 用于访问全局状态和Uniform
         * @param packDirectives Shader Pack配置指令 - 控制渲染行为
         * @param shaderSources DEBUG着色器程序列表 - debug.vsh/debug.fsh
         * @param renderTargets RenderTargets管理器 - colortex0-15和depth
         */
        explicit DebugRenderer(
            std::shared_ptr<IWorldRenderingPipeline>    pipeline,
            PackDirectives                              packDirectives,
            std::vector<std::shared_ptr<ShaderSource>>& shaderSources,
            std::shared_ptr<RenderTargets>              renderTargets
        );

        /**
         * @brief 析构函数 - RAII自动清理
         *
         * 智能指针确保所有资源安全释放，无需手动管理。
         */
        ~DebugRenderer();

        // 禁用拷贝和移动，确保渲染器唯一性
        DebugRenderer()                                = delete;
        DebugRenderer(const DebugRenderer&)            = delete;
        DebugRenderer& operator=(const DebugRenderer&) = delete;
        DebugRenderer(DebugRenderer&&)                 = delete;
        DebugRenderer& operator=(DebugRenderer&&)      = delete;

    private:
        // ===========================================
        // 核心资源管理 - 与CompositeRenderer一致
        // ===========================================

        /// 渲染管线引用 - 用于访问全局状态和Uniform
        std::shared_ptr<IWorldRenderingPipeline> m_pipeline;

        /// 渲染目标管理器 - 管理colortex0-15 (Debug直接写入colortex0)
        std::shared_ptr<RenderTargets> m_renderTargets;

        /// 命令列表管理器 - 用于提交GPU命令
        std::shared_ptr<CommandListManager> m_commandManager;

        /// 着色器包管理器 - 管理DEBUG着色器程序
        std::shared_ptr<ShaderPackManager> m_shaderManager;

        /// Uniform变量管理器 - 管理全局和自定义Uniform
        std::shared_ptr<UniformManager> m_uniformManager;

        /// 渲染指令队列 - 用于Immediate模式指令提取
        std::shared_ptr<RenderCommandQueue> m_commandQueue;

        // ===========================================
        // Pass配置 - DEBUG Pass特定
        // ===========================================

        /// Shader Pack配置指令
        PackDirectives m_packDirectives;

        // 注：compositePass和textureStage固定为DEBUG，不需要存储
        // - CompositePass::DEBUG (固定值)
        // - TextureStage::DEBUG (固定值)

        // ===========================================
        // 调试功能状态
        // ===========================================

        /// 是否启用几何体渲染测试 (三角形、四边形)
        bool m_enableGeometryTest = true;

        /// 是否启用Bindless纹理测试
        bool m_enableTextureTest = true;

        /// 是否启用Immediate指令测试
        bool m_enableImmediateTest = true;

        /// 是否启用性能统计显示
        bool m_enablePerformanceStats = true;

        /// 调试模式启用标志
        bool m_debugMode = false;

        // ===========================================
        // 渲染统计
        // ===========================================

        /// 当前帧渲染统计
        struct DebugRenderStats
        {
            uint32_t geometryDrawCalls = 0; ///< 几何体绘制调用次数
            uint32_t texturesUsed      = 0; ///< 使用的纹理数量
            uint32_t commandsExecuted  = 0; ///< 执行的Immediate指令数
            float    totalRenderTime   = 0.0f; ///< 总渲染时间(ms)
        } m_renderStats;

    public:
        // ===========================================
        // 主要渲染方法
        // ===========================================

        /**
         * @brief 执行所有调试渲染
         *
         * 这是DebugRenderer的主要方法，在WorldRenderingPhase::DEBUG阶段调用。
         * 执行流程：
         * 1. 准备渲染目标 (colortex0)
         * 2. 设置Debug着色器程序
         * 3. 渲染测试几何体 (三角形、四边形)
         * 4. 执行Bindless纹理测试
         * 5. 执行Immediate模式指令
         * 6. 渲染性能统计信息
         * 7. 更新统计数据
         *
         * 对应EnigmaRenderingPipeline::ExecuteDebugStage()调用
         *
         * 教学重点：MVP验证流程的组织
         */
        void RenderAll();

        /**
         * @brief 渲染测试几何体
         *
         * 绘制基础几何体验证渲染管线基本功能：
         * - 红色三角形 (左上角)
         * - 绿色四边形 (右上角)
         * - 蓝色全屏四边形 (测试清屏功能)
         *
         * 教学价值：验证顶点缓冲、索引缓冲、管线状态
         */
        void RenderTestGeometry();

        /**
         * @brief 执行Bindless纹理测试
         *
         * 验证Bindless纹理系统的核心功能：
         * - 纹理注册到全局Descriptor Heap
         * - 通过index引用纹理
         * - 采样器使用
         * - 纹理混合
         *
         * 教学价值：验证Shader Model 6.6 Bindless架构
         */
        void RenderBindlessTextureTest();

        /**
         * @brief 执行Immediate模式指令
         *
         * 从RenderCommandQueue提取DEBUG Phase的所有指令并执行：
         * - DrawIndexed指令
         * - DrawInstanced指令
         * - 自定义渲染指令
         *
         * 教学价值：验证Immediate模式指令队列系统
         */
        void ExecuteImmediateCommands();

        /**
         * @brief 渲染性能统计信息
         *
         * 在屏幕左上角渲染调试信息：
         * - FPS和帧时间
         * - 绘制调用次数
         * - 纹理使用统计
         * - 指令执行数量
         *
         * 教学价值：调试信息可视化
         */
        void RenderPerformanceStats();

        // ===========================================
        // 调试控制
        // ===========================================

        /**
         * @brief 启用/禁用几何体测试
         *
         * @param enable 是否启用
         */
        void SetGeometryTestEnabled(bool enable) { m_enableGeometryTest = enable; }

        /**
         * @brief 启用/禁用Bindless纹理测试
         *
         * @param enable 是否启用
         */
        void SetTextureTestEnabled(bool enable) { m_enableTextureTest = enable; }

        /**
         * @brief 启用/禁用Immediate指令测试
         *
         * @param enable 是否启用
         */
        void SetImmediateTestEnabled(bool enable) { m_enableImmediateTest = enable; }

        /**
         * @brief 启用/禁用性能统计显示
         *
         * @param enable 是否启用
         */
        void SetPerformanceStatsEnabled(bool enable) { m_enablePerformanceStats = enable; }

        /**
         * @brief 启用调试模式
         *
         * 开启详细的调试信息输出和验证检查。
         *
         * @param enable 是否启用调试
         */
        void SetDebugMode(bool enable) { m_debugMode = enable; }

        // ===========================================
        // 统计和查询
        // ===========================================

        /**
         * @brief 获取渲染统计信息
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

        // ===========================================
        // 资源管理
        // ===========================================

        /**
         * @brief 销毁渲染器资源
         *
         * 释放所有GPU资源和系统资源。
         * 注：RAII设计下通常由析构函数自动调用。
         */
        void Destroy();

    private:
        // ===========================================
        // 内部辅助方法
        // ===========================================

        /**
         * @brief 准备DEBUG渲染目标
         *
         * 设置colortex0为当前渲染目标，准备深度缓冲。
         */
        void PrepareRenderTargets();

        /**
         * @brief 绑定DEBUG着色器程序
         *
         * 从ShaderPackManager获取debug.fsh/debug.vsh程序并绑定。
         *
         * @return true如果着色器绑定成功
         */
        bool BindDebugShaderProgram();

        /**
         * @brief 更新Debug Uniform变量
         *
         * 更新frameCounter, frameTime等全局Uniform。
         */
        void UpdateDebugUniforms();

        /**
         * @brief 记录调试信息
         *
         * @param message 调试消息
         */
        void LogDebugInfo(const std::string& message) const;
    };
} // namespace enigma::graphic
