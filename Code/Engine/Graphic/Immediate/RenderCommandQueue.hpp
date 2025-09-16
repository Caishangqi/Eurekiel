#pragma once

#include "RenderCommand.hpp"
#include "Detection/PhaseDetector.hpp"
#include "../Resource/CommandListManager.hpp"
#include <map>
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>
#include <functional>
#include <chrono>
#include <d3d12.h>
#include <wrl/client.h>

namespace enigma::graphic
{
    // Forward declarations
    class CommandListManager;
    class PhaseDetector;

    /// <summary>
    /// 按阶段分类的渲染指令队列管理器
    /// 
    /// 核心功能:
    /// - 按WorldRenderingPhase分类存储绘制指令
    /// - 支持phase-based的批量执行
    /// - 线程安全的指令提交和执行
    /// - 自动Phase检测和状态管理
    /// 
    /// 设计原则:
    /// - 使用map<phase, vector<command>>存储结构
    /// - 专注于绘制指令的管理
    /// - 简化设计，提高性能
    /// - 与Iris渲染管线完全兼容
    /// 
    /// 教学要点:
    /// - Phase-based渲染的重要性
    /// - 指令分类和批处理优化
    /// - 现代渲染管线的状态管理
    /// </summary>
    class RenderCommandQueue
    {
    public:
        /// <summary>
        /// 队列配置参数
        /// </summary>
        struct QueueConfig
        {
            bool     enablePhaseDetection      = true; // 启用自动阶段检测
            bool     enableDebugLogging        = false; // 启用调试日志
            bool     enablePerformanceCounters = true; // 启用性能计数器
            size_t   maxCommandsPerPhase       = 10000; // 每个阶段最大指令数
            uint64_t frameTimeoutUs            = 16667; // 帧超时时间(60FPS)
        };

    private:
        // 核心存储：按phase分类的指令存储
        using Phase         = WorldRenderingPhase;
        using CommandVector = std::vector<RenderCommandPtr>;
        std::map<Phase, CommandVector> m_phaseCommands;

        // 当前执行状态
        Phase    m_currentPhase      = Phase::NONE;
        uint64_t m_currentFrameIndex = 0;

        // 配置和管理
        QueueConfig                    m_config;
        std::unique_ptr<PhaseDetector> m_phaseDetector;

        // 性能统计
        struct PerformanceMetrics
        {
            uint64_t                  totalCommandsSubmitted  = 0;
            uint64_t                  totalCommandsExecuted   = 0;
            uint64_t                  totalFramesProcessed    = 0;
            uint64_t                  averageCommandsPerFrame = 0;
            std::map<Phase, uint64_t> commandsPerPhase;

            void UpdateFrameStats(const std::map<Phase, CommandVector>& phaseCommands)
            {
                totalFramesProcessed++;
                uint64_t frameCommands = 0;
                for (const auto& [phase, commands] : phaseCommands)
                {
                    commandsPerPhase[phase] += commands.size();
                    frameCommands += commands.size();
                }
                totalCommandsExecuted += frameCommands;
                averageCommandsPerFrame = totalCommandsExecuted / totalFramesProcessed;
            }
        } m_performanceMetrics;

        // 线程安全
        mutable std::mutex m_queueMutex;

    public:
        /// <summary>
        /// 构造函数
        /// </summary>
        explicit RenderCommandQueue(const QueueConfig& config = QueueConfig{});

        /// <summary>
        /// 析构函数
        /// </summary>
        ~RenderCommandQueue();

        // 禁止拷贝，允许移动
        RenderCommandQueue(const RenderCommandQueue&)            = delete;
        RenderCommandQueue& operator=(const RenderCommandQueue&) = delete;
        RenderCommandQueue(RenderCommandQueue&&)                 = default;
        RenderCommandQueue& operator=(RenderCommandQueue&&)      = default;

        /// <summary>
        /// 初始化队列
        /// </summary>
        bool Initialize();

        /// <summary>
        /// 提交绘制指令到指定阶段
        /// </summary>
        /// <param name="command">绘制指令</param>
        /// <param name="phase">目标渲染阶段</param>
        /// <param name="debugTag">调试标签</param>
        void SubmitCommand(
            RenderCommandPtr   command,
            Phase              phase,
            const std::string& debugTag = ""
        );

        /// <summary>
        /// 提交绘制指令（自动检测阶段）
        /// </summary>
        /// <param name="command">绘制指令</param>
        /// <param name="debugTag">调试标签</param>
        void SubmitCommand(
            RenderCommandPtr   command,
            const std::string& debugTag = ""
        );

        /// <summary>
        /// 开始新的帧
        /// </summary>
        /// <param name="frameIndex">帧索引</param>
        void BeginFrame(uint64_t frameIndex);

        /// <summary>
        /// 按阶段顺序执行所有指令
        /// </summary>
        /// <param name="commandList">DirectX 12命令列表</param>
        /// <param name="commandManager">命令列表管理器</param>
        void ExecuteAllPhases(
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
            CommandListManager*                               commandManager
        );

        /// <summary>
        /// 执行特定阶段的指令
        /// </summary>
        /// <param name="phase">渲染阶段</param>
        /// <param name="commandList">DirectX 12命令列表</param>
        /// <param name="commandManager">命令列表管理器</param>
        void ExecutePhase(
            Phase                                             phase,
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
            CommandListManager*                               commandManager
        );

        /// <summary>
        /// 结束当前帧，清空队列
        /// </summary>
        void EndFrame();

        /// <summary>
        /// 获取指定阶段的指令数量
        /// </summary>
        /// <param name="phase">渲染阶段</param>
        /// <returns>指令数量</returns>
        size_t GetCommandCount(Phase phase) const;

        /// <summary>
        /// 获取所有阶段的总指令数量
        /// </summary>
        /// <returns>总指令数量</returns>
        size_t GetTotalCommandCount() const;

        /// <summary>
        /// 获取当前阶段
        /// </summary>
        Phase GetCurrentPhase() const { return m_currentPhase; }

        /// <summary>
        /// 手动设置当前阶段
        /// </summary>
        /// <param name="phase">目标阶段</param>
        void SetCurrentPhase(Phase phase);

        /// <summary>
        /// 清空所有指令
        /// </summary>
        void Clear();

        /// <summary>
        /// 清空指定阶段的指令
        /// </summary>
        /// <param name="phase">渲染阶段</param>
        void ClearPhase(Phase phase);

        /// <summary>
        /// 获取性能统计信息
        /// </summary>
        const PerformanceMetrics& GetPerformanceMetrics() const { return m_performanceMetrics; }

        /// <summary>
        /// 更新配置
        /// </summary>
        /// <param name="config">新配置</param>
        void UpdateConfig(const QueueConfig& config);

        /// <summary>
        /// 获取当前配置
        /// </summary>
        const QueueConfig& GetConfig() const { return m_config; }

        /// <summary>
        /// 检查队列是否为空
        /// </summary>
        bool IsEmpty() const;

        /// <summary>
        /// 获取所有有效的阶段列表
        /// </summary>
        std::vector<Phase> GetActivePhases() const;

    private:
        /// <summary>
        /// 内部执行阶段指令的实现
        /// </summary>
        void ExecutePhaseInternal(
            Phase                                             phase,
            const CommandVector&                              commands,
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
            CommandListManager*                               commandManager
        );

        /// <summary>
        /// 记录调试信息
        /// </summary>
        void LogDebugInfo(const std::string& message) const;

        /// <summary>
        /// 验证指令有效性
        /// </summary>
        bool ValidateCommand(const IRenderCommand* command) const;

        /// <summary>
        /// 获取当前时间(微秒)
        /// </summary>
        uint64_t GetCurrentTimeMicroseconds() const
        {
            auto now      = std::chrono::high_resolution_clock::now();
            auto duration = now.time_since_epoch();
            return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        }
    };
} // namespace enigma::graphic
