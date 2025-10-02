#include "TaskWorkerThread.hpp"
#include "ScheduleSubsystem.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

namespace enigma::core
{
    TaskWorkerThread::TaskWorkerThread(int workerID, const std::string& assignedType, ScheduleSubsystem* system)
        : m_workerID(workerID)
        , m_assignedType(assignedType)
        , m_system(system)
        , m_thread(nullptr)
    {
        m_thread = new std::thread(&TaskWorkerThread::ThreadMain, this);
    }

    TaskWorkerThread::~TaskWorkerThread()
    {
        if (m_thread && m_thread->joinable())
        {
            m_thread->join();
        }

        delete m_thread;
        m_thread = nullptr;
    }

    //-----------------------------------------------------------------------------------------------
    // ThreadMain: Worker thread's main execution loop (PHASE 2: Condition Variable Optimization)
    //
    // ALGORITHM (Optimized Producer-Consumer Pattern):
    // 1. Loop until shutdown signal received
    // 2. Lock mutex and wait on condition variable for:
    //    - Shutdown signal OR
    //    - Task of our type is available
    // 3. If task found: execute it, mark as completed
    // 4. Repeat
    //
    // PHASE 2 IMPROVEMENTS:
    // - Replaces sleep_for() with condition_variable::wait()
    // - Worker threads sleep efficiently (no CPU waste)
    // - Wake immediately when AddTask() notifies new work
    // - Predicate lambda prevents spurious wakeups
    //
    // EDUCATIONAL NOTE - Condition Variable Pattern:
    // 1. unique_lock: RAII-style mutex lock (can unlock/relock)
    // 2. cv.wait(lock, predicate): Atomically releases lock and sleeps
    // 3. When notified: wakes up, reacquires lock, checks predicate
    // 4. If predicate true: proceeds; if false: releases lock and sleeps again
    // 5. This prevents "lost wakeup" and "spurious wakeup" problems
    //
    // WHY THIS IS BETTER THAN PHASE 1:
    // - Phase 1: sleep_for(10ms) = wastes CPU polling, 10ms latency
    // - Phase 2: wait() = zero CPU when idle, instant wakeup (<1ms latency)
    //
    // THREAD SAFETY:
    // - unique_lock ensures mutex held when accessing queues
    // - Predicate lambda safely checks shutdown and task availability
    // - Execute() runs outside critical section (parallel execution)
    //-----------------------------------------------------------------------------------------------
    void TaskWorkerThread::ThreadMain()
    {
        LogDebug(ScheduleSubsystem::GetStaticSubsystemName(),
                 "Worker #%d (type='%s') started (Phase 2: CV optimization)",
                 m_workerID, m_assignedType.c_str());

        // Main worker loop: run until shutdown signal
        while (!m_system->IsShuttingDown())
        {
            RunnableTask* task = nullptr;

            // PHASE 2: Use condition variable for efficient waiting
            {
                std::unique_lock<std::mutex> lock(m_system->GetQueueMutex());

                // Wait until: shutdown requested OR task of our type is available
                // PREDICATE: Return true to wake up and proceed
                m_system->GetTaskAvailableCV().wait(lock, [this]() {
                    // Wake up if shutting down (need to exit loop)
                    if (m_system->IsShuttingDown())
                        return true;
                    
                    // Wake up if task of our type is pending
                    return m_system->HasPendingTaskOfType(m_assignedType);
                });

                // If we woke due to shutdown, exit loop
                if (m_system->IsShuttingDown())
                    break;

                // Otherwise, we woke because task is available
                // NOTE: We still hold the lock here, so GetNextTaskForType() is safe
                task = m_system->GetNextTaskForType(m_assignedType);
            }
            // Mutex automatically released here (unique_lock destructor)

            // Execute task outside critical section (allows parallel execution)
            if (task)
            {
                LogDebug(ScheduleSubsystem::GetStaticSubsystemName(),
                        "Worker #%d executing task of type='%s'",
                        m_workerID, task->GetType().c_str());

                task->SetState(TaskState::Executing);
                task->Execute();  // This may take time (file I/O, computation, etc.)

                // Mark task as completed (re-enters critical section internally)
                m_system->OnTaskCompleted(task);

                LogDebug(ScheduleSubsystem::GetStaticSubsystemName(),
                        "Worker #%d completed task of type='%s'",
                        m_workerID, task->GetType().c_str());
            }
        }

        LogDebug(ScheduleSubsystem::GetStaticSubsystemName(),
                "Worker #%d exiting", m_workerID);
    }
}
