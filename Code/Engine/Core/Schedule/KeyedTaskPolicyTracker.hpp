#pragma once

#include "ScheduleTaskTypes.hpp"

#include <optional>
#include <unordered_map>

namespace enigma::core
{
    enum class KeyedTaskStage : uint8_t
    {
        Queued = 0,
        Executing
    };

    struct KeyedTaskSubmissionDecision
    {
        bool                     shouldQueueTask = true;
        TaskHandle               resolvedHandle;
        TaskPolicyDecision       policyDecision  = TaskPolicyDecision::Executed;
        std::optional<TaskHandle> existingHandle;
        std::optional<TaskHandle> supersededHandle;
    };

    struct TaskFreshnessEvaluation
    {
        bool               isStale        = false;
        TaskPolicyDecision policyDecision = TaskPolicyDecision::Executed;
    };

    class KeyedTaskPolicyTracker
    {
    public:
        KeyedTaskSubmissionDecision RegisterSubmission(const TaskHandle& handle, const TaskSubmissionOptions& options);
        void MarkTaskExecuting(const TaskHandle& handle);
        TaskFreshnessEvaluation EvaluateCompletion(const TaskHandle& handle) const;
        void MarkTaskTerminal(const TaskHandle& handle);

        bool IsTrackingTask(const TaskHandle& handle) const;
        void Clear();

    private:
        struct TrackedTaskMetadata
        {
            TaskKey                key;
            KeyedTaskPolicy        policy  = KeyedTaskPolicy::AllowDuplicates;
            std::optional<uint64_t> version;
            KeyedTaskStage         stage   = KeyedTaskStage::Queued;
            bool                   superseded = false;
        };

        struct KeyedTaskEntry
        {
            std::optional<TaskHandle> latestSubmittedHandle;
            std::optional<uint64_t>   latestSubmittedVersion;
            std::optional<TaskHandle> queuedHandle;
            size_t                    liveTaskCount = 0;
        };

    private:
        KeyedTaskSubmissionDecision registerLatestOnlySubmission(
            const TaskHandle& handle,
            const TaskSubmissionOptions& options);

    private:
        std::unordered_map<TaskHandle, TrackedTaskMetadata, TaskHandleHasher> m_metadataByHandle;
        std::unordered_map<TaskKey, KeyedTaskEntry, TaskKeyHasher>            m_entriesByKey;
    };
}
