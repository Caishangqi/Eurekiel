#pragma once

#include "../RenderCommand.hpp"
#include "../../Core/Pipeline/WorldRenderingPhase.hpp"
#include <unordered_map>
#include <chrono>
#include <memory>
#include <vector>
#include <mutex>
#include <functional>
#include <algorithm>

namespace enigma::graphic
{
    /// <summary>
    /// Iris渲染阶段检测模式
    /// 
    /// AUTOMATIC: 基于绘制调用模式和时间自动推断
    /// MANUAL: 用户手动设置阶段
    /// HYBRID: 结合自动检测和手动设置
    /// STATISTICAL: 基于历史统计数据进行预测
    /// </summary>
    enum class PhaseDetectionMode
    {
        AUTOMATIC,
        MANUAL,
        HYBRID,
        STATISTICAL
    };

    /// <summary>
    /// 阶段转换触发器类型
    /// 定义什么条件会触发阶段转换
    /// </summary>
    enum class PhaseTriggerType
    {
        COMMAND_PATTERN, // 基于命令模式
        TIME_THRESHOLD, // 基于时间阈值
        RESOURCE_USAGE, // 基于资源使用模式
        RENDER_TARGET_CHANGE, // 基于渲染目标变化
        COMPUTE_DISPATCH, // 基于计算着色器调度
        USER_MARKER // 基于用户标记
    };

    /// <summary>
    /// 阶段检测器配置
    /// </summary>
    struct PhaseDetectorConfig
    {
        PhaseDetectionMode mode = PhaseDetectionMode::AUTOMATIC;

        // 时间相关配置
        uint64_t phaseTimeoutUs = 16000; // 16ms超时
        uint64_t minPhaseTimeUs = 100; // 最小阶段时间100微秒

        // 统计学习配置
        size_t historyFrameCount   = 60; // 保留60帧历史
        float  confidenceThreshold = 0.8f; // 80%置信度阈值

        // 模式识别配置
        bool enablePatternLearning  = true;
        bool enableResourceTracking = true;
        bool enableTimingAnalysis   = true;

        // 调试配置
        bool enableDebugLogging    = false;
        bool enablePhaseValidation = true;
    };

    /// <summary>
    /// 阶段转换规则
    /// 定义从一个阶段转换到另一个阶段的条件
    /// </summary>
    struct PhaseTransitionRule
    {
        RenderCommandContext::Phase                fromPhase;
        RenderCommandContext::Phase                toPhase;
        PhaseTriggerType                           triggerType;
        std::function<bool(const IRenderCommand*)> condition;
        float                                      confidence                 = 1.0f;
        uint64_t                                   minTimeSinceLastTransition = 0;

        PhaseTransitionRule(
            RenderCommandContext::Phase                from,
            RenderCommandContext::Phase                to,
            PhaseTriggerType                           trigger,
            std::function<bool(const IRenderCommand*)> cond,
            float                                      conf = 1.0f
        ) : fromPhase(from), toPhase(to), triggerType(trigger),
            condition(std::move(cond)), confidence(conf)
        {
        }
    };

    /// <summary>
    /// 阶段统计信息
    /// 用于学习和优化阶段检测
    /// </summary>
    struct PhaseStatistics
    {
        uint64_t totalTime    = 0; // 总时间(微秒)
        uint64_t commandCount = 0; // 指令数量
        uint64_t averageTime  = 0; // 平均时间
        uint64_t minTime      = UINT64_MAX; // 最小时间
        uint64_t maxTime      = 0; // 最大时间
        float    confidence   = 0.0f; // 检测置信度

        // 指令类型分布
        std::unordered_map<RenderCommandType, uint64_t> commandTypeDistribution;

        void UpdateStats(uint64_t phaseTime, const std::vector<RenderCommandType>& commands)
        {
            totalTime += phaseTime;
            commandCount++;
            averageTime = totalTime / commandCount;
#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif
            minTime = std::min(minTime, phaseTime);
            maxTime = std::max(maxTime, phaseTime);

            for (auto cmdType : commands)
            {
                commandTypeDistribution[cmdType]++;
            }
        }

        void Reset()
        {
            totalTime    = 0;
            commandCount = 0;
            averageTime  = 0;
            minTime      = UINT64_MAX;
            maxTime      = 0;
            confidence   = 0.0f;
            commandTypeDistribution.clear();
        }
    };

    /// <summary>
    /// Iris渲染阶段自动检测器
    /// 
    /// 核心功能:
    /// - 基于渲染指令模式自动识别Iris的10个渲染阶段
    /// - 学习用户的渲染模式并优化检测准确性
    /// - 提供手动和自动模式的混合检测
    /// - 支持实时性能分析和调优建议
    /// 
    /// 设计原则:
    /// - Observer Pattern监听渲染指令
    /// - State Machine管理阶段转换
    /// - Machine Learning进行模式识别
    /// - Statistics-based优化检测精度
    /// 
    /// 教学要点:
    /// - 理解Iris渲染管线的10个标准阶段
    /// - 学习基于模式的自动化检测算法
    /// - 掌握机器学习在图形编程中的应用
    /// - 理解实时系统的性能监控和优化
    /// 
    /// Iris 10阶段对应关系:
    /// SETUP       -> setup1-99 (compute-only)
    /// BEGIN       -> begin1-99 (composite-style)  
    /// SHADOW      -> shadow (gbuffers-style)
    /// SHADOW_COMP -> shadowcomp1-99 (composite-style)
    /// PREPARE     -> prepare1-99 (composite-style)
    /// GBUFFERS_OPAQUE -> gbuffers_* (gbuffers-style, opaque)
    /// DEFERRED    -> deferred1-99 (composite-style)
    /// GBUFFERS_TRANSLUCENT -> gbuffers_* (gbuffers-style, translucent)
    /// COMPOSITE   -> composite1-99 (composite-style)
    /// FINAL       -> final (composite-style)
    /// </summary>
    class PhaseDetector
    {
    public:
        /// <summary>
        /// 阶段变化事件回调类型
        /// </summary>
        using PhaseChangeCallback = std::function<void(
            RenderCommandContext::Phase oldPhase,
            RenderCommandContext::Phase newPhase,
            float                       confidence
        )>;

    private:
        // 配置和状态
        PhaseDetectorConfig         m_config;
        RenderCommandContext::Phase m_currentPhase;
        RenderCommandContext::Phase m_previousPhase;

        // 时间跟踪
        std::chrono::high_resolution_clock::time_point m_phaseStartTime;
        std::chrono::high_resolution_clock::time_point m_lastTransitionTime;

        // 转换规则和统计
        std::vector<PhaseTransitionRule>                                 m_transitionRules;
        std::unordered_map<RenderCommandContext::Phase, PhaseStatistics> m_phaseStats;

        // 当前帧的指令历史
        std::vector<RenderCommandType> m_currentFrameCommands;
        std::vector<uint64_t>          m_framePhaseDurations;

        // 历史学习数据
        struct FrameHistory
        {
            std::vector<RenderCommandContext::Phase>    phaseSequence;
            std::vector<uint64_t>                       phaseDurations;
            std::vector<std::vector<RenderCommandType>> phaseCommands;
            uint64_t                                    frameIndex;
        };

        std::vector<FrameHistory> m_frameHistory;
        size_t                    m_currentFrameIndex = 0;

        // 事件回调
        std::vector<PhaseChangeCallback> m_phaseChangeCallbacks;

        // 线程安全
        mutable std::mutex m_detectorMutex;

        // 性能监控
        struct PerformanceMetrics
        {
            uint64_t detectionTime   = 0;
            uint64_t falsePositives  = 0;
            uint64_t falseNegatives  = 0;
            uint64_t totalDetections = 0;
            float    accuracy        = 0.0f;
        } m_performanceMetrics;

    public:
        /// <summary>
        /// 构造函数
        /// </summary>
        explicit PhaseDetector(const PhaseDetectorConfig& config = PhaseDetectorConfig{});

        /// <summary>
        /// 析构函数
        /// </summary>
        ~PhaseDetector() = default;

        // 禁止拷贝，允许移动
        PhaseDetector(const PhaseDetector&)            = delete;
        PhaseDetector& operator=(const PhaseDetector&) = delete;
        PhaseDetector(PhaseDetector&&)                 = default;
        PhaseDetector& operator=(PhaseDetector&&)      = default;

        /// <summary>
        /// 初始化检测器
        /// 加载默认转换规则和学习数据
        /// </summary>
        bool Initialize();

        /// <summary>
        /// 处理新的渲染指令
        /// 这是检测器的主要入口点
        /// </summary>
        /// <param name="command">渲染指令</param>
        /// <returns>检测到的阶段</returns>
        RenderCommandContext::Phase ProcessCommand(const IRenderCommand* command);

        /// <summary>
        /// 手动设置当前阶段
        /// 用于MANUAL和HYBRID模式
        /// </summary>
        /// <param name="phase">目标阶段</param>
        /// <param name="confidence">置信度</param>
        void SetCurrentPhase(RenderCommandContext::Phase phase, float confidence = 1.0f);

        /// <summary>
        /// 获取当前阶段
        /// </summary>
        RenderCommandContext::Phase GetCurrentPhase() const
        {
            std::lock_guard<std::mutex> lock(m_detectorMutex);
            return m_currentPhase;
        }

        /// <summary>
        /// 开始新的帧
        /// 重置帧级别的检测状态
        /// </summary>
        void BeginFrame(uint64_t frameIndex);

        /// <summary>
        /// 结束当前帧
        /// 更新学习数据和统计信息
        /// </summary>
        void EndFrame();

        /// <summary>
        /// 注册阶段变化回调
        /// </summary>
        void RegisterPhaseChangeCallback(PhaseChangeCallback callback);

        /// <summary>
        /// 更新配置
        /// </summary>
        void UpdateConfig(const PhaseDetectorConfig& config);

        /// <summary>
        /// 获取当前配置
        /// </summary>
        const PhaseDetectorConfig& GetConfig() const { return m_config; }

        /// <summary>
        /// 获取阶段统计信息
        /// </summary>
        PhaseStatistics GetPhaseStatistics(RenderCommandContext::Phase phase) const;

        /// <summary>
        /// 获取检测器性能指标
        /// </summary>
        const PerformanceMetrics& GetPerformanceMetrics() const { return m_performanceMetrics; }

        /// <summary>
        /// 重置学习数据
        /// </summary>
        void ResetLearningData();

        /// <summary>
        /// 导出学习数据
        /// 用于保存和加载检测模型
        /// </summary>
        std::vector<uint8_t> ExportLearningData() const;

        /// <summary>
        /// 导入学习数据
        /// </summary>
        bool ImportLearningData(const std::vector<uint8_t>& data);

        /// <summary>
        /// 获取阶段预测
        /// 基于当前上下文预测下一个可能的阶段
        /// </summary>
        /// <param name="lookaheadCommands">前瞻指令列表</param>
        /// <returns>预测阶段和置信度的配对</returns>
        std::vector<std::pair<RenderCommandContext::Phase, float>> PredictNextPhases(
            const std::vector<const IRenderCommand*>& lookaheadCommands
        ) const;

    private:
        /// <summary>
        /// 初始化默认转换规则
        /// 基于Iris标准渲染管线定义规则
        /// </summary>
        void InitializeDefaultRules();

        /// <summary>
        /// 分析指令模式进行阶段检测
        /// </summary>
        RenderCommandContext::Phase AnalyzeCommandPattern(const IRenderCommand* command);

        /// <summary>
        /// 检查阶段转换条件
        /// </summary>
        bool CheckTransitionCondition(
            const PhaseTransitionRule& rule,
            const IRenderCommand*      command
        ) const;

        /// <summary>
        /// 更新阶段统计信息
        /// </summary>
        void UpdatePhaseStatistics(
            RenderCommandContext::Phase           phase,
            uint64_t                              duration,
            const std::vector<RenderCommandType>& commands
        );

        /// <summary>
        /// 执行阶段转换
        /// </summary>
        void TransitionToPhase(
            RenderCommandContext::Phase newPhase,
            float                       confidence
        );

        /// <summary>
        /// 基于历史数据进行预测
        /// </summary>
        RenderCommandContext::Phase PredictPhaseFromHistory(
            const IRenderCommand* command
        ) const;

        /// <summary>
        /// 计算阶段转换置信度
        /// </summary>
        float CalculateTransitionConfidence(
            RenderCommandContext::Phase fromPhase,
            RenderCommandContext::Phase toPhase,
            const IRenderCommand*       trigger
        ) const;

        /// <summary>
        /// 学习新的转换模式
        /// </summary>
        void LearnTransitionPattern(
            RenderCommandContext::Phase fromPhase,
            RenderCommandContext::Phase toPhase,
            const IRenderCommand*       trigger
        );

        /// <summary>
        /// 验证阶段检测准确性
        /// </summary>
        void ValidateDetection(
            RenderCommandContext::Phase detectedPhase,
            RenderCommandContext::Phase expectedPhase
        );

        /// <summary>
        /// 获取当前时间(微秒)
        /// </summary>
        uint64_t GetCurrentTimeMicroseconds() const
        {
            auto now      = std::chrono::high_resolution_clock::now();
            auto duration = now.time_since_epoch();
            return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        }

        /// <summary>
        /// 日志调试信息
        /// </summary>
        void LogDebugInfo(const std::string& message) const;

        /// <summary>
        /// 阶段名称转换
        /// </summary>
        static std::string PhaseToString(RenderCommandContext::Phase phase);
    };

    /// <summary>
    /// 阶段检测器工厂
    /// 用于创建和配置不同类型的检测器
    /// </summary>
    class PhaseDetectorFactory
    {
    public:
        /// <summary>
        /// 创建自动检测器
        /// 适用于大多数标准Iris渲染管线
        /// </summary>
        static std::unique_ptr<PhaseDetector> CreateAutomaticDetector();

        /// <summary>
        /// 创建学习型检测器
        /// 具有机器学习能力，可以适应自定义渲染模式
        /// </summary>
        static std::unique_ptr<PhaseDetector> CreateLearningDetector();

        /// <summary>
        /// 创建高性能检测器
        /// 优化检测速度，适用于实时应用
        /// </summary>
        static std::unique_ptr<PhaseDetector> CreateHighPerformanceDetector();

        /// <summary>
        /// 创建调试检测器
        /// 提供详细的调试信息和验证
        /// </summary>
        static std::unique_ptr<PhaseDetector> CreateDebugDetector();
    };
} // namespace enigma::graphic
