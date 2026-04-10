#pragma once

#include "ScheduleTaskTypes.hpp"

namespace enigma::core
{
    //-----------------------------------------------------------------------------------------------
    // TaskControlBlock
    // Internal scheduler metadata for one submitted task.
    //-----------------------------------------------------------------------------------------------
    struct TaskControlBlock
    {
        TaskHandle              handle;
        RunnableTask*           task                 = nullptr;
        TaskState               state                = TaskState::Queued;
        bool                    cancellationRequested = false;
        bool                    supportsCancellation  = false;
        bool                    wasCancelled          = false;
        bool                    isStale               = false;
        std::optional<TaskKey>  taskKey;
        std::optional<uint64_t> version;
        KeyedTaskPolicy         keyedPolicy           = KeyedTaskPolicy::AllowDuplicates;
        TaskPolicyDecision      policyDecision        = TaskPolicyDecision::Executed;

        bool IsTerminal() const
        {
            return state == TaskState::Completed || state == TaskState::Cancelled || state == TaskState::Failed;
        }

        TaskCompletionRecord ToCompletionRecord() const
        {
            TaskCompletionRecord record;
            record.handle         = handle;
            record.task           = task;
            record.finalState     = state;
            record.taskKey        = taskKey;
            record.version        = version;
            record.wasCancelled   = wasCancelled;
            record.isStale        = isStale;
            record.policyDecision = policyDecision;
            return record;
        }
    };
}
