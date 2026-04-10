#pragma once

#include "RunnableTask.hpp"
#include "TaskHandle.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace enigma::core
{
    //-----------------------------------------------------------------------------------------------
    // Task Priority
    // Shared priority model for scheduler submission options.
    //-----------------------------------------------------------------------------------------------
    enum class TaskPriority : uint8_t
    {
        Normal = 0,
        High   = 1
    };

    //-----------------------------------------------------------------------------------------------
    // TaskKey
    // Logical identity for keyed render-task workflows.
    //-----------------------------------------------------------------------------------------------
    struct TaskKey
    {
        std::string domain;
        uint64_t    value = 0ULL;

        bool IsValid() const
        {
            return !domain.empty();
        }
    };

    inline bool operator==(const TaskKey& lhs, const TaskKey& rhs)
    {
        return lhs.domain == rhs.domain && lhs.value == rhs.value;
    }

    inline bool operator!=(const TaskKey& lhs, const TaskKey& rhs)
    {
        return !(lhs == rhs);
    }

    struct TaskKeyHasher
    {
        std::size_t operator()(const TaskKey& key) const
        {
            const std::size_t domainHash = std::hash<std::string>{}(key.domain);
            const std::size_t valueHash  = std::hash<uint64_t>{}(key.value);
            return domainHash ^ (valueHash + 0x9e3779b9U + (domainHash << 6U) + (domainHash >> 2U));
        }
    };

    //-----------------------------------------------------------------------------------------------
    // Scheduler policy and completion metadata.
    //-----------------------------------------------------------------------------------------------
    enum class KeyedTaskPolicy : uint8_t
    {
        AllowDuplicates = 0,
        LatestOnly      = 1,
        CoalescePending = 2
    };

    enum class TaskPolicyDecision : uint8_t
    {
        None                     = 0,
        Executed                 = 1,
        CoalescedToExisting      = 2,
        SupersededBeforeExecution = 3,
        SupersededAfterExecution  = 4
    };

    struct TaskSubmissionOptions
    {
        TaskPriority            priority             = TaskPriority::Normal;
        bool                    supportsCancellation = false;
        std::optional<TaskKey>  taskKey;
        std::optional<uint64_t> version;
        KeyedTaskPolicy         keyedPolicy          = KeyedTaskPolicy::AllowDuplicates;
    };

    struct TaskCompletionRecord
    {
        TaskHandle              handle;
        RunnableTask*           task           = nullptr;
        TaskState               finalState     = TaskState::Completed;
        std::optional<TaskKey>  taskKey;
        std::optional<uint64_t> version;
        bool                    wasCancelled   = false;
        bool                    isStale        = false;
        TaskPolicyDecision      policyDecision = TaskPolicyDecision::Executed;
    };

    struct TaskResultDrainView
    {
        std::vector<TaskCompletionRecord> records;

        bool IsEmpty() const
        {
            return records.empty();
        }

        std::size_t GetRecordCount() const
        {
            return records.size();
        }
    };
}

namespace std
{
    template <>
    struct hash<enigma::core::TaskKey>
    {
        std::size_t operator()(const enigma::core::TaskKey& key) const
        {
            return enigma::core::TaskKeyHasher{}(key);
        }
    };
}
