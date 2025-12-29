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
    // [NEW] SlotSpaceInfo - Slot and Space binding information
    // ========================================================================

    /**
     * @brief Stores slot and space information for type-safe buffer routing
     * 
     * Used by UniformManager to route UploadBuffer calls to the correct path:
     * - space=0: UploadEngineBuffer (Root CBV)
     * - space=1: UploadCustomBuffer (Descriptor Table)
     */
    struct SlotSpaceInfo
    {
        uint32_t slot  = UINT32_MAX; // HLSL register slot
        uint32_t space = 0; // 0=Engine Root CBV, 1=Custom Descriptor Table
    };

    // ========================================================================
    // [NEW] SlotSpaceKey - Hash key for (slot, space) combination
    // ========================================================================

    /**
     * @brief Hash key for unordered_map using (slot, space) as composite key
     * 
     * Allows same slot with different space to coexist:
     * - register(b1, space0) and register(b1, space1) are different bindings
     */
    struct SlotSpaceKey
    {
        uint32_t slot  = UINT32_MAX;
        uint32_t space = 0;

        bool operator==(const SlotSpaceKey& other) const
        {
            return slot == other.slot && space == other.space;
        }
    };

    // Hash function for SlotSpaceKey
    struct SlotSpaceKeyHash
    {
        std::size_t operator()(const SlotSpaceKey& key) const
        {
            // Combine slot and space into a single hash
            return std::hash<uint64_t>{}((static_cast<uint64_t>(key.slot) << 32) | key.space);
        }
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
    // [REFACTORED] UniformBufferState - Unified Ring Buffer state
    // ========================================================================

    /**
     * @brief Unified Ring Buffer state for all Uniform buffers
     *
     * [REFACTORED] Merged PerObjectBufferState and CustomBufferState into single struct
     * 
     * Key design decisions:
     * - Uses D12Buffer exclusively (no duplicate resource management)
     * - Unified naming: ringIndex (was currentIndex/currentDrawIndex)
     * - Unified naming: maxCount (was maxCount/maxDraws)
     * - D12Buffer already provides: mappedData, bindlessIndex, gpuResource
     * 
     * Ring Buffer mechanism:
     * - 256-byte alignment: elementSize aligned per D3D12 requirements
     * - Persistent mapping: D12Buffer::GetPersistentMappedData()
     * - ringIndex cycles through maxCount slots for PerObject frequency
     * 
     * Binding modes (determined by BufferSpace during registration):
     * - Engine (space=0): Root CBV, uses GetGPUVirtualAddress()
     * - Custom (space=1): Descriptor Table, uses GetBindlessIndex()
     */
    struct UniformBufferState
    {
        // ==================== Core Buffer Resource ====================
        std::unique_ptr<D12Buffer> buffer; // GPU Buffer (provides resource, mapping, bindless)

        // ==================== Ring Buffer Parameters ====================
        size_t          elementSize = 0; // 256-byte aligned element size
        size_t          maxCount    = 0; // Maximum element count in Ring Buffer
        size_t          ringIndex   = 0; // Current write index (unified naming)
        UpdateFrequency frequency   = UpdateFrequency::PerObject;

        // ==================== Routing Information ====================
        uint32_t    slot  = UINT32_MAX; // HLSL register slot (b0-b14 or b15+)
        BufferSpace space = BufferSpace::Engine; // Engine=Root CBV, Custom=Descriptor Table

        // ==================== Optimization: Delayed Fill ====================
        std::vector<uint8_t> lastUpdatedValue; // Cached last value (skip duplicate uploads)
        size_t               lastUpdatedIndex = SIZE_MAX; // Last updated ring index

        // ==================== Convenience Accessors ====================

        /**
         * @brief Get persistent mapped data pointer from D12Buffer
         * @return Mapped pointer, nullptr if not mapped
         */
        void* GetMappedData() const;

        /**
         * @brief Get bindless index from D12Buffer (for Custom buffers)
         * @return Bindless index, UINT32_MAX if not registered
         */
        uint32_t GetBindlessIndex() const;

        /**
         * @brief Get GPU virtual address from D12Buffer (for Engine buffers)
         * @return GPU virtual address
         */
        D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const;

        /**
         * @brief Get data address at specified ring index
         * @param index Ring buffer index
         * @return void* pointer to data at index
         */
        void* GetDataAt(size_t index) const;

        /**
         * @brief Get current ring index to use (with modulo for PerObject)
         * @return For PerObject: ringIndex % maxCount, others: 0
         */
        size_t GetCurrentRingIndex() const;

        /**
         * @brief Check if buffer is valid and ready for use
         * @return true if buffer exists and is mapped
         */
        bool IsValid() const;
    };

    // ==================== Legacy Compatibility Aliases ====================
    // [DEPRECATED] Use UniformBufferState instead
    using PerObjectBufferState = UniformBufferState;

    // ========================================================================
    // [KEEP] CustomBufferDescriptor - Descriptor allocation record
    // ========================================================================

    /**
     * @brief Custom Buffer Descriptor allocation record
     *
     * [KEEP] This is for descriptor heap allocation tracking, not buffer state
     * 
     * Purpose:
     * 1. Records CBV Descriptor allocation info for Custom Buffer slots
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
