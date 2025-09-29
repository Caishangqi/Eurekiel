/**
 * @file RendererSubsystem.hpp
 * @brief Enigma引擎渲染子系统 - DirectX 12延迟渲染管线管理器
 * 
 * 教学重点:
 * 1. 理解Iris真实的管线管理架构（PipelineManager模式）
 * 2. 学习DirectX 12的现代化资源管理
 * 3. 掌握引擎子系统的生命周期管理
 * 4. 理解按维度分离的管线设计理念
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <array>

#include "Engine/Core/SubsystemManager.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Resource/ResourceCommon.hpp"
#include "Engine/Window/Window.hpp"
#include "../Core/DX12/D3D12RenderSystem.hpp"
#include "../Core/Pipeline/IWorldRenderingPipeline.hpp"
#include "../Core/Pipeline/EnigmaRenderingPipeline.hpp"
#include "../Core/Pipeline/ShaderPackManager.hpp"
#include "../Immediate/RenderCommand.hpp"
#include "../Immediate/RenderCommandQueue.hpp"

// 使用Enigma核心命名空间中的EngineSubsystem
using namespace enigma::core;

namespace enigma::graphic
{
    // 前向声明 - 避免循环包含
    class ShaderPackManager;
    class RenderCommandQueue;

    /**
     * @brief DirectX 12渲染子系统管理器
     * @details 
     * 这个类是整个延迟渲染系统的入口点和生命周期管理器。它继承自EngineSubsystem，
     * 与Enigma引擎的其他子系统协同工作。
     * 
     * 教学要点:
     * - 子系统模式的设计模式应用
     * - Iris渲染管线的生命周期管理  
     * - DirectX 12设备和命令队列的初始化
     * - 智能指针在资源管理中的应用
     * 
     * DirectX 12特性:
     * - 显式的GPU资源管理
     * - Command List和Command Queue的分离
     * - Bindless描述符堆的使用
     * - 多线程渲染支持的基础架构
     * 
     * @note 这是教学项目，重点在于理解渲染管线概念而非极限性能
     */
    class RendererSubsystem final : public EngineSubsystem
    {
    public:
        /**
          * @brief Rendering Subsystem Configuration
          * @details contains all parameters required to initialize the renderer
          */
        struct Configuration
        {
            uint32_t    renderWidth             = 1920; ///< render resolution width
            uint32_t    renderHeight            = 1080; ///< Render resolution height
            uint32_t    maxFramesInFlight       = 3; ///< Maximum number of flight frames
            bool        enableDebugLayer        = true; ///< DirectX 12 debug layer
            bool        enableGPUValidation     = true; ///< GPU verification layer
            bool        enableBindlessResources = true; ///< Enable Bindless resources
            std::string defaultShaderPackPath; ///< Default Shader Pack Path

            // ========================== 渲染配置 (Milestone 2.6新增) =========================
            Rgba8   defaultClearColor    = Rgba8::RED; ///< 默认清屏颜色，使用引擎Rgba8系统
            float   defaultClearDepth    = 1.0f; ///< 默认深度清除值 (0.0-1.0, 1.0表示最远)
            uint8_t defaultClearStencil  = 0; ///< 默认模板清除值 (0-255)
            bool    enableAutoClearColor = true; ///< 是否在BeginFrame自动执行清屏
            bool    enableAutoClearDepth = true; ///< 是否在BeginFrame自动清除深度缓冲
            bool    enableShadowMapping  = false; ///< 是否启用阴影映射（后续Milestone实现）

            // ========================== 窗口系统集成 =========================
            Window* targetWindow = nullptr; ///< 目标窗口，用于SwapChain创建

            // ========================== Immediate mode configuration =========================
            bool   enableImmediateMode    = true; ///< Enable immediate mode rendering
            size_t maxCommandsPerPhase    = 10000; ///< Maximum number of instructions per stage
            bool   enablePhaseDetection   = true; ///< Enable automatic phase detection
            bool   enableCommandProfiling = false; ///< Enable directive performance analysis

            /**
             * @brief default constructor
             * @details Set default parameters suitable for teaching and development
             */
            Configuration() = default;
        };

        /**
         * @brief 渲染统计信息
         * @details 用于性能分析和调试的统计数据
         */
        struct RenderStatistics
        {
            uint64_t frameIndex           = 0; ///< 当前帧索引
            float    frameTime            = 0.0f; ///< 帧时间(毫秒)
            float    gpuTime              = 0.0f; ///< GPU时间(毫秒)
            uint32_t drawCalls            = 0; ///< 绘制调用数量
            uint32_t trianglesRendered    = 0; ///< 渲染三角形数量
            uint32_t activeShaderPrograms = 0; ///< 活跃着色器程序数量
            size_t   gpuMemoryUsed        = 0; ///< GPU内存使用量(字节)

            // ==================== Immediate模式统计 ====================
            std::map<WorldRenderingPhase, uint64_t> commandsPerPhase; ///< 每个阶段的指令数量
            uint64_t                                totalCommandsSubmitted = 0; ///< 提交的总指令数量
            uint64_t                                totalCommandsExecuted  = 0; ///< 已执行的总指令数量
            float                                   averageCommandTime     = 0.0f; ///< 平均指令执行时间(微秒)
            WorldRenderingPhase                     currentPhase           = WorldRenderingPhase::NONE; ///< 当前渲染阶段

            /**
             * @brief 重置统计信息
             */
            void Reset()
            {
                drawCalls            = 0;
                trianglesRendered    = 0;
                activeShaderPrograms = 0;

                // 重置immediate模式统计
                commandsPerPhase.clear();
                totalCommandsSubmitted = 0;
                totalCommandsExecuted  = 0;
                averageCommandTime     = 0.0f;
                currentPhase           = WorldRenderingPhase::NONE;
            }
        };

    public:
        /**
         * @brief 构造函数
         * @details 初始化基本成员，但不进行重型初始化工作
         */
        explicit RendererSubsystem(Configuration& config);

        /**
         * @brief 析构函数
         * @details 确保所有资源正确释放，遵循RAII原则
         */
        ~RendererSubsystem() override;

        // ==================== EngineSubsystem接口实现 ====================

        /// 使用引擎提供的宏来简化子系统注册
        DECLARE_SUBSYSTEM(RendererSubsystem, "RendererSubsystem", 80)

        /**
         * @brief 早期初始化阶段
         * @details 
         * 在所有子系统的Startup之前调用，用于：
         * - 委托D3D12RenderSystem初始化DirectX 12设备和命令队列
         * - 设置调试和验证层
         * 
         * 教学要点: 理解为什么图形设备需要最早初始化，以及架构分层的重要性
         */
        void Initialize() override;

        /**
         * @brief 主要启动阶段
         * @details
         * 在Initialize阶段之后调用，用于：
         * - 创建PipelineManager实例
         * - 加载默认Shader Pack
         * - 准备初始渲染管线（主世界维度）
         */
        void Startup() override;

        /**
         * @brief 关闭子系统
         * @details
         * 释放所有管理的资源，确保无内存泄漏：
         * - 释放所有高级渲染组件
         * - 委托D3D12RenderSystem进行底层资源清理
         * 
         * @note 析构函数会调用此方法，但显式调用更安全
         */
        void Shutdown() override;

        bool RequiresInitialize() const override { return true; }

        // ==================== 游戏循环接口 - 对应Iris管线生命周期 ====================

        /**
         * @brief 帧开始处理
         * @details
         * 对应Iris管线的setup和begin阶段：
         * - setup1-99: GPU状态初始化、SSBO设置
         * - begin1-99: 每帧参数更新、摄像机矩阵计算
         * 
         * 教学要点:
         * - 理解为什么每帧开始需要更新全局参数
         * - 学习GPU状态管理的重要性
         * - 掌握Command List的开始记录
         */
        void BeginFrame() override;

        /**
         * @brief 主要更新和渲染
         * @param deltaTime 帧时间间隔(秒)
         * @details
         * 对应Iris管线的主要渲染阶段：
         * - shadow: 阴影贴图生成
         * - shadowcomp1-99: 阴影后处理
         * - prepare1-99: SSAO等预处理
         * - gbuffers(opaque): 不透明几何体G-Buffer填充
         * - deferred1-99: 延迟光照计算
         * - gbuffers(translucent): 半透明几何体前向渲染
         * - composite1-99: 后处理效果链
         * 
         * 教学要点:
         * - 理解延迟渲染的核心概念
         * - 学习G-Buffer的数据组织
         * - 掌握后处理效果的实现原理
         */

        /// @brief 执行完整的渲染帧管线 (基于Iris架构)
        ///
        /// 实现完整的Iris风格渲染流水线：
        /// Begin → Shadow → Prepare(G-Buffer) → Deferred → Translucent → Composite → Debug → Final
        ///
        /// 教学要点:
        /// - 理解现代渲染管线的完整流程
        /// - 学习各个渲染阶段的职责分工
        /// - 掌握延迟渲染的实现架构
        void RenderFrame();

        /// @brief 更新渲染系统逻辑 (简化版本，建议使用RenderFrame())
        /// @deprecated 推荐使用RenderFrame()获得完整的渲染管线体验
        void Update(float deltaTime) override;

        /**
         * @brief 帧结束处理
         * @details
         * 对应Iris管线的final阶段：
         * - final: 最终输出处理
         * - Present: 交换链呈现
         * - 性能统计收集
         * - Command List提交和同步
         * 
         * 教学要点:
         * - 理解双缓冲和垂直同步
         * - 学习GPU/CPU同步的重要性
         */
        void EndFrame() override;

        // ==================== Immediate模式渲染接口 - 用户友好API ====================

        /**
         * @brief 获取渲染指令队列
         * @return RenderCommandQueue指针，如果未初始化返回nullptr
         * @details 
         * 提供对immediate模式渲染指令队列的访问
         * 用户可以通过此接口提交自定义渲染指令
         */
        RenderCommandQueue* GetRenderCommandQueue() const noexcept
        {
            return m_renderCommandQueue.get();
        }

        /**
         * @brief 提交绘制指令到指定阶段
         * @param command 要执行的绘制指令
         * @param phase 目标渲染阶段
         * @param debugTag 调试标签，用于性能分析
         * @return 成功返回true
         * @details 
         * immediate模式的核心接口，支持按阶段分类存储
         */
        bool SubmitRenderCommand(RenderCommandPtr command, WorldRenderingPhase phase, const std::string& debugTag = "");

        /**
         * @brief 提交绘制指令（自动检测阶段）
         * @param command 要执行的绘制指令
         * @param debugTag 调试标签
         * @return 成功返回true
         * @details 使用PhaseDetector自动判断指令应该归属的阶段
         */
        bool SubmitRenderCommand(RenderCommandPtr command, const std::string& debugTag = "");

        /**
         * @brief 立即执行队列中的所有指令
         * @details 
         * 强制同步执行，主要用于：
         * - 帧结束时确保所有指令完成
         * - 调试时的即时执行
         * - 需要同步的特殊情况
         */
        void FlushRenderQueue();

        /**
         * @brief 执行指定阶段的所有指令
         * @param phase 要执行的渲染阶段
         * @details 按阶段执行存储的绘制指令
         */
        void ExecutePhase(WorldRenderingPhase phase);

        /**
         * @brief 按顺序执行所有阶段的指令
         * @details 按Iris标准顺序执行所有有指令的阶段
         */
        void ExecuteAllPhases();

        /**
         * @brief 设置当前渲染阶段
         * @param phase Iris渲染阶段
         * @details 
         * 用于自动Phase检测和状态管理
         * 影响指令验证和性能分析
         */
        void SetCurrentRenderPhase(WorldRenderingPhase phase);

        /**
         * @brief 获取当前渲染阶段
         * @return 当前的Iris渲染阶段
         */
        WorldRenderingPhase GetCurrentRenderPhase() const;

        /**
         * @brief 获取指定阶段的指令数量
         * @param phase 渲染阶段
         * @return 指令数量
         */
        size_t GetCommandCount(WorldRenderingPhase phase) const;

        /**
         * @brief immediate模式便捷绘制接口 - DrawIndexed
         * @param indexCount 索引数量
         * @param phase 渲染阶段
         * @param startIndexLocation 起始索引位置
         * @param baseVertexLocation 基础顶点位置
         * @return 成功返回true
         * @details 直接创建并提交DrawIndexed指令的便捷方法
         */
        bool DrawIndexed(
            uint32_t            indexCount,
            WorldRenderingPhase phase,
            uint32_t            startIndexLocation = 0,
            int32_t             baseVertexLocation = 0
        );

        /**
         * @brief immediate模式便捷绘制接口 - DrawInstanced
         * @param vertexCountPerInstance 每实例顶点数
         * @param instanceCount 实例数量
         * @param phase 渲染阶段
         * @param startVertexLocation 起始顶点位置
         * @param startInstanceLocation 起始实例位置
         * @return 成功返回true
         */
        bool DrawInstanced(
            uint32_t            vertexCountPerInstance,
            uint32_t            instanceCount,
            WorldRenderingPhase phase,
            uint32_t            startVertexLocation   = 0,
            uint32_t            startInstanceLocation = 0
        );

        /**
         * @brief immediate模式便捷绘制接口 - DrawIndexedInstanced
         * @param indexCountPerInstance 每实例索引数
         * @param instanceCount 实例数量
         * @param phase 渲染阶段
         * @param startIndexLocation 起始索引位置
         * @param baseVertexLocation 基础顶点位置
         * @param startInstanceLocation 起始实例位置
         * @return 成功返回true
         */
        bool DrawIndexedInstanced(
            uint32_t            indexCountPerInstance,
            uint32_t            instanceCount,
            WorldRenderingPhase phase,
            uint32_t            startIndexLocation    = 0,
            int32_t             baseVertexLocation    = 0,
            uint32_t            startInstanceLocation = 0
        );

        // ==================== 管线管理接口 - 基于Iris PipelineManager ====================

        /**
         * @brief 准备指定维度的渲染管线
         * @param dimensionId 维度标识符
         * @return 渲染管线指针
         * @details 
         * 对应Iris的preparePipeline(ResourceLocation currentDimension)方法
         * 支持多维度管线缓存和动态切换
         */
        class IWorldRenderingPipeline* PreparePipeline(const enigma::resource::ResourceLocation& dimensionId);

        /**
         * @brief 获取Shader Pack管理器
         * @return Shader Pack管理器指针
         * @details 用于加载、管理和切换不同的着色器包
         */
        ShaderPackManager* GetShaderPackManager() const noexcept
        {
            return m_shaderPackManager.get();
        }

        /**
         * @brief 获取当前活跃的渲染管线 (Milestone 2.6新增)
         * @return 当前EnigmaRenderingPipeline指针
         * @details
         * 用于访问调试渲染器和其他管线功能。
         * 主要供TestGame等测试代码使用。
         */
        EnigmaRenderingPipeline* GetCurrentPipeline() const noexcept
        {
            return m_currentPipeline.get();
        }

        // ==================== 配置和状态查询 ====================

        /**
         * @brief 更新渲染配置
         * @param config 新的配置参数
         * @details 某些配置更改可能需要重新创建资源
         */
        void UpdateConfiguration(const Configuration& config);

        /**
         * @brief 获取当前配置
         * @return 当前的配置参数
         */
        const Configuration& GetConfiguration() const noexcept { return m_configuration; }

        /**
         * @brief 调整渲染分辨率
         * @param width 新宽度
         * @param height 新高度
         * @details 处理窗口大小变化，重新创建相关资源
         */
        void ResizeRenderTargets(uint32_t width, uint32_t height);

        /**
         * @brief 获取渲染统计信息
         * @return 当前帧的渲染统计
         * @details 用于性能分析和调试
         */
        const RenderStatistics& GetRenderStatistics() const noexcept
        {
            return m_renderStatistics;
        }

        /**
         * @brief 检查子系统是否已准备好渲染
         * @return true表示可以进行渲染
         */
        bool IsReadyForRendering() const noexcept;

        // ==================== Shader Pack管理接口 ====================

        /**
         * @brief 加载Shader Pack
         * @param packPath Shader Pack文件路径
         * @return true表示加载成功
         * @details 
         * 支持运行时Shader Pack切换，用于：
         * - 不同视觉风格的切换
         * - Shader开发和调试
         * - 性能测试
         */
        bool LoadShaderPack(const std::string& packPath);

        /**
         * @brief 卸载当前Shader Pack
         * @details 回退到默认渲染管线
         */
        void UnloadShaderPack();

        /**
         * @brief 重新加载当前Shader Pack
         * @return true表示重新加载成功
         * @details 用于Shader开发期间的热重载
         */
        bool ReloadShaderPack();

        /**
         * @brief 启用/禁用Shader Pack热重载
         * @param enable true表示启用文件监控
         * @details 开发模式下自动检测Shader文件变化并重新加载
         */
        void EnableShaderHotReload(bool enable);

        // ==================== 调试和开发支持 ====================

        /**
         * @brief 启用/禁用调试渲染
         * @param enable true表示启用调试输出
         * @details 包括线框模式、法线可视化等调试功能
         */
        void EnableDebugRendering(bool enable);

        /**
         * @brief 获取DirectX 12设备
         * @return D3D12设备指针
         * @details 委托给D3D12RenderSystem的全局设备访问
         * @warning 直接操作设备可能导致状态不一致，建议通过高级接口操作
         */
        ID3D12Device* GetD3D12Device() const noexcept
        {
            return D3D12RenderSystem::GetDevice();
        }

        /**
         * @brief 获取主命令队列
         * @return 命令队列指针
         * @details 委托给D3D12RenderSystem获取CommandListManager的图形队列
         */
        ID3D12CommandQueue* GetCommandQueue() const noexcept
        {
            auto* cmdMgr = D3D12RenderSystem::GetCommandListManager();
            return cmdMgr ? cmdMgr->GetCommandQueue(CommandListManager::Type::Graphics) : nullptr;
        }

    private:
        // ==================== 内部初始化方法 ====================

        /**
         * @brief 初始化交换链
         * @param windowHandle 窗口句柄
         * @details 创建与窗口关联的交换链，使用D3D12RenderSystem提供的设备
         */
        void InitializeSwapChain(HWND windowHandle);

        /**
         * @brief 创建管线工厂函数
         * @param dimensionId 维度ID
         * @return 创建的渲染管线实例
         * @details 
         * 对应Iris中的createPipeline(ResourceLocation dimensionId)方法
         * 根据当前着色器包状态决定创建VanillaRenderingPipeline或EnigmaRenderingPipeline
         */
        std::unique_ptr<class IWorldRenderingPipeline> CreatePipeline(const enigma::resource::ResourceLocation& dimensionId);

    private:
        // ==================== 子系统特定对象 (非DirectX核心对象) ====================

        /// 交换链 - 双缓冲显示 (需要窗口句柄，由子系统管理)
        Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;

        // ==================== 渲染系统组件 - 基于Iris架构 ====================

        // TODO: 重新启用PipelineManager（需要修复编码问题）
        // std::unique_ptr<PipelineManager> m_pipelineManager;

        /// 当前活跃的渲染管线 - Enigma扩展 (Milestone 2.6新增)
        /// 支持EnigmaRenderingPipeline实例，用于开发和测试DirectX 12渲染管线
        std::unique_ptr<EnigmaRenderingPipeline> m_currentPipeline;

        /// Shader Pack管理器 - 着色器包系统
        std::unique_ptr<ShaderPackManager> m_shaderPackManager;

        // ==================== Immediate模式渲染组件 ====================

        /// 渲染指令队列 - immediate模式核心
        std::unique_ptr<RenderCommandQueue> m_renderCommandQueue;

        // ==================== 配置和状态 ====================

        /// 子系统配置
        Configuration m_configuration;

        /// 渲染统计信息
        mutable RenderStatistics m_renderStatistics;

        /// 初始化状态标志
        bool m_isInitialized = false;
        bool m_isStarted     = false;
        bool m_isShutdown    = false;

        /// 调试状态 (子系统级别的调试，底层调试由D3D12RenderSystem管理)
        bool m_debugRenderingEnabled = false;
    };
} // namespace enigma::graphic
