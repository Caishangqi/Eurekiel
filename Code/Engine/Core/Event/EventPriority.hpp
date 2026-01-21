#pragma once

// ============================================================================
// EventPriority.hpp - Event listener priority enumeration
// Part of Event System
// ============================================================================

#include <cstdint>

namespace enigma::event
{
    /// Event listener priority
    /// Listeners are called in order: HIGHEST -> HIGH -> NORMAL -> LOW -> LOWEST
    enum class EventPriority : uint8_t
    {
        Highest = 0, // First to execute
        High = 1,
        Normal = 2, // Default priority
        Low = 3,
        Lowest = 4 // Last to execute
    };
} // namespace enigma::event
