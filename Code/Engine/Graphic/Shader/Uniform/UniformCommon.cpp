#include "UniformCommon.hpp"
#include "Engine/Graphic/Resource/Buffer/D12Buffer.hpp"

// ============================================================================
// UniformCommon.cpp - [REFACTORED] Implementation of unified UniformBufferState
// ============================================================================

namespace enigma::graphic
{
    // ========================================================================
    // Log Category Definition
    // ========================================================================

    DEFINE_LOG_CATEGORY(LogUniform);

    // ========================================================================
    // [REFACTORED] UniformBufferState Implementation
    // ========================================================================

    void* UniformBufferState::GetMappedData() const
    {
        if (!buffer)
        {
            return nullptr;
        }
        return buffer->GetPersistentMappedData();
    }

    uint32_t UniformBufferState::GetBindlessIndex() const
    {
        if (!buffer)
        {
            return UINT32_MAX;
        }
        return buffer->GetBindlessIndex();
    }

    D3D12_GPU_VIRTUAL_ADDRESS UniformBufferState::GetGPUVirtualAddress() const
    {
        if (!buffer)
        {
            return 0;
        }
        return buffer->GetGPUVirtualAddress();
    }

    void* UniformBufferState::GetDataAt(size_t index) const
    {
        void* mappedData = GetMappedData();
        if (!mappedData)
        {
            return nullptr;
        }
        return static_cast<uint8_t*>(mappedData) + index * elementSize;
    }

    size_t UniformBufferState::GetCurrentRingIndex() const
    {
        // Return appropriate index based on update frequency
        switch (frequency)
        {
        case UpdateFrequency::PerObject:
            // Ring Buffer: cycle through maxCount slots
            return (maxCount > 0) ? (ringIndex % maxCount) : 0;

        case UpdateFrequency::PerPass:
        case UpdateFrequency::PerFrame:
        case UpdateFrequency::Static:
            // Single slot for non-PerObject frequencies
            return 0;

        default:
            return 0;
        }
    }

    bool UniformBufferState::IsValid() const
    {
        return buffer && buffer->GetPersistentMappedData() != nullptr;
    }
} // namespace enigma::graphic
