#include "ScheduleSubsystem.hpp"
#include "ScheduleException.hpp"
#include "TaskWorkerThread.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Yaml.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include <algorithm>

using namespace enigma::core;
DEFINE_LOG_CATEGORY(LogSchedule)
ScheduleSubsystem* g_theSchedule = nullptr;

namespace
{
    int countPendingTasksForType(
        const std::map<std::string, std::map<TaskPriority, std::deque<RunnableTask*>>>& pendingTasksByType,
        const std::string& typeStr)
    {
        const auto typeIt = pendingTasksByType.find(typeStr);
        if (typeIt == pendingTasksByType.end())
        {
            return 0;
        }

        int totalPending = 0;
        for (const auto& priorityPair : typeIt->second)
        {
            totalPending += static_cast<int>(priorityPair.second.size());
        }

        return totalPending;
    }

    int countPendingTasksTotal(
        const std::map<std::string, std::map<TaskPriority, std::deque<RunnableTask*>>>& pendingTasksByType)
    {
        int totalPending = 0;
        for (const auto& typePair : pendingTasksByType)
        {
            for (const auto& priorityPair : typePair.second)
            {
                totalPending += static_cast<int>(priorityPair.second.size());
            }
        }

        return totalPending;
    }

    bool removePendingTaskByPointer(
        std::map<std::string, std::map<TaskPriority, std::deque<RunnableTask*>>>& pendingTasksByType,
        RunnableTask* task)
    {
        for (auto typeIt = pendingTasksByType.begin(); typeIt != pendingTasksByType.end(); ++typeIt)
        {
            auto& priorityMap = typeIt->second;
            for (auto priorityIt = priorityMap.begin(); priorityIt != priorityMap.end(); ++priorityIt)
            {
                auto& taskQueue = priorityIt->second;
                const auto taskIt = std::find(taskQueue.begin(), taskQueue.end(), task);
                if (taskIt == taskQueue.end())
                {
                    continue;
                }

                taskQueue.erase(taskIt);

                if (taskQueue.empty())
                {
                    priorityIt = priorityMap.erase(priorityIt);
                    if (priorityIt == priorityMap.end())
                    {
                        break;
                    }
                }

                if (priorityMap.empty())
                {
                    pendingTasksByType.erase(typeIt);
                }

                return true;
            }
        }

        return false;
    }
}

//-----------------------------------------------------------------------------------------------
// LoadFromYaml: Parse YAML configuration WITHOUT logging (called before Logger is ready)
// Uses Bukkit/SpigotAPI-style YAML access (now fixed to work with array elements)
//-----------------------------------------------------------------------------------------------
void ScheduleConfig::LoadFromYaml(const YamlConfiguration& yaml)
{
    task_types.clear();

    // Get list of task type configurations using SpigotAPI-style method
    auto taskTypeList = yaml.GetConfigurationList("task_types");

    for (const auto& taskTypeYaml : taskTypeList)
    {
        TaskTypeDefinition def;

        // Bukkit/SpigotAPI-style field access (now works thanks to GetNodeByPath fix)
        def.type        = taskTypeYaml.GetString("type", "");
        def.threads     = taskTypeYaml.GetInt("threads", 1);
        def.description = taskTypeYaml.GetString("description", "");

        // Validation
        if (def.type.empty())
            continue;

        if (def.threads <= 0)
            def.threads = 1;

        task_types.push_back(def);
    }
}

//-----------------------------------------------------------------------------------------------
// LoadFromFile: Load YAML configuration WITHOUT logging (called before Logger is ready)
//-----------------------------------------------------------------------------------------------
bool ScheduleConfig::LoadFromFile(const std::string& filePath)
{
    auto yamlOpt = YamlConfiguration::TryLoadFromFile(filePath);

    if (!yamlOpt.has_value())
        return false;

    LoadFromYaml(yamlOpt.value());
    return true;
}

//-----------------------------------------------------------------------------------------------
// GetDefaultConfig: Return default configuration WITHOUT logging (called before Logger is ready)
//-----------------------------------------------------------------------------------------------
ScheduleConfig ScheduleConfig::GetDefaultConfig()
{
    ScheduleConfig config;

    config.task_types.push_back(TaskTypeDefinition("Generic", 4, "General-purpose CPU-bound tasks"));
    config.task_types.push_back(TaskTypeDefinition("FileIO", 2, "File I/O operations"));
    config.task_types.push_back(TaskTypeDefinition("ChunkGen", 2, "Procedural chunk generation"));
    config.task_types.push_back(TaskTypeDefinition("Rendering", 1, "Render preparation tasks"));

    return config;
}

//-----------------------------------------------------------------------------------------------
// ScheduleSubsystem: Constructor (no logging - Logger may not be ready)
//-----------------------------------------------------------------------------------------------
ScheduleSubsystem::ScheduleSubsystem(ScheduleConfig& config)
    : m_config(config)
{
    // PERFORMANCE: Global log level is set to ERROR in module.yml to reduce console I/O overhead
    // Debug logging causes significant SubmitTask overhead due to synchronous console writes
    // All categories (including ScheduleSubsystem) inherit ERROR level from configuration
}

ScheduleSubsystem::~ScheduleSubsystem()
{
    if (!m_workerThreads.empty())
    {
        LogWarn(LogSchedule,
                "ScheduleSubsystem destroyed without Shutdown()!");
    }
}

//-----------------------------------------------------------------------------------------------
// Startup: Initialize subsystem (NOW Logger is ready, safe to log)
//-----------------------------------------------------------------------------------------------
void ScheduleSubsystem::Startup()
{
    LogInfo(LogSchedule,
            "Startup() - Phase 2 (YAML-driven), loaded %d task types",
            (int)m_config.task_types.size());

    for (const auto& typeDef : m_config.task_types)
    {
        m_typeRegistry.RegisterType(typeDef.type, typeDef.threads);
        LogInfo(LogSchedule,
                "Registered type '%s' with %d threads",
                typeDef.type.c_str(), typeDef.threads);
    }

    LogInfo(LogSchedule,
            "Registered %d task types, total %d threads",
            (int)m_typeRegistry.GetAllTypes().size(),
            m_typeRegistry.GetTotalThreadCount());

    CreateWorkerThreads();
    g_theSchedule = this;
    LogInfo(LogSchedule, "Startup complete");
}

//-----------------------------------------------------------------------------------------------
// Shutdown: Stop worker threads and cleanup
//-----------------------------------------------------------------------------------------------
void ScheduleSubsystem::Shutdown()
{
    LogInfo(LogSchedule, "Shutdown() called");

    m_isShuttingDown.store(true);
    // Notify all types to wake up for shutdown
    for (auto& cvPair : m_typeConditionVariables)
    {
        cvPair.second.notify_all();
    }

    DestroyWorkerThreads();

    // Clean up layered pending tasks map
    for (auto& typePair : m_pendingTasksByType)
    {
        for (auto& priorityPair : typePair.second)
        {
            for (RunnableTask* task : priorityPair.second)
            {
                delete task;
            }
        }
    }

    for (RunnableTask* task : m_executingTasks) { delete task; }
    for (const TaskCompletionRecord& completionRecord : m_completedTaskRecords)
    {
        if (completionRecord.task != nullptr)
        {
            delete completionRecord.task;
        }
    }

    m_pendingTasksByType.clear();
    m_executingTasks.clear();
    m_completedTaskRecords.clear();
    m_taskControlBlocks.clear();
    m_taskHandleByPointer.clear();
    m_keyedTaskPolicyTracker.Clear();

    LogInfo(LogSchedule, "Shutdown complete");
}

TaskHandle ScheduleSubsystem::SubmitTask(RunnableTask* task, const TaskSubmissionOptions& options)
{
    if (!task)
    {
        LogError(LogSchedule,
                 "SubmitTask: Attempted to submit null task");
        return {};
    }

    std::string taskType = task->GetType();
    TaskHandle  resolvedHandle;
    bool        shouldNotify = false;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);

        TaskHandle submissionHandle = allocateTaskHandle();
        KeyedTaskSubmissionDecision decision;
        decision.resolvedHandle = submissionHandle;

        if (options.taskKey.has_value())
        {
            decision = m_keyedTaskPolicyTracker.RegisterSubmission(submissionHandle, options);
        }

        if (!decision.shouldQueueTask)
        {
            delete task;
            return decision.resolvedHandle;
        }

        resolvedHandle = decision.resolvedHandle;
        task->AttachHandle(resolvedHandle);
        task->SetState(TaskState::Queued);

        TaskControlBlock controlBlock;
        controlBlock.handle                 = resolvedHandle;
        controlBlock.task                   = task;
        controlBlock.state                  = TaskState::Queued;
        controlBlock.cancellationRequested  = false;
        controlBlock.supportsCancellation   = options.supportsCancellation;
        controlBlock.wasCancelled           = false;
        controlBlock.isStale                = false;
        controlBlock.taskKey                = options.taskKey;
        controlBlock.version                = options.version;
        controlBlock.keyedPolicy            = options.keyedPolicy;
        controlBlock.policyDecision         = decision.policyDecision;

        if (decision.supersededHandle.has_value())
        {
            TaskControlBlock* supersededControlBlock = findTaskControlBlock(*decision.supersededHandle);
            if (supersededControlBlock != nullptr && !supersededControlBlock->IsTerminal())
            {
                supersededControlBlock->isStale = true;
                supersededControlBlock->policyDecision =
                    supersededControlBlock->state == TaskState::Executing
                    ? TaskPolicyDecision::SupersededAfterExecution
                    : TaskPolicyDecision::SupersededBeforeExecution;
            }
        }

        m_taskControlBlocks[resolvedHandle] = controlBlock;
        m_taskHandleByPointer[task]         = resolvedHandle;

        m_pendingTasksByType[taskType][options.priority].push_back(task);
        shouldNotify = true;

        LogInfo(LogSchedule,
                "SubmitTask: Added task type='%s' priority=%d handle=(%llu,%u) pending=%d",
                taskType.c_str(),
                static_cast<int>(options.priority),
                resolvedHandle.id,
                resolvedHandle.generation,
                countPendingTasksTotal(m_pendingTasksByType));
    }

    if (shouldNotify)
    {
        m_typeConditionVariables[taskType].notify_one();
    }

    return resolvedHandle;
}

bool ScheduleSubsystem::RequestTaskCancellation(const TaskHandle& handle)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);

    TaskControlBlock* controlBlock = findTaskControlBlock(handle);
    if (controlBlock == nullptr)
    {
        throw InvalidTaskHandleException("ScheduleSubsystem::RequestTaskCancellation: Invalid task handle");
    }

    if (!controlBlock->supportsCancellation || controlBlock->task == nullptr)
    {
        return false;
    }

    if (controlBlock->IsTerminal())
    {
        return false;
    }

    const bool firstRequest = controlBlock->task->RequestCancellation();
    controlBlock->cancellationRequested = controlBlock->task->IsCancellationRequested();

    if (controlBlock->state == TaskState::Queued && removePendingTaskByPointer(m_pendingTasksByType, controlBlock->task))
    {
        controlBlock->state        = TaskState::Cancelled;
        controlBlock->wasCancelled = true;
        controlBlock->task->SetState(TaskState::Cancelled);

        if (m_keyedTaskPolicyTracker.IsTrackingTask(handle))
        {
            m_keyedTaskPolicyTracker.MarkTaskTerminal(handle);
        }

        m_completedTaskRecords.push_back(controlBlock->ToCompletionRecord());
        return firstRequest;
    }

    controlBlock->state = TaskState::CancelRequested;
    controlBlock->task->SetState(TaskState::CancelRequested);
    return firstRequest;
}

TaskState ScheduleSubsystem::GetTaskState(const TaskHandle& handle) const
{
    std::lock_guard<std::mutex> lock(m_queueMutex);

    const TaskControlBlock* controlBlock = findTaskControlBlock(handle);
    if (controlBlock == nullptr)
    {
        throw InvalidTaskHandleException("ScheduleSubsystem::GetTaskState: Invalid task handle");
    }

    return controlBlock->state;
}

TaskResultDrainView ScheduleSubsystem::DrainCompletedTaskRecords()
{
    std::lock_guard<std::mutex> lock(m_queueMutex);

    TaskResultDrainView drainView;
    drainView.records = std::move(m_completedTaskRecords);

    for (const TaskCompletionRecord& record : drainView.records)
    {
        if (record.task != nullptr)
        {
            m_taskHandleByPointer.erase(record.task);
        }

        m_taskControlBlocks.erase(record.handle);
    }

    m_completedTaskRecords.clear();
    return drainView;
}

RunnableTask* ScheduleSubsystem::GetNextTaskForType(const std::string& typeStr)
{
    // IMPORTANT: Caller must already hold m_queueMutex lock!
    // This is called from TaskWorkerThread::ThreadMain() which already holds unique_lock

    // O(1) priority-based retrieval using layered map
    // Strategy: Check High priority queue first, then Normal priority queue

    auto typeIt = m_pendingTasksByType.find(typeStr);
    if (typeIt == m_pendingTasksByType.end())
        return nullptr; // No tasks of this type at all

    auto& priorityMap = typeIt->second; // map<TaskPriority, deque<Task>>

    // Priority 1: Check High priority queue first
    auto highIt = priorityMap.find(TaskPriority::High);
    if (highIt != priorityMap.end() && !highIt->second.empty())
    {
        RunnableTask* task = highIt->second.front();
        highIt->second.pop_front();

        // Clean up empty deque
        if (highIt->second.empty())
        {
            priorityMap.erase(highIt);
            // Clean up empty priority map
            if (priorityMap.empty())
                m_pendingTasksByType.erase(typeIt);
        }

        m_executingTasks.push_back(task);
        if (TaskControlBlock* controlBlock = findTaskControlBlock(task))
        {
            controlBlock->state = TaskState::Executing;
            task->SetState(TaskState::Executing);

            if (m_keyedTaskPolicyTracker.IsTrackingTask(controlBlock->handle))
            {
                m_keyedTaskPolicyTracker.MarkTaskExecuting(controlBlock->handle);
            }
        }
        return task;
    }

    // Priority 2: Check Normal priority queue
    auto normalIt = priorityMap.find(TaskPriority::Normal);
    if (normalIt != priorityMap.end() && !normalIt->second.empty())
    {
        RunnableTask* task = normalIt->second.front();
        normalIt->second.pop_front();

        // Clean up empty deque
        if (normalIt->second.empty())
        {
            priorityMap.erase(normalIt);
            // Clean up empty priority map
            if (priorityMap.empty())
                m_pendingTasksByType.erase(typeIt);
        }

        m_executingTasks.push_back(task);
        if (TaskControlBlock* controlBlock = findTaskControlBlock(task))
        {
            controlBlock->state = TaskState::Executing;
            task->SetState(TaskState::Executing);

            if (m_keyedTaskPolicyTracker.IsTrackingTask(controlBlock->handle))
            {
                m_keyedTaskPolicyTracker.MarkTaskExecuting(controlBlock->handle);
            }
        }
        return task;
    }

    return nullptr; // No tasks available for this type
}

void ScheduleSubsystem::OnTaskCompleted(RunnableTask* task)
{
    std::string taskType     = task->GetType();
    bool        shouldNotify = false;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);

        auto it = std::find(m_executingTasks.begin(), m_executingTasks.end(), task);
        if (it != m_executingTasks.end())
        {
            m_executingTasks.erase(it);
        }

        TaskControlBlock* controlBlock = findTaskControlBlock(task);
        if (controlBlock != nullptr)
        {
            TaskState finalState = task->GetState();
            if (finalState != TaskState::Cancelled && finalState != TaskState::Failed)
            {
                finalState = controlBlock->cancellationRequested || task->IsCancellationRequested()
                    ? TaskState::Cancelled
                    : TaskState::Completed;
            }

            controlBlock->state        = finalState;
            controlBlock->wasCancelled = finalState == TaskState::Cancelled;
            task->SetState(finalState);

            if (m_keyedTaskPolicyTracker.IsTrackingTask(controlBlock->handle))
            {
                const TaskFreshnessEvaluation freshness = m_keyedTaskPolicyTracker.EvaluateCompletion(controlBlock->handle);
                controlBlock->isStale = freshness.isStale;
                if (freshness.policyDecision != TaskPolicyDecision::Executed || freshness.isStale)
                {
                    controlBlock->policyDecision = freshness.policyDecision;
                }

                m_keyedTaskPolicyTracker.MarkTaskTerminal(controlBlock->handle);
            }

            m_completedTaskRecords.push_back(controlBlock->ToCompletionRecord());
        }
        else
        {
            TaskCompletionRecord completionRecord;
            completionRecord.handle         = task->GetHandle();
            completionRecord.task           = task;
            completionRecord.finalState     = task->IsCancellationRequested() ? TaskState::Cancelled : TaskState::Completed;
            completionRecord.wasCancelled   = completionRecord.finalState == TaskState::Cancelled;
            completionRecord.policyDecision = TaskPolicyDecision::Executed;
            task->SetState(completionRecord.finalState);
            m_completedTaskRecords.push_back(completionRecord);
        }

        // Plan 1: Check if there are more tasks of the SAME type pending
        shouldNotify = HasPendingTaskOfType(taskType);
    }
    // Release lock before notifying (best practice)

    if (shouldNotify)
    {
        // Notify only workers of the matching type (Plan 1: Per-Type Condition Variables)
        m_typeConditionVariables[taskType].notify_one();
    }
}

void ScheduleSubsystem::CreateWorkerThreads()
{
    LogInfo(LogSchedule, "Creating worker threads...");

    int  globalWorkerID = 0;
    auto allTypes       = m_typeRegistry.GetAllTypes();

    for (const std::string& typeStr : allTypes)
    {
        int threadCount = m_typeRegistry.GetThreadCount(typeStr);

        for (int i = 0; i < threadCount; ++i)
        {
            TaskWorkerThread* worker = new TaskWorkerThread(globalWorkerID, typeStr, this);
            m_workerThreads.push_back(worker);
            globalWorkerID++;
        }
    }

    LogInfo(LogSchedule,
            "Created %d worker threads", globalWorkerID);
}

void ScheduleSubsystem::DestroyWorkerThreads()
{
    LogInfo(LogSchedule,
            "Destroying %d worker threads...", (int)m_workerThreads.size());

    for (TaskWorkerThread* worker : m_workerThreads)
    {
        delete worker;
    }

    m_workerThreads.clear();
    LogInfo(LogSchedule, "All worker threads destroyed");
}

bool ScheduleSubsystem::HasPendingTaskOfType(const std::string& typeStr) const
{
    // O(log k) lookup in layered map
    auto typeIt = m_pendingTasksByType.find(typeStr);
    if (typeIt == m_pendingTasksByType.end())
    {
        LogDebug(LogSchedule,
                 "HasPendingTaskOfType('%s'): No tasks found (type not in map)",
                 typeStr.c_str());
        return false;
    }

    const auto& priorityMap = typeIt->second;

    // Check if any priority queue has tasks
    for (const auto& priorityPair : priorityMap)
    {
        if (!priorityPair.second.empty())
        {
            LogDebug(LogSchedule,
                     "HasPendingTaskOfType('%s'): Found %d tasks in priority queue",
                     typeStr.c_str(), (int)priorityPair.second.size());
            return true;
        }
    }

    LogDebug(LogSchedule,
             "HasPendingTaskOfType('%s'): No tasks found (all queues empty)",
             typeStr.c_str());
    return false;
}

int32_t ScheduleSubsystem::GetPendingTaskCount(const std::string& typeStr) const
{
    std::scoped_lock lock(m_queueMutex);
    return countPendingTasksForType(m_pendingTasksByType, typeStr);
}

int32_t ScheduleSubsystem::GetExecutingTaskCount(const std::string& typeStr) const
{
    std::scoped_lock lock(m_queueMutex);

    int result = 0;
    for (const RunnableTask* executingTask : m_executingTasks)
    {
        if (executingTask != nullptr && executingTask->GetType() == typeStr)
        {
            result++;
        }
    }

    return result;
}

int32_t ScheduleSubsystem::GetCompletedTaskCount(const std::string& typeStr) const
{
    std::scoped_lock lock(m_queueMutex);

    int result = 0;
    for (const TaskCompletionRecord& completionRecord : m_completedTaskRecords)
    {
        if (completionRecord.task != nullptr && completionRecord.task->GetType() == typeStr)
        {
            result++;
        }
    }

    return result;
}

//-----------------------------------------------------------------------------------------------
// GetConditionVariableForType: Return type-specific condition variable (Plan 1)
// Auto-creates CV on first access for each type
//-----------------------------------------------------------------------------------------------
std::condition_variable& ScheduleSubsystem::GetConditionVariableForType(const std::string& typeStr)
{
    return m_typeConditionVariables[typeStr];
}

//------------------------------------------------------------------------------------------------------------------
// Shutdown support: check Executing queue
//
// Purpose: Wait for all executing tasks to be completed during shutdown
// Thread safety: use m_queueMutex protection
// Assignment 03 requirement: must wait for all threads to complete, not just the Pending queue
//------------------------------------------------------------------------------------------------------------------
bool ScheduleSubsystem::HasExecutingTasks(const std::string& taskType) const
{
    std::scoped_lock lock(m_queueMutex);

    for (const auto& task : m_executingTasks)
    {
        if (task && task->GetType() == taskType)
        {
            return true;
        }
    }

    return false;
}

TaskHandle ScheduleSubsystem::allocateTaskHandle()
{
    TaskHandle handle;
    handle.id         = m_nextTaskId.fetch_add(1ULL, std::memory_order_relaxed);
    handle.generation = 1U;
    return handle;
}

TaskControlBlock* ScheduleSubsystem::findTaskControlBlock(const TaskHandle& handle)
{
    const auto controlBlockIt = m_taskControlBlocks.find(handle);
    return controlBlockIt != m_taskControlBlocks.end() ? &controlBlockIt->second : nullptr;
}

const TaskControlBlock* ScheduleSubsystem::findTaskControlBlock(const TaskHandle& handle) const
{
    const auto controlBlockIt = m_taskControlBlocks.find(handle);
    return controlBlockIt != m_taskControlBlocks.end() ? &controlBlockIt->second : nullptr;
}

TaskControlBlock* ScheduleSubsystem::findTaskControlBlock(const RunnableTask* task)
{
    const auto handleIt = m_taskHandleByPointer.find(const_cast<RunnableTask*>(task));
    if (handleIt == m_taskHandleByPointer.end())
    {
        return nullptr;
    }

    return findTaskControlBlock(handleIt->second);
}

const TaskControlBlock* ScheduleSubsystem::findTaskControlBlock(const RunnableTask* task) const
{
    const auto handleIt = m_taskHandleByPointer.find(const_cast<RunnableTask*>(task));
    if (handleIt == m_taskHandleByPointer.end())
    {
        return nullptr;
    }

    return findTaskControlBlock(handleIt->second);
}
