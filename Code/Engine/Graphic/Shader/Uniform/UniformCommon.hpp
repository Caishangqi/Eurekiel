#pragma once

// ============================================================================
// UniformCommon.hpp - [NEW] Consolidated type definitions for Uniform system
// 
// This module serves as the foundation for the entire Uniform system, providing
// common types, enums, structs, and exception hierarchy.
// ============================================================================

#include <cstdint>
#include <memory>
#include <vector>
#include <stdexcept>
#include <string>
#include <d3d12.h>
#include <wrl/client.h>

#include "Engine/Core/LogCategory/LogCategory.hpp"
#include "Engine/Graphic/Resource/GlobalDescriptorHeapManager.hpp"

namespace enigma::graphic
{
    // Forward declaration
    class D12Buffer;

    // ========================================================================
    // Log Category Declaration
    // ========================================================================

    DECLARE_LOG_CATEGORY_EXTERN(LogUniform);

    // ========================================================================
    // [NEW] BufferSpace Enum - Space parameter routing
    // ========================================================================

    /**
     * @brief Buffer space routing for Engine vs Custom buffers
     * 
     * [NEW] Introduced for space parameter routing architecture:
     * - Engine buffers use space=0 with Root CBV (slots 0-14)
     * - Custom buffers use space=1 with Descriptor Table (unlimited slots)
     */
    enum class BufferSpace : uint32_t
    {
        Engine = 0, // [NEW] space=0, uses Root CBV, slots 0-14
        Custom = 1 // [NEW] space=1, uses Descriptor Table, unlimited slots
    };

    // ========================================================================
    // [MOVE] UpdateFrequency Enum - from UpdateFrequency.hpp
    // ========================================================================

    /**
     * @brief Buffer update frequency enumeration
     *
     * [MOVE] Consolidated from UpdateFrequency.hpp
     * 
     * Usage:
     * - PerObject: Updated once per Draw call (high frequency, 10,000/frame)
     * - PerPass:   Updated once per render pass (medium frequency, 24/frame)
     * - PerFrame:  Updated once per frame (low frequency, 60/sec)
     * - Static:    Set once and never updated (very low frequency)
     *
     * Purpose:
     * - Determines Ring Buffer size (PerObject needs many copies)
     * - Optimizes memory and performance (Static=1 copy, PerObject=10,000 copies)
     */
    enum class UpdateFrequency : uint8_t
    {
        PerObject = 0, // Updated once per Draw call (high frequency)
        PerPass = 1, // Updated once per render pass (medium frequency)
        PerFrame = 2, // Updated once per frame (low frequency)
        Static = 3 // Set once and never updated (very low frequency)
    };

    // ========================================================================
    // [MOVE] PerObjectBufferState - from UniformManager.hpp line 32
    // ========================================================================

    /**
     * @brief Ring Buffer management state
     *
     * [MOVE] Consolidated from UniformManager.hpp
     * 
     * Key features:
     * - 256-byte alignment: elementSize aligned per D3D12 requirements
     * - Persistent mapping: mappedData pointer remains valid, avoids Map/Unmap overhead
     * - Delayed Fill: lastUpdatedValue caches value, detects duplicate uploads
     * - Ring Buffer: currentIndex cycles through maxCount slots
     */
    struct PerObjectBufferState
    {
        std::unique_ptr<D12Buffer> gpuBuffer; // GPU Buffer resource
        void*                      mappedData  = nullptr; // Persistent mapped pointer
        size_t                     elementSize = 0; // 256-byte aligned element size
        size_t                     maxCount    = 0; // Maximum element count
        UpdateFrequency            frequency; // Update frequency
        size_t                     currentIndex = 0; // Current write index
        std::vector<uint8_t>       lastUpdatedValue; // Cached last updated value
        size_t                     lastUpdatedIndex = SIZE_MAX; // Last updated index

        /**
         * @brief Get data address at specified index
         * @param index Index value
         * @return void* pointer to data
         */
        void* GetDataAt(size_t index);

        /**
         * @brief Get current index to use
         * @return For PerObject frequency returns currentIndex % maxCount, others return 0
         */
        size_t GetCurrentIndex() const;
    };

    // ========================================================================
    // [MOVE] CustomBufferDescriptor - from UniformManager.hpp line 293
    // ========================================================================

    /**
     * @brief Custom Buffer Descriptor allocation record
     *
     * [MOVE] Consolidated from UniformManager.hpp
     * 
     * Purpose:
     * 1. Records CBV Descriptor allocation info for each Custom Buffer
     * 2. slotId range: 15-114 (corresponds to b15-b114)
     * 3. allocation contains CPU and GPU handles for CBV creation and binding
     */
    struct CustomBufferDescriptor
    {
        GlobalDescriptorHeapManager::DescriptorAllocation allocation; // Descriptor allocation info
        uint32_t                                          slotId; // Slot number (15-114)
        bool                                              isValid; // Validity flag

        CustomBufferDescriptor()
            : slotId(0)
              , isValid(false)
        {
        }
    };

    // ========================================================================
    // [NEW] CustomBufferState - Ring Buffer state for Custom buffers
    // ========================================================================

    /**
     * @brief Custom Buffer state for space=1 buffers using Descriptor Table
     *
     * [NEW] Introduced for Custom Buffer management with Ring Buffer mechanism
     * 
     * Key features:
     * - Ring Buffer for per-draw data independence
     * - Bindless integration via bindlessIndex
     * - Descriptor Table binding via cpuHandle
     */
    struct CustomBufferState
    {
        size_t                                 elementSize      = 0; // Buffer element size
        size_t                                 maxDraws         = 0; // Maximum draws for Ring Buffer
        uint32_t                               currentDrawIndex = 0; // Current draw index
        Microsoft::WRL::ComPtr<ID3D12Resource> gpuBuffer; // GPU Buffer resource
        uint8_t*                               mappedPtr     = nullptr; // Persistent mapped pointer
        uint32_t                               bindlessIndex = 0; // Bindless resource index
        D3D12_CPU_DESCRIPTOR_HANDLE            cpuHandle     = {}; // CPU Descriptor handle
        uint32_t                               slot          = 0; // Slot number
        UpdateFrequency                        frequency     = UpdateFrequency::PerObject; // Update frequency
        bool                                   isDirty       = false; // Dirty flag for updates
    };

    // ========================================================================
    // [NEW] Exception Hierarchy - for type-safe error handling
    // ========================================================================

    /**
     * @brief Base exception class for Uniform system
     *
     * [NEW] Root of Uniform exception hierarchy
     * Inherits from std::runtime_error for standard library compatibility
     */
    class UniformException : public std::runtime_error
    {
    public:
        explicit UniformException(const std::string& message)
            : std::runtime_error(message)
        {
        }
    };

    /**
     * @brief Exception for buffer-related errors
     *
     * [NEW] Provides slot and space context for debugging
     */
    class UniformBufferException : public UniformException
    {
    public:
        UniformBufferException(const std::string& message, uint32_t slot, uint32_t space)
            : UniformException(message)
              , m_slot(slot)
              , m_space(space)
        {
        }

        uint32_t GetSlot() const { return m_slot; }
        uint32_t GetSpace() const { return m_space; }

    private:
        uint32_t m_slot;
        uint32_t m_space;
    };

    /**
     * @brief Exception for descriptor heap-related errors
     *
     * [NEW] For descriptor allocation and management failures
     */
    class DescriptorHeapException : public UniformException
    {
    public:
        explicit DescriptorHeapException(const std::string& message)
            : UniformException(message)
        {
        }
    };
} // namespace enigma::graphic
