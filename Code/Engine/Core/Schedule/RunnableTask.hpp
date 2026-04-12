#pragma once
#include <atomic>
#include <string>

#include "TaskHandle.hpp"

namespace enigma::core
{
    //-----------------------------------------------------------------------------------------------
    // Task State Enumeration
    //-----------------------------------------------------------------------------------------------
    enum class TaskState : uint8_t
    {
        Queued = 0, // Queued and waiting for execution
        Executing, // Currently executing
        CancelRequested, // Cancellation requested and awaiting terminal resolution
        Completed, // Execution completed successfully
        Cancelled, // Execution terminated due to cancellation
        Failed // Execution terminated due to failure
    };

    //-----------------------------------------------------------------------------------------------
    // Task Type Constants (for convenience and type safety)
    //-----------------------------------------------------------------------------------------------
    namespace TaskTypeConstants
    {
        constexpr const char* GENERIC   = "Generic";
        constexpr const char* FILE_IO   = "FileIO";
        constexpr const char* CHUNK_GEN = "ChunkGen";
        constexpr const char* MESH_BUILDING = "MeshBuilding";
        constexpr const char* RENDERING = "Rendering";
    }

    //-----------------------------------------------------------------------------------------------
    // Abstract Task Base Class (dynamic string-based type)
    //-----------------------------------------------------------------------------------------------
    class RunnableTask
    {
    public:
        // Constructor: specify task type with string
        explicit RunnableTask(const std::string& typeStr);
        explicit RunnableTask();
        virtual  ~RunnableTask() = default;

        // Pure virtual: derived classes implement specific task logic
        virtual void Execute() = 0;

        // Accessors
        const std::string& GetType() const { return m_type; }
        TaskState          GetState() const { return m_state.load(); }
        const TaskHandle&  GetHandle() const { return m_handle; }
        bool               HasHandle() const { return m_handle.IsValid(); }

        // State management (called by ScheduleSubsystem)
        void SetState(TaskState newState) { m_state.store(newState); }

        // Cancellation support shared by all scheduler tasks
        bool RequestCancellation()
        {
            return !m_cancellationRequested.exchange(true, std::memory_order_acq_rel);
        }

        bool IsCancellationRequested() const
        {
            return m_cancellationRequested.load(std::memory_order_acquire);
        }

        bool IsTerminalState() const
        {
            const TaskState state = GetState();
            return state == TaskState::Completed || state == TaskState::Cancelled || state == TaskState::Failed;
        }

    protected:
        std::string m_type = TaskTypeConstants::GENERIC; // Task type (string)

    private:
        void AttachHandle(const TaskHandle& handle) { m_handle = handle; }

        friend class ScheduleSubsystem; // Allow ScheduleSubsystem to modify state and handle attachment

    private:
        std::atomic<TaskState> m_state; // Atomic state (thread-safe)
        std::atomic<bool>      m_cancellationRequested{false}; // Shared cancellation flag
        TaskHandle             m_handle; // Stable task identity assigned by scheduler
    };
}
