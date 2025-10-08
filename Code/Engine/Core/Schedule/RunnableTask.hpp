#pragma once
#include <atomic>
#include <string>

namespace enigma::core
{
    //-----------------------------------------------------------------------------------------------
    // Task State Enumeration
    //-----------------------------------------------------------------------------------------------
    enum class TaskState : uint8_t
    {
        Queued = 0, // Queued and waiting for execution
        Executing, // Currently executing
        Completed // Execution completed
    };

    //-----------------------------------------------------------------------------------------------
    // Task Type Constants (for convenience and type safety)
    //-----------------------------------------------------------------------------------------------
    namespace TaskTypeConstants
    {
        constexpr const char* GENERIC   = "Generic";
        constexpr const char* FILE_IO   = "FileIO";
        constexpr const char* CHUNK_GEN = "ChunkGen";
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

        // State management (called by ScheduleSubsystem)
        void SetState(TaskState newState) { m_state.store(newState); }

    protected:
        std::string m_type = TaskTypeConstants::GENERIC; // Task type (string)
        friend class ScheduleSubsystem; // Allow ScheduleSubsystem to modify state
    private:
        std::atomic<TaskState> m_state; // Atomic state (thread-safe)
    };
}
