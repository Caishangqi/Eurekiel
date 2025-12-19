#include "UniformCommon.hpp"

// ============================================================================
// UniformCommon.cpp - [NEW] Implementation of common Uniform system types
// ============================================================================

namespace enigma::graphic
{
    // ========================================================================
    // Log Category Definition
    // ========================================================================

    DEFINE_LOG_CATEGORY(LogUniform);

    // ========================================================================
    // PerObjectBufferState Implementation
    // ========================================================================

    void* PerObjectBufferState::GetDataAt(size_t index)
    {
        // [MOVE] Calculate data address at specified index
        return static_cast<uint8_t*>(mappedData) + index * elementSize;
    }

    size_t PerObjectBufferState::GetCurrentIndex() const
    {
        // [MOVE] Return appropriate index based on update frequency
        switch (frequency)
        {
        case UpdateFrequency::PerObject:
            // Ring Buffer: cycle through maxCount slots
            return currentIndex % maxCount;

        case UpdateFrequency::PerPass:
        case UpdateFrequency::PerFrame:
        case UpdateFrequency::Static:
            // Single slot for non-PerObject frequencies
            return 0;

        default:
            return 0;
        }
    }
} // namespace enigma::graphic
