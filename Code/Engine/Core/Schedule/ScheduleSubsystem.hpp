#pragma once
#include "Engine/Core/SubsystemManager.hpp"
#include "KeyedTaskPolicyTracker.hpp"
#include "TaskTypeRegistry.hpp"
#include "TaskControlBlock.hpp"
#include "RunnableTask.hpp"
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>
#include <map> // For per-type condition variables and priority queues
#include <deque> // For task queues
#include <unordered_map>

#include "Engine/Core/LogCategory/LogCategory.hpp"

DECLARE_LOG_CATEGORY_EXTERN(LogSchedule)

namespace enigma::core
{
    // Forward declarations
    class TaskWorkerThread;
    class YamlConfiguration;

    //-----------------------------------------------------------------------------------------------
    // Task Type Definition (from YAML configuration)
    // Represents a single task type entry in the schedule.yml file
    //-----------------------------------------------------------------------------------------------
    struct TaskTypeDefinition
    {
        std::string type; // Task type identifier (e.g., "Generic", "FileIO")
        int         threads; // Number of worker threads for this type
        std::string description; // Human-readable description (optional)

        TaskTypeDefinition() : threads(1)
        {
        }

        TaskTypeDefinition(const std::string& t, int th, const std::string& desc = "")
            : type(t), threads(th), description(desc)
        {
        }
    };

    //-----------------------------------------------------------------------------------------------
    // Schedule Subsystem Configuration
    // Loaded from YAML configuration file at engine startup
    //
    // YAML File Structure (schedule.yml):
    //   task_types:
    //     - type: Generic
    //       threads: 4
    //       description: General-purpose tasks
    //     - type: FileIO
    //       threads: 2
    //       description: File I/O operations
    //
    // LOADING:
    // - LoadFromYaml(YamlConfiguration&): Parse YAML and populate task_types
    // - LoadFromFile(filePath): Convenience method to load from file path
    // - GetDefaultConfig(): Returns hardcoded defaults if YAML file missing
    //-----------------------------------------------------------------------------------------------
    struct ScheduleConfig
    {
        std::vector<TaskTypeDefinition> task_types; // Task type definitions from YAML

        // Load configuration from YAML object
        void LoadFromYaml(const YamlConfiguration& yaml);

        // Load configuration from YAML file path
        // Returns true if successful, false if file not found (uses defaults)
        bool LoadFromFile(const std::string& filePath);

        // Get default configuration (hardcoded fallback)
        static ScheduleConfig GetDefaultConfig();
    };

    //-----------------------------------------------------------------------------------------------
    // Schedule Subsystem (Phase 2: Per-Type Condition Variables (Plan 1))
    //
    // DESIGN PHILOSOPHY:
    // - Multi-threaded task scheduling system with type-based thread pools
    // - Explicit scheduler state flow: Pending -> Executing -> CompletionRecord drain
    // - Thread-safe queue operations using mutex/condition_variable
    // - Dynamic task type registration from YAML (no recompilation needed)
    //
    // LIFECYCLE:
    // 1. Construction: Initialize config reference
    // 2. Startup(): Load YAML config -> Create type registry -> Allocate worker threads
    // 3. Runtime: SubmitTask() -> Workers execute -> DrainCompletedTaskRecords()
    // 4. Shutdown(): Signal workers to stop, join threads, cleanup
    //
    // THREAD SAFETY:
    // - Scheduler state containers protected by m_queueMutex
    // - Task state uses std::atomic for lock-free reads
    // - Per-type condition variables eliminate spurious wakeups
    //-----------------------------------------------------------------------------------------------
    class ScheduleSubsystem : public EngineSubsystem
    {
    public:
        DECLARE_SUBSYSTEM(ScheduleSubsystem, "ScheduleSubsystem", 50)

    public:
        // Constructor now takes optional config file path instead of config object
        explicit ScheduleSubsystem(ScheduleConfig& config);
        ~ScheduleSubsystem() override;

        // EngineSubsystem Interface
        void Startup() override;
        void Shutdown() override;

        //-------------------------------------------------------------------------------------------
        // Modern Task Management API
        //-------------------------------------------------------------------------------------------

        // Modern scheduler submission API.
        TaskHandle SubmitTask(RunnableTask* task, const TaskSubmissionOptions& options = {});

        // Modern task control API.
        bool      RequestTaskCancellation(const TaskHandle& handle);
        TaskState GetTaskState(const TaskHandle& handle) const;

        // Modern completion drain API.
        TaskResultDrainView DrainCompletedTaskRecords();

        // Get the type registry (for manual type registration in Phase 1)
        TaskTypeRegistry& GetTypeRegistry() { return m_typeRegistry; }

        //-------------------------------------------------------------------------------------------
        // PHASE 1 API: Worker Thread Support
        //-------------------------------------------------------------------------------------------

        // Get next task for a specific worker type (called by TaskWorkerThread)
        // Returns nullptr if no tasks available for this type
        RunnableTask* GetNextTaskForType(const std::string& typeStr);

        // Mark a task as completed and move it to completed queue
        void OnTaskCompleted(RunnableTask* task);

        // Check if shutdown is requested (workers check this in their loop)
        bool IsShuttingDown() const { return m_isShuttingDown.load(); }

        //-------------------------------------------------------------------------------------------
        // PHASE 2 API: Optimized Worker Thread Support (Per-Type Condition Variables - Plan 1)
        //-------------------------------------------------------------------------------------------

        // Access to synchronization primitives for efficient condition variable waiting
        // Workers use these to wait() instead of busy-looping with sleep_for()
        std::mutex& GetQueueMutex() { return m_queueMutex; }
        // Get type-specific condition variable for a task type
        // Returns a reference to the CV for precise worker notification (eliminates spurious wakeups)
        std::condition_variable& GetConditionVariableForType(const std::string& typeStr);

        // Check if there are any pending tasks of the specified type (for CV predicate)
        // PRECONDITION: Caller must hold m_queueMutex
        bool HasPendingTaskOfType(const std::string& typeStr) const;

        //-------------------------------------------------------------------------------------------
        // Query API
        //-------------------------------------------------------------------------------------------
        int32_t GetPendingTaskCount(const std::string& typeStr) const;
        int32_t GetExecutingTaskCount(const std::string& typeStr) const;
        int32_t GetCompletedTaskCount(const std::string& typeStr) const;

        // Shutdown support: Check whether a task of the specified type is being executed
        bool HasExecutingTasks(const std::string& taskType) const;

    private:
        //-------------------------------------------------------------------------------------------
        // Internal Helpers
        //-------------------------------------------------------------------------------------------

        // Create worker threads based on registered types
        void CreateWorkerThreads();

        // Destroy all worker threads (called during Shutdown)
        void DestroyWorkerThreads();

        TaskHandle allocateTaskHandle();
        TaskControlBlock* findTaskControlBlock(const TaskHandle& handle);
        const TaskControlBlock* findTaskControlBlock(const TaskHandle& handle) const;
        TaskControlBlock* findTaskControlBlock(const RunnableTask* task);
        const TaskControlBlock* findTaskControlBlock(const RunnableTask* task) const;

    private:
        //-------------------------------------------------------------------------------------------
        // Configuration & Registry
        //-------------------------------------------------------------------------------------------
        ScheduleConfig&  m_config; // Configuration reference
        TaskTypeRegistry m_typeRegistry; // Task type -> thread count mapping

        //-------------------------------------------------------------------------------------------
        // Scheduler State Containers
        // Pending tasks organized by Type -> Priority -> Queue for O(1) high-priority access
        //-------------------------------------------------------------------------------------------
        // Pending tasks: map<TaskType, map<Priority, deque<Task>>>
        // Example: m_pendingTasksByType["ChunkGen"][TaskPriority::High] = deque of high-priority ChunkGen tasks
        std::map<std::string, std::map<TaskPriority, std::deque<RunnableTask*>>> m_pendingTasksByType;

        std::vector<RunnableTask*> m_executingTasks; // Tasks currently being executed
        std::vector<TaskCompletionRecord> m_completedTaskRecords; // Modern completion transport
        std::unordered_map<TaskHandle, TaskControlBlock, TaskHandleHasher> m_taskControlBlocks;
        std::unordered_map<RunnableTask*, TaskHandle> m_taskHandleByPointer;
        KeyedTaskPolicyTracker m_keyedTaskPolicyTracker;
        std::atomic<uint64_t> m_nextTaskId{1ULL};

        //-------------------------------------------------------------------------------------------
        // Thread Synchronization (Per-Type Condition Variables - Plan 1)
        //-------------------------------------------------------------------------------------------
        mutable std::mutex                             m_queueMutex; // Protects all three queues
        std::map<std::string, std::condition_variable> m_typeConditionVariables; // One CV per task type
        std::atomic<bool>                              m_isShuttingDown{false}; // Shutdown flag (atomic for lock-free read)

        //-------------------------------------------------------------------------------------------
        // Worker Thread Pool
        //-------------------------------------------------------------------------------------------
        std::vector<TaskWorkerThread*> m_workerThreads; // All worker threads (indexed by ID)
    };
}
