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

    bool UniformBufferState::IsValid() const
    {
        return buffer && buffer->GetPersistentMappedData() != nullptr;
    }
} // namespace enigma::graphic
