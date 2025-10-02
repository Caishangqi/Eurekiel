#pragma once
#include <thread>
#include <string>

namespace enigma::core
{
    class ScheduleSubsystem;  // Forward declaration

    //-----------------------------------------------------------------------------------------------
    // Worker Thread Class
    // Retrieves and executes tasks from the scheduling system
    //-----------------------------------------------------------------------------------------------
    class TaskWorkerThread
    {
    public:
        // Constructor: specify task type with string
        TaskWorkerThread(int workerID, const std::string& assignedType, ScheduleSubsystem* system);
        ~TaskWorkerThread();

        // Disable copy and move
        TaskWorkerThread(const TaskWorkerThread&) = delete;
        TaskWorkerThread& operator=(const TaskWorkerThread&) = delete;

        // Get thread information
        int GetWorkerID() const { return m_workerID; }
        const std::string& GetAssignedType() const { return m_assignedType; }

    private:
        // Thread entry point
        void ThreadMain();

    private:
        int m_workerID;                      // Unique thread ID
        std::string m_assignedType;          // Task type handled (string)
        ScheduleSubsystem* m_system;         // Scheduling system pointer
        std::thread* m_thread;               // Underlying thread object
    };
}
