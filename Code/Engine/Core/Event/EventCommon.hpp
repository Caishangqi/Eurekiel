#pragma once

// ============================================================================
// EventCommon.hpp - Common definitions for Event module
// Part of Event System
// ============================================================================

#include "Engine/Core/LogCategory/LogCategory.hpp"
#include <cstdint>

// ========================================================================
// Log Category Declaration (must be in global namespace)
// ========================================================================
DECLARE_LOG_CATEGORY_EXTERN(LogEvent)

namespace enigma::event
{
    // ========================================================================
    // Event System Constants
    // ========================================================================

    // Event priority count
    constexpr uint32_t EVENT_PRIORITY_COUNT = 5;

    // Default listener capacity for EventBus
    constexpr uint32_t DEFAULT_LISTENER_CAPACITY = 16;

    // Invalid listener handle marker
    constexpr uint64_t INVALID_LISTENER_HANDLE = 0;

    // Maximum event recursion depth (prevent infinite loops)
    constexpr uint32_t MAX_EVENT_RECURSION_DEPTH = 16;
} // namespace enigma::event
