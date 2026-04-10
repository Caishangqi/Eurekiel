#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace enigma::core
{
    constexpr uint64_t INVALID_TASK_ID         = 0ULL;
    constexpr uint32_t INVALID_TASK_GENERATION = 0U;

    //-----------------------------------------------------------------------------------------------
    // TaskHandle
    // Opaque value type used by callers to identify and control a scheduled task.
    //-----------------------------------------------------------------------------------------------
    struct TaskHandle
    {
        uint64_t id         = INVALID_TASK_ID;
        uint32_t generation = INVALID_TASK_GENERATION;

        bool IsValid() const noexcept
        {
            return id != INVALID_TASK_ID;
        }
    };

    inline bool operator==(const TaskHandle& lhs, const TaskHandle& rhs) noexcept
    {
        return lhs.id == rhs.id && lhs.generation == rhs.generation;
    }

    inline bool operator!=(const TaskHandle& lhs, const TaskHandle& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    struct TaskHandleHasher
    {
        std::size_t operator()(const TaskHandle& handle) const noexcept
        {
            const std::size_t idHash         = std::hash<uint64_t>{}(handle.id);
            const std::size_t generationHash = std::hash<uint32_t>{}(handle.generation);
            return idHash ^ (generationHash + 0x9e3779b9U + (idHash << 6U) + (idHash >> 2U));
        }
    };
}

namespace std
{
    template <>
    struct hash<enigma::core::TaskHandle>
    {
        std::size_t operator()(const enigma::core::TaskHandle& handle) const noexcept
        {
            return enigma::core::TaskHandleHasher{}(handle);
        }
    };
}
