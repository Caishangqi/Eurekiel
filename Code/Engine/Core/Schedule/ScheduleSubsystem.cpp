#include "ScheduleSubsystem.hpp"
#include "TaskWorkerThread.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Yaml.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

using namespace enigma::core;

ScheduleSubsystem* g_theSchedule = nullptr;

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
    // Debug logging causes 60% of AddTask time due to synchronous console writes
    // All categories (including ScheduleSubsystem) inherit ERROR level from configuration
}

ScheduleSubsystem::~ScheduleSubsystem()
{
    if (!m_workerThreads.empty())
    {
        LogWarn(ScheduleSubsystem::GetStaticSubsystemName(),
                "ScheduleSubsystem destroyed without Shutdown()!");
    }
}

//-----------------------------------------------------------------------------------------------
// Startup: Initialize subsystem (NOW Logger is ready, safe to log)
//-----------------------------------------------------------------------------------------------
void ScheduleSubsystem::Startup()
{
    LogInfo(ScheduleSubsystem::GetStaticSubsystemName(),
            "Startup() - Phase 2 (YAML-driven), loaded %d task types",
            (int)m_config.task_types.size());

    for (const auto& typeDef : m_config.task_types)
    {
        m_typeRegistry.RegisterType(typeDef.type, typeDef.threads);
        LogInfo(ScheduleSubsystem::GetStaticSubsystemName(),
                "Registered type '%s' with %d threads",
                typeDef.type.c_str(), typeDef.threads);
    }

    LogInfo(ScheduleSubsystem::GetStaticSubsystemName(),
            "Registered %d task types, total %d threads",
            (int)m_typeRegistry.GetAllTypes().size(),
            m_typeRegistry.GetTotalThreadCount());

    CreateWorkerThreads();
    LogInfo(ScheduleSubsystem::GetStaticSubsystemName(), "Startup complete");
}

//-----------------------------------------------------------------------------------------------
// Shutdown: Stop worker threads and cleanup
//-----------------------------------------------------------------------------------------------
void ScheduleSubsystem::Shutdown()
{
    LogInfo(ScheduleSubsystem::GetStaticSubsystemName(), "Shutdown() called");

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
    for (RunnableTask* task : m_completedTasks) { delete task; }

    m_pendingTasksByType.clear();
    m_executingTasks.clear();
    m_completedTasks.clear();

    LogInfo(ScheduleSubsystem::GetStaticSubsystemName(), "Shutdown complete");
}

void ScheduleSubsystem::AddTask(RunnableTask* task, TaskPriority priority)
{
    if (!task)
    {
        LogError(ScheduleSubsystem::GetStaticSubsystemName(),
                 "Attempted to add null task");
        return;
    }

    std::string taskType    = task->GetType();
    const char* priorityStr = (priority == TaskPriority::High) ? "High" : "Normal";

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);

        // Insert into layered map: Type -> Priority -> Queue
        // This is O(log k) where k is number of task types (typically < 10)
        m_pendingTasksByType[taskType][priority].push_back(task);

        int totalPending = 0;
        for (const auto& typePair : m_pendingTasksByType)
        {
            for (const auto& priorityPair : typePair.second)
            {
                totalPending += (int)priorityPair.second.size();
            }
        }

        LogInfo(ScheduleSubsystem::GetStaticSubsystemName(),
                "AddTask: Added task type='%s' priority=%s (total pending: %d)",
                taskType.c_str(), priorityStr, totalPending);
    }

    // Notify only workers of the matching type (Plan 1: Per-Type Condition Variables)
    m_typeConditionVariables[taskType].notify_one();

    LogInfo(ScheduleSubsystem::GetStaticSubsystemName(),
            "AddTask: Notified workers of type '%s'", taskType.c_str());
}

std::vector<RunnableTask*> ScheduleSubsystem::RetrieveCompletedTasks()
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    std::vector<RunnableTask*>  result = m_completedTasks;
    m_completedTasks.clear();
    return result;
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

        task->SetState(TaskState::Completed);

        auto it = std::find(m_executingTasks.begin(), m_executingTasks.end(), task);
        if (it != m_executingTasks.end())
        {
            m_executingTasks.erase(it);
        }

        m_completedTasks.push_back(task);

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
    LogInfo(ScheduleSubsystem::GetStaticSubsystemName(), "Creating worker threads...");

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

    LogInfo(ScheduleSubsystem::GetStaticSubsystemName(),
            "Created %d worker threads", globalWorkerID);
}

void ScheduleSubsystem::DestroyWorkerThreads()
{
    LogInfo(ScheduleSubsystem::GetStaticSubsystemName(),
            "Destroying %d worker threads...", (int)m_workerThreads.size());

    for (TaskWorkerThread* worker : m_workerThreads)
    {
        delete worker;
    }

    m_workerThreads.clear();
    LogInfo(ScheduleSubsystem::GetStaticSubsystemName(), "All worker threads destroyed");
}

bool ScheduleSubsystem::HasPendingTaskOfType(const std::string& typeStr) const
{
    // O(log k) lookup in layered map
    auto typeIt = m_pendingTasksByType.find(typeStr);
    if (typeIt == m_pendingTasksByType.end())
    {
        LogDebug(ScheduleSubsystem::GetStaticSubsystemName(),
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
            LogDebug(ScheduleSubsystem::GetStaticSubsystemName(),
                     "HasPendingTaskOfType('%s'): Found %d tasks in priority queue",
                     typeStr.c_str(), (int)priorityPair.second.size());
            return true;
        }
    }

    LogDebug(ScheduleSubsystem::GetStaticSubsystemName(),
             "HasPendingTaskOfType('%s'): No tasks found (all queues empty)",
             typeStr.c_str());
    return false;
}

//-----------------------------------------------------------------------------------------------
// GetConditionVariableForType: Return type-specific condition variable (Plan 1)
// Auto-creates CV on first access for each type
//-----------------------------------------------------------------------------------------------
std::condition_variable& ScheduleSubsystem::GetConditionVariableForType(const std::string& typeStr)
{
    return m_typeConditionVariables[typeStr];
}
