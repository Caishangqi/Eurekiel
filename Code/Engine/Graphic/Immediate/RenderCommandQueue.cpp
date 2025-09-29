#include "RenderCommandQueue.hpp"

#include "Engine/Core/Logger/LoggerAPI.hpp"

namespace enigma::graphic
{
    RenderCommandQueue::RenderCommandQueue(const QueueConfig& config)
    {
        m_config = config;

        if (m_config.enableDebugLogging)
        {
            enigma::core::LogInfo("RenderCommandQueue", "Initialized with config - MaxCommandsPerPhase: %d, EnablePhaseDetection: %b",
                                  m_config.maxCommandsPerPhase, m_config.enablePhaseDetection);
        }
    }

    RenderCommandQueue::~RenderCommandQueue()
    {
        Clear();
        enigma::core::LogInfo("RenderCommandQueue", "Destroyed");
    }

    bool RenderCommandQueue::Initialize()
    {
        // 初始化所有支持的Phase为空向量
        for (int i = static_cast<int>(Phase::NONE); i <= static_cast<int>(Phase::HAND_TRANSLUCENT); ++i)
        {
            Phase phase            = static_cast<Phase>(i);
            m_phaseCommands[phase] = CommandVector{};
        }

        if (m_config.enableDebugLogging)
        {
            enigma::core::LogInfo("RenderCommandQueue", "Initialize: Successfully initialized %d phases",
                                  m_phaseCommands.size());
        }

        return true;
    }

    unsigned long long RenderCommandQueue::GetPhaseCommandCount(WorldRenderingPhase phase)
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        auto                        it = m_phaseCommands.find(phase);
        if (it != m_phaseCommands.end())
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

        std::lock_guard<std::mutex> lock(m_queueMutex);

        // 检查阶段命令数量限制
        auto& phaseCommands = m_phaseCommands[phase];
        if (phaseCommands.size() >= m_config.maxCommandsPerPhase)
        {
            enigma::core::LogWarn("RenderCommandQueue",
                                  "SubmitCommand: Phase %s has reached max commands limit (%d)",
                                  static_cast<uint32_t>(phase), m_config.maxCommandsPerPhase);
            return;
        }

        // 添加命令到指定阶段
        phaseCommands.emplace_back(std::move(command));
        m_performanceMetrics.totalCommandsSubmitted++;

        if (m_config.enableDebugLogging)
        {
            enigma::core::LogDebug("RenderCommandQueue",
                                   "SubmitCommand: Added command '%s' to phase %s (tag: &s). Total in phase: %d",
                                   phaseCommands.back()->GetName(),
                                   static_cast<uint32_t>(phase),
                                   debugTag.empty() ? "none" : debugTag,
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

        std::lock_guard<std::mutex> lock(m_queueMutex);

        auto it = m_phaseCommands.find(phase);
        if (it == m_phaseCommands.end() || it->second.empty())
        {
            if (m_config.enableDebugLogging)
            {
                enigma::core::LogDebug("RenderCommandQueue", "ExecutePhase: No commands to execute for phase %s",
                                       static_cast<uint32_t>(phase));
            }
            return;
        }

        const auto& commands     = it->second;
        size_t      commandCount = commands.size();

        if (m_config.enableDebugLogging)
        {
            enigma::core::LogInfo("RenderCommandQueue", "ExecutePhase: Executing %d commands for phase %s",
                                  commandCount, static_cast<uint32_t>(phase));
        }


        // 执行阶段内的所有命令
        ExecutePhaseInternal(commands, commandManager);

        // 更新性能统计
        m_performanceMetrics.totalCommandsExecuted += commandCount;
        m_performanceMetrics.commandsPerPhase[phase] += commandCount;

        if (m_config.enableDebugLogging)
        {
            enigma::core::LogInfo("RenderCommandQueue", "ExecutePhase: Successfully executed %d commands for phase %s",
                                  commandCount, static_cast<uint32_t>(phase));
        }
    }

    size_t RenderCommandQueue::GetCommandCount(Phase phase) const
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        auto                        it = m_phaseCommands.find(phase);
        if (it != m_phaseCommands.end())
        {
            return it->second.size();
        }
        return 0;
    }

    size_t RenderCommandQueue::GetTotalCommandCount() const
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        size_t                      total = 0;
        for (const auto& [phase, commands] : m_phaseCommands)
        {
            total += commands.size();
        }
        return total;
    }

    void RenderCommandQueue::ClearPhase(Phase phase)
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        auto                        it = m_phaseCommands.find(phase);
        if (it != m_phaseCommands.end())
        {
            size_t clearedCount = it->second.size();
            it->second.clear();

            if (m_config.enableDebugLogging)
            {
                enigma::core::LogDebug("RenderCommandQueue", "ClearPhase: Cleared %d commands from phase %s",
                                       clearedCount, static_cast<uint32_t>(phase));
            }
        }
    }

    void RenderCommandQueue::Clear()
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        size_t                      totalCleared = 0;
        for (auto& [phase, commands] : m_phaseCommands)
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
        std::lock_guard<std::mutex> lock(m_queueMutex);
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
        std::lock_guard<std::mutex>      lock(m_queueMutex);
        std::vector<WorldRenderingPhase> activePhases;

        for (const auto& [phase, commands] : m_phaseCommands)
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
                                               command->GetName());
                    }
                }
                catch (const std::exception& e)
                {
                    enigma::core::LogError("RenderCommandQueue",
                                           "ExecutePhaseInternal: Failed to execute command '%s': %s",
                                           command->GetName(), e.what());
                }
            }
        }
    }

    void RenderCommandQueue::LogDebugInfo(const std::string& message) const
    {
        if (m_config.enableDebugLogging)
        {
            enigma::core::LogDebug("RenderCommandQueue", "%s", message);
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
