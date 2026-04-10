#include "KeyedTaskPolicyTracker.hpp"

#include "ScheduleException.hpp"

namespace enigma::core
{
    KeyedTaskSubmissionDecision KeyedTaskPolicyTracker::RegisterSubmission(
        const TaskHandle& handle,
        const TaskSubmissionOptions& options)
    {
        KeyedTaskSubmissionDecision decision;
        decision.resolvedHandle = handle;

        if (!options.taskKey.has_value())
        {
            return decision;
        }

        if (options.keyedPolicy == KeyedTaskPolicy::CoalescePending)
        {
            const auto entryIt = m_entriesByKey.find(*options.taskKey);
            if (entryIt != m_entriesByKey.end() && entryIt->second.queuedHandle.has_value())
            {
                const TaskHandle queuedHandle = *entryIt->second.queuedHandle;
                const auto metadataIt = m_metadataByHandle.find(queuedHandle);
                if (metadataIt != m_metadataByHandle.end() && metadataIt->second.stage == KeyedTaskStage::Queued)
                {
                    decision.shouldQueueTask = false;
                    decision.resolvedHandle  = queuedHandle;
                    decision.existingHandle  = queuedHandle;
                    decision.policyDecision  = TaskPolicyDecision::CoalescedToExisting;
                    return decision;
                }
            }
        }

        if (options.keyedPolicy == KeyedTaskPolicy::AllowDuplicates)
        {
            TrackedTaskMetadata metadata;
            metadata.key     = *options.taskKey;
            metadata.policy  = options.keyedPolicy;
            metadata.version = options.version;
            m_metadataByHandle[handle] = metadata;

            KeyedTaskEntry& entry = m_entriesByKey[*options.taskKey];
            entry.liveTaskCount++;
            return decision;
        }

        if (options.keyedPolicy == KeyedTaskPolicy::LatestOnly ||
            options.keyedPolicy == KeyedTaskPolicy::CoalescePending)
        {
            return registerLatestOnlySubmission(handle, options);
        }

        throw UnsupportedTaskPolicyException("KeyedTaskPolicyTracker: Unsupported keyed task policy");
    }

    void KeyedTaskPolicyTracker::MarkTaskExecuting(const TaskHandle& handle)
    {
        const auto metadataIt = m_metadataByHandle.find(handle);
        if (metadataIt == m_metadataByHandle.end())
        {
            return;
        }

        metadataIt->second.stage = KeyedTaskStage::Executing;

        auto entryIt = m_entriesByKey.find(metadataIt->second.key);
        if (entryIt == m_entriesByKey.end())
        {
            return;
        }

        if (entryIt->second.queuedHandle.has_value() && *entryIt->second.queuedHandle == handle)
        {
            entryIt->second.queuedHandle.reset();
        }
    }

    TaskFreshnessEvaluation KeyedTaskPolicyTracker::EvaluateCompletion(const TaskHandle& handle) const
    {
        TaskFreshnessEvaluation evaluation;

        const auto metadataIt = m_metadataByHandle.find(handle);
        if (metadataIt == m_metadataByHandle.end())
        {
            return evaluation;
        }

        const TrackedTaskMetadata& metadata = metadataIt->second;
        if (metadata.policy == KeyedTaskPolicy::AllowDuplicates)
        {
            return evaluation;
        }

        const auto entryIt = m_entriesByKey.find(metadata.key);
        if (entryIt == m_entriesByKey.end())
        {
            return evaluation;
        }

        const bool latestHandleMismatch =
            entryIt->second.latestSubmittedHandle.has_value() &&
            *entryIt->second.latestSubmittedHandle != handle;

        const bool olderVersion =
            metadata.version.has_value() &&
            entryIt->second.latestSubmittedVersion.has_value() &&
            *metadata.version < *entryIt->second.latestSubmittedVersion;

        if (metadata.superseded || latestHandleMismatch || olderVersion)
        {
            evaluation.isStale = true;
            evaluation.policyDecision =
                metadata.stage == KeyedTaskStage::Executing
                ? TaskPolicyDecision::SupersededAfterExecution
                : TaskPolicyDecision::SupersededBeforeExecution;
        }

        return evaluation;
    }

    void KeyedTaskPolicyTracker::MarkTaskTerminal(const TaskHandle& handle)
    {
        const auto metadataIt = m_metadataByHandle.find(handle);
        if (metadataIt == m_metadataByHandle.end())
        {
            return;
        }

        const TaskKey key = metadataIt->second.key;
        auto entryIt = m_entriesByKey.find(key);
        if (entryIt != m_entriesByKey.end())
        {
            if (entryIt->second.queuedHandle.has_value() && *entryIt->second.queuedHandle == handle)
            {
                entryIt->second.queuedHandle.reset();
            }

            if (entryIt->second.liveTaskCount > 0)
            {
                entryIt->second.liveTaskCount--;
            }

            if (entryIt->second.liveTaskCount == 0)
            {
                m_entriesByKey.erase(entryIt);
            }
        }

        m_metadataByHandle.erase(metadataIt);
    }

    bool KeyedTaskPolicyTracker::IsTrackingTask(const TaskHandle& handle) const
    {
        return m_metadataByHandle.find(handle) != m_metadataByHandle.end();
    }

    void KeyedTaskPolicyTracker::Clear()
    {
        m_metadataByHandle.clear();
        m_entriesByKey.clear();
    }

    KeyedTaskSubmissionDecision KeyedTaskPolicyTracker::registerLatestOnlySubmission(
        const TaskHandle& handle,
        const TaskSubmissionOptions& options)
    {
        KeyedTaskSubmissionDecision decision;
        decision.resolvedHandle = handle;

        KeyedTaskEntry& entry = m_entriesByKey[*options.taskKey];
        if (entry.latestSubmittedHandle.has_value())
        {
            decision.supersededHandle = entry.latestSubmittedHandle;

            const auto metadataIt = m_metadataByHandle.find(*entry.latestSubmittedHandle);
            if (metadataIt != m_metadataByHandle.end())
            {
                metadataIt->second.superseded = true;
            }
        }

        TrackedTaskMetadata metadata;
        metadata.key     = *options.taskKey;
        metadata.policy  = options.keyedPolicy;
        metadata.version = options.version;
        m_metadataByHandle[handle] = metadata;

        entry.latestSubmittedHandle   = handle;
        entry.latestSubmittedVersion  = options.version;
        entry.queuedHandle            = handle;
        entry.liveTaskCount++;

        return decision;
    }
}
