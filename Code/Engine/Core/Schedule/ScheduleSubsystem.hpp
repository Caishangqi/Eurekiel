#pragma once
#include "Engine/Core/SubsystemManager.hpp"
#include "TaskTypeRegistry.hpp"
#include "RunnableTask.hpp"
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>
#include <map> // For per-type condition variables and priority queues
#include <deque> // For task queues

#include "Engine/Core/LogCategory/LogCategory.hpp"

DECLARE_LOG_CATEGORY_EXTERN(LogSchedule)

namespace enigma::core
{
    // Forward declarations
    class TaskWorkerThread;
    class YamlConfiguration;

    //-----------------------------------------------------------------------------------------------
    // Task Priority Enum
    // Used to prioritize tasks in the pending queue
    // - Normal: Default priority for world generation, file I/O, etc.
    // - High: High priority for player interaction, immediate response needed
    //-----------------------------------------------------------------------------------------------
    enum class TaskPriority
    {
        Normal = 0, // Default priority
        High = 1 // High priority (processed first)
    };

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
    // - Three-queue model: Pending -> Executing -> Completed
    // - Thread-safe queue operations using mutex/condition_variable
    // - Dynamic task type registration from YAML (no recompilation needed)
    //
    // LIFECYCLE:
    // 1. Construction: Initialize config reference
    // 2. Startup(): Load YAML config -> Create type registry -> Allocate worker threads
    // 3. Runtime: AddTask() -> Workers execute -> RetrieveCompletedTasks()
    // 4. Shutdown(): Signal workers to stop, join threads, cleanup
    //
    // THREAD SAFETY:
    // - All queue operations protected by m_queueMutex
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
        // PHASE 1 API: Basic Task Management
        //-------------------------------------------------------------------------------------------

        // Add a task to the pending queue (thread-safe)
        // The task will be picked up by a worker thread of matching type
        // Priority parameter: Normal (default) for world generation, High for player interaction
        void AddTask(RunnableTask* task, TaskPriority priority = TaskPriority::Normal);

        // Retrieve all completed tasks and clear the completed queue (thread-safe)
        // Caller is responsible for deleting the tasks
        std::vector<RunnableTask*> RetrieveCompletedTasks();

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

    private:
        //-------------------------------------------------------------------------------------------
        // Configuration & Registry
        //-------------------------------------------------------------------------------------------
        ScheduleConfig&  m_config; // Configuration reference
        TaskTypeRegistry m_typeRegistry; // Task type -> thread count mapping

        //-------------------------------------------------------------------------------------------
        // Three-Queue System (Pending -> Executing -> Completed)
        // Pending tasks organized by Type -> Priority -> Queue for O(1) high-priority access
        //-------------------------------------------------------------------------------------------
        // Pending tasks: map<TaskType, map<Priority, deque<Task>>>
        // Example: m_pendingTasksByType["ChunkGen"][TaskPriority::High] = deque of high-priority ChunkGen tasks
        std::map<std::string, std::map<TaskPriority, std::deque<RunnableTask*>>> m_pendingTasksByType;

        std::vector<RunnableTask*> m_executingTasks; // Tasks currently being executed
        std::vector<RunnableTask*> m_completedTasks; // Tasks finished execution

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
