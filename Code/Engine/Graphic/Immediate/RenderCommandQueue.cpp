#include "RenderCommandQueue.hpp"

#include "Engine/Core/Logger/LoggerAPI.hpp"

namespace enigma::graphic
{
    RenderCommandQueue::RenderCommandQueue(const QueueConfig& config)
    {
        m_config = config;

        if (m_config.enableDebugLogging)
        {
            enigma::core::LogInfo("RenderCommandQueue", "Initialized with config - MaxCommandsPerPhase: %zu, EnablePhaseDetection: %s",
                                  m_config.maxCommandsPerPhase, m_config.enablePhaseDetection ? "true" : "false");
        }
    }

    RenderCommandQueue::~RenderCommandQueue()
    {
        Clear();
        enigma::core::LogInfo("RenderCommandQueue", "Destroyed");
    }

    bool RenderCommandQueue::Initialize()
    {
        // 预分配submitQueue和executeQueue，避免运行时分配
        // 为所有24个Iris Phase预留空间
        for (int i = static_cast<int>(Phase::NONE); i <= static_cast<int>(Phase::HAND_TRANSLUCENT); ++i)
        {
            Phase phase           = static_cast<Phase>(i);
            m_submitQueue[phase]  = CommandVector{};
            m_executeQueue[phase] = CommandVector{};
        }

        if (m_config.enableDebugLogging)
        {
            enigma::core::LogInfo("RenderCommandQueue",
                                  "Initialize: Double-buffered queue initialized for %d phases",
                                  m_submitQueue.size());
        }

        return true;
    }

    void RenderCommandQueue::SwapBuffers()
    {
        // 双缓冲队列交换 - 无锁设计的核心
        // 教学要点：
        // 1. 原子标志避免重复交换
        // 2. std::swap 是O(1)操作，只交换指针
        // 3. 交换后 Game线程写入新的m_submitQueue, 渲染线程读取m_executeQueue

        bool expected = false;
        if (!m_isSwapping.compare_exchange_strong(expected, true))
        {
            // 正在交换中，跳过本次操作
            if (m_config.enableDebugLogging)
            {
                enigma::core::LogWarn("RenderCommandQueue", "SwapBuffers: Already swapping, skipping");
            }
            return;
        }

        // 交换两个队列
        std::swap(m_submitQueue, m_executeQueue);

        // 清空新的提交队列 (原executeQueue)
        // 为下一帧的指令提交做准备
        for (auto& [phase, commands] : m_submitQueue)
        {
            commands.clear();
        }

        // 重置交换标志
        m_isSwapping.store(false);

        if (m_config.enableDebugLogging)
        {
            size_t totalCommandsSwapped = 0;
            for (const auto& [phase, commands] : m_executeQueue)
            {
                totalCommandsSwapped += commands.size();
            }
            enigma::core::LogDebug("RenderCommandQueue",
                                   "SwapBuffers: Swapped buffers, %d commands ready for execution",
                                   totalCommandsSwapped);
        }
    }

    void RenderCommandQueue::BeginFrame(uint64_t frameIndex)
    {
        m_currentFrameIndex = frameIndex;

        // 双缓冲架构：每帧开始时交换缓冲
        // 上一帧Game线程提交的指令 → 进入executeQueue供渲染线程执行
        // 清空submitQueue → 供本帧Game线程提交新指令
        SwapBuffers();

        if (m_config.enableDebugLogging)
        {
            enigma::core::LogInfo("RenderCommandQueue", "BeginFrame: Frame %llu started", frameIndex);
        }
    }

    unsigned long long RenderCommandQueue::GetPhaseCommandCount(WorldRenderingPhase phase)
    {
        // 双缓冲架构：读取executeQueue（当前正在执行的队列）
        auto it = m_executeQueue.find(phase);
        if (it != m_executeQueue.end())
        {
            return it->second.size();
        }
        return 0;
    }

    void RenderCommandQueue::SubmitCommand(
        RenderCommandPtr   command,
        Phase              phase,
        const std::string& debugTag)
    {
        if (!command)
        {
            if (m_config.enableDebugLogging)
            {
                enigma::core::LogError("RenderCommandQueue", "SubmitCommand: Null command provided");
            }
            return;
        }

        // 验证命令有效性
        if (!ValidateCommand(command.get()))
        {
            if (m_config.enableDebugLogging)
            {
                enigma::core::LogError("RenderCommandQueue", "SubmitCommand: Command validation failed");
            }
            return;
        }

        // 双缓冲架构：提交到submitQueue（Game线程写入）
        // 无需mutex，因为Game线程独占submitQueue写入权限
        auto& phaseCommands = m_submitQueue[phase];

        // 检查阶段命令数量限制
        if (phaseCommands.size() >= m_config.maxCommandsPerPhase)
        {
            enigma::core::LogWarn("RenderCommandQueue",
                                  "SubmitCommand: Phase %d has reached max commands limit (%d)",
                                  static_cast<uint32_t>(phase), m_config.maxCommandsPerPhase);
            return;
        }

        // 添加命令到指定阶段
        phaseCommands.emplace_back(std::move(command));
        m_performanceMetrics.totalCommandsSubmitted++;

        if (m_config.enableDebugLogging)
        {
            enigma::core::LogDebug("RenderCommandQueue",
                                   "SubmitCommand: Added command '%s' to phase %d (tag: %s). Total in phase: %d",
                                   phaseCommands.back()->GetName().c_str(),
                                   static_cast<uint32_t>(phase),
                                   debugTag.empty() ? "none" : debugTag.c_str(),
                                   phaseCommands.size());
        }
    }

    void RenderCommandQueue::SubmitCommand(
        RenderCommandPtr   command,
        const std::string& debugTag)
    {
        // 使用当前阶段作为默认阶段
        SubmitCommand(std::move(command), m_currentPhase, debugTag);
    }

    void RenderCommandQueue::ExecutePhase(Phase phase, std::shared_ptr<CommandListManager> commandManager)
    {
        if (!commandManager)
        {
            enigma::core::LogError("RenderCommandQueue", "ExecutePhase: CommandListManager is null");
            return;
        }

        // 双缓冲架构：从executeQueue读取（渲染线程读取）
        // 无需mutex，因为渲染线程独占executeQueue读取权限
        auto it = m_executeQueue.find(phase);
        if (it == m_executeQueue.end() || it->second.empty())
        {
            if (m_config.enableDebugLogging)
            {
                enigma::core::LogDebug("RenderCommandQueue", "ExecutePhase: No commands to execute for phase %d",
                                       static_cast<uint32_t>(phase));
            }
            return;
        }

        const auto& commands     = it->second;
        size_t      commandCount = commands.size();

        if (m_config.enableDebugLogging)
        {
            enigma::core::LogInfo("RenderCommandQueue", "ExecutePhase: Executing %d commands for phase %d",
                                  commandCount, static_cast<uint32_t>(phase));
        }

        // 执行阶段内的所有命令
        ExecutePhaseInternal(commands, commandManager);

        // 更新性能统计
        m_performanceMetrics.totalCommandsExecuted += commandCount;
        m_performanceMetrics.commandsPerPhase[phase] += commandCount;

        if (m_config.enableDebugLogging)
        {
            enigma::core::LogInfo("RenderCommandQueue", "ExecutePhase: Successfully executed %d commands for phase %d",
                                  commandCount, static_cast<uint32_t>(phase));
        }
    }

    size_t RenderCommandQueue::GetCommandCount(Phase phase) const
    {
        // 双缓冲架构：读取executeQueue的命令数量
        auto it = m_executeQueue.find(phase);
        if (it != m_executeQueue.end())
        {
            return it->second.size();
        }
        return 0;
    }

    size_t RenderCommandQueue::GetTotalCommandCount() const
    {
        // 双缓冲架构：统计executeQueue的总命令数
        size_t total = 0;
        for (const auto& [phase, commands] : m_executeQueue)
        {
            total += commands.size();
        }
        return total;
    }

    void RenderCommandQueue::ClearPhase(Phase phase)
    {
        // 双缓冲架构：清空submitQueue和executeQueue中指定阶段
        size_t clearedCount = 0;

        auto itSubmit = m_submitQueue.find(phase);
        if (itSubmit != m_submitQueue.end())
        {
            clearedCount += itSubmit->second.size();
            itSubmit->second.clear();
        }

        auto itExecute = m_executeQueue.find(phase);
        if (itExecute != m_executeQueue.end())
        {
            clearedCount += itExecute->second.size();
            itExecute->second.clear();
        }

        if (m_config.enableDebugLogging && clearedCount > 0)
        {
            enigma::core::LogDebug("RenderCommandQueue", "ClearPhase: Cleared %d commands from phase %d",
                                   clearedCount, static_cast<uint32_t>(phase));
        }
    }

    void RenderCommandQueue::Clear()
    {
        // 双缓冲架构：清空两个队列
        size_t totalCleared = 0;

        for (auto& [phase, commands] : m_submitQueue)
        {
            totalCleared += commands.size();
            commands.clear();
        }

        for (auto& [phase, commands] : m_executeQueue)
        {
            totalCleared += commands.size();
            commands.clear();
        }

        if (m_config.enableDebugLogging)
        {
            enigma::core::LogDebug("RenderCommandQueue", "Clear: Cleared %d total commands from all phases", totalCleared);
        }
    }

    void RenderCommandQueue::SetCurrentPhase(Phase phase)
    {
        // 简单的标志位设置，零开销 (方案A)
        m_currentPhase = phase;

        if (m_config.enableDebugLogging)
        {
            enigma::core::LogDebug("RenderCommandQueue", "SetCurrentPhase: Phase changed to %d",
                                   static_cast<uint32_t>(phase));
        }
    }

    bool RenderCommandQueue::IsEmpty() const
    {
        return GetTotalCommandCount() == 0;
    }

    std::vector<WorldRenderingPhase> RenderCommandQueue::GetActivePhases() const
    {
        // 双缓冲架构：返回executeQueue中有命令的Phase列表
        std::vector<WorldRenderingPhase> activePhases;

        for (const auto& [phase, commands] : m_executeQueue)
        {
            if (!commands.empty())
            {
                activePhases.push_back(phase);
            }
        }

        return activePhases;
    }

    void RenderCommandQueue::ExecutePhaseInternal(
        const CommandVector&                commands,
        std::shared_ptr<CommandListManager> commandManager)
    {
        if (commands.empty() || !commandManager)
        {
            return;
        }

        for (const auto& command : commands)
        {
            if (command && ValidateCommand(command.get()))
            {
                try
                {
                    // 执行单个渲染命令
                    command->Execute(commandManager);

                    if (m_config.enableDebugLogging)
                    {
                        enigma::core::LogDebug("RenderCommandQueue",
                                               "ExecutePhaseInternal: Executed command '%s'",
                                               command->GetName().c_str());
                    }
                }
                catch (const std::exception& e)
                {
                    enigma::core::LogError("RenderCommandQueue",
                                           "ExecutePhaseInternal: Failed to execute command '%s': %s",
                                           command->GetName().c_str(), e.what());
                }
            }
        }
    }

    void RenderCommandQueue::LogDebugInfo(const std::string& message) const
    {
        if (m_config.enableDebugLogging)
        {
            enigma::core::LogDebug("RenderCommandQueue", "%s", message.c_str());
        }
    }

    bool RenderCommandQueue::ValidateCommand(const IRenderCommand* command) const
    {
        if (!command)
        {
            return false;
        }

        // 基础验证：检查命令名称是否有效
        std::string commandName = command->GetName();
        if (commandName.empty())
        {
            if (m_config.enableDebugLogging)
            {
                core::LogWarn("RenderCommandQueue", "ValidateCommand: Command has empty name");
            }
            return false;
        }

        // 可以在这里添加更多验证逻辑
        return true;
    }
} // namespace enigma::graphic
