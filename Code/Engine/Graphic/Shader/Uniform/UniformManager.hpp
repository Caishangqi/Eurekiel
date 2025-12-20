#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <typeindex>

#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Resource/Buffer/BufferHelper.hpp"
#include "Engine/Graphic/Resource/Buffer/D12Buffer.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformCommon.hpp"
#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp" // For MAX_DRAWS_PER_FRAME

namespace enigma::graphic
{
    // Forward declaration
    class D12Buffer;

    // [NOTE] PerObjectBufferState is now defined in UniformCommon.hpp

    /**
     * @brief Uniform Manager - SM6.6 Bindless Constant Buffer management
     *
     * Features:
     * - TypeId-based type-safe buffer registration and upload
     * - Ring Buffer for high-frequency updates (PerObject: up to 10000/frame)
     * - Persistent mapping to reduce CPU overhead
     * - Auto-binding by UpdateFrequency
     * - Slot 0-14: Engine reserved (Root CBV), Slot 0-99: User custom (Descriptor Table)
     */
    class UniformManager
    {
    public:
        /**
         * @brief Constructor - RAII auto-initialization
         * @throws DescriptorHeapException if descriptor allocation fails (fatal)
         */
        UniformManager();

        /// @brief Destructor - RAII auto-cleanup
        ~UniformManager();

        /**
         * @brief Get buffer state by slot
         * @param rootSlot Slot number
         * @param space Buffer space (0=Engine, 1=Custom)
         * @return PerObjectBufferState pointer, nullptr if not found
         */
        PerObjectBufferState* GetBufferStateBySlot(uint32_t rootSlot, uint32_t space = 0) const;

        /**
         * @brief Register typed buffer with space parameter
         * @tparam T Buffer data type
         * @param registerSlot Slot number (0-14 engine, 0-99 custom)
         * @param frequency Update frequency
         * @param space BufferSpace::Engine (Root CBV) or BufferSpace::Custom (Descriptor Table)
         * @param maxDraws Max draws for PerObject mode
         * @throws UniformBufferException if validation fails
         * @throws DescriptorHeapException if descriptor allocation fails (fatal)
         */
        // [REFACTORED] maxDraws default changed from 10000 to MAX_DRAWS_PER_FRAME (64)
        // This ensures Ring Buffer index stays within valid range and prevents memory waste
        template <typename T>
        void RegisterBuffer(uint32_t registerSlot, UpdateFrequency frequency, BufferSpace space = BufferSpace::Engine, size_t maxDraws = MAX_DRAWS_PER_FRAME);

        /**
         * @brief Upload data to typed buffer (auto-route by registered space)
         * @tparam T Buffer data type (must match RegisterBuffer)
         * @param data Data to upload
         */
        template <typename T>
        void UploadBuffer(const T& data);

        /**
         * @brief Upload data with explicit slot and space
         * @throws UniformBufferException if space is invalid
         */
        template <typename T>
        void UploadBuffer(const T& data, uint32_t slot, uint32_t space);

        /// @brief Get current draw count for Ring Buffer indexing
        size_t GetCurrentDrawCount() const { return m_currentDrawCount; }

        /// @brief Increment draw count (call after each Draw)
        void IncrementDrawCount()
        {
            m_currentDrawCount++;
            for (auto& [slotId, state] : m_customBufferStates)
            {
                state->currentDrawIndex++;
            }
        }

        /// @brief Reset draw count (call in BeginFrame)
        void ResetDrawCount()
        {
            m_currentDrawCount = 0;
            for (auto& [slotId, state] : m_customBufferStates)
            {
                state->currentDrawIndex = 0;
            }
        }

        // ========================================================================
        // Slot Management API
        // ========================================================================

        /// @brief Get registered slot by TypeId
        template <typename T>
        uint32_t GetRegisteredSlot() const
        {
            auto typeId = std::type_index(typeid(T));
            auto it     = m_typeToSlotMap.find(typeId);
            return (it != m_typeToSlotMap.end()) ? it->second.slot : UINT32_MAX;
        }

        /// @brief Check if slot is registered
        bool IsSlotRegistered(uint32_t slot, uint32_t space = 0) const;

        /// @brief Get all slots with specified update frequency
        const std::vector<uint32_t>& GetSlotsByFrequency(UpdateFrequency frequency) const;

        // ========================================================================
        // Ring Buffer Offset Management
        // ========================================================================

        /// @brief Update Ring Buffer offsets for specified frequency
        void UpdateRingBufferOffsets(UpdateFrequency frequency);

        // ========================================================================
        // Public Accessor Methods
        // ========================================================================

        /// @brief Get Custom Buffer Descriptor Table GPU handle
        D3D12_GPU_DESCRIPTOR_HANDLE GetCustomBufferDescriptorTableGPUHandle(uint32_t ringIndex = 0) const;

        /// @brief Get Engine Buffer GPU virtual address for Root CBV binding
        D3D12_GPU_VIRTUAL_ADDRESS GetEngineBufferGPUAddress(uint32_t slotId) const;

    private:
        // ========================================================================
        // Ring Buffer State Management
        // ========================================================================

        // TypeId -> BufferState mapping
        std::unordered_map<std::type_index, PerObjectBufferState> m_perObjectBuffers;

        // (Slot,Space) -> TypeId mapping for GetBufferStateBySlot()
        std::unordered_map<SlotSpaceKey, std::type_index, SlotSpaceKeyHash> m_slotToTypeMap;

        // TypeId -> (Slot,Space) mapping for UploadBuffer routing
        std::unordered_map<std::type_index, SlotSpaceInfo> m_typeToSlotMap;

        // UpdateFrequency -> Slot list for auto-binding
        std::unordered_map<UpdateFrequency, std::vector<uint32_t>> m_frequencyToSlotsMap;

        // Current frame draw count for Ring Buffer indexing
        size_t m_currentDrawCount = 0;

        // ========================================================================
        // Custom Buffer Descriptor Management
        // ========================================================================

        // SlotId -> Descriptor allocation mapping
        std::unordered_map<uint32_t, CustomBufferDescriptor> m_customBufferDescriptors;

        // Custom Buffer states (space=1)
        std::unordered_map<uint32_t, std::unique_ptr<CustomBufferState>> m_customBufferStates;

        // Pre-allocated Ring Descriptor Pool
        std::vector<GlobalDescriptorHeapManager::DescriptorAllocation> m_customBufferDescriptorPool;

        // Descriptor Table base GPU handle
        D3D12_GPU_DESCRIPTOR_HANDLE m_customBufferDescriptorTableBaseGPUHandle = {};

        // CBV/SRV/UAV Descriptor increment size
        UINT m_cbvSrvUavDescriptorIncrementSize = 0;

        // Initialization flag
        bool m_initialized = false;

        // ========================================================================
        // Internal Buffer Registration
        // ========================================================================

        /// @brief Register Engine Buffer (slot 0-14, Root CBV)
        /// @throws UniformBufferException if validation or creation fails
        bool RegisterEngineBuffer(
            uint32_t        slotId,
            std::type_index typeId,
            size_t          bufferSize,
            UpdateFrequency freq,
            size_t          maxDraws,
            uint32_t        space = 0
        );

        /// @brief Register Custom Buffer (slot 0-99, Descriptor Table, space1)
        /// @throws UniformBufferException if validation or creation fails
        /// @throws DescriptorHeapException if descriptor allocation fails
        bool RegisterCustomBuffer(
            uint32_t        slotId,
            std::type_index typeId,
            size_t          bufferSize,
            UpdateFrequency freq,
            size_t          maxDraws,
            uint32_t        space = 0
        );

        /// @brief Upload to Engine Buffer
        bool UploadEngineBuffer(uint32_t slotId, const void* data, size_t size);

        /// @brief Upload to Custom Buffer
        bool UploadCustomBuffer(uint32_t slotId, const void* data, size_t size);

        // ========================================================================
        // Ring Buffer Offset Update
        // ========================================================================

        /// @brief Update Root CBV offset (placeholder, actual binding in RendererSubsystem)
        void UpdateRootCBVOffset(uint32_t slotId, size_t currentIndex);

        /// @brief Update Descriptor Table CBV offset
        void UpdateDescriptorTableOffset(uint32_t slotId, size_t currentIndex);

        // ========================================================================
        // Descriptor Management Helpers
        // ========================================================================

        /// @brief Allocate descriptor for Custom Buffer
        bool AllocateCustomBufferDescriptor(uint32_t slotId);

        /// @brief Free Custom Buffer descriptor
        void FreeCustomBufferDescriptor(uint32_t slotId);

        /// @brief Get CPU descriptor handle for Custom Buffer
        D3D12_CPU_DESCRIPTOR_HANDLE GetCustomBufferCPUHandle(uint32_t slotId) const;

        // Disable copy (RAII principle)
        UniformManager(const UniformManager&)            = delete;
        UniformManager& operator=(const UniformManager&) = delete;
    };

    // ========================================================================
    // Template Method implementation (must be defined in the header file)
    // ========================================================================
    template <typename T>
    void UniformManager::RegisterBuffer(uint32_t    registerSlot, UpdateFrequency frequency,
                                        BufferSpace space, size_t                 maxDraws)
    {
        // Convert enum to uint32_t for internal use
        uint32_t spaceValue = static_cast<uint32_t>(space);

        try
        {
            std::type_index typeId     = std::type_index(typeid(T));
            size_t          bufferSize = sizeof(T);

            // [NOTE] Duplicate registration is a warning, not an error
            if (m_perObjectBuffers.find(typeId) != m_perObjectBuffers.end())
            {
                LogWarn(LogUniform, "Buffer already registered: %s", typeid(T).name());
                return;
            }

            // [REFACTOR] Route by space parameter - internal methods may throw
            bool success = false;
            if (space == BufferSpace::Engine)
            {
                // Engine Buffer path (space=0, Root CBV)
                success = RegisterEngineBuffer(registerSlot, typeId, bufferSize, frequency, maxDraws, spaceValue);
            }
            else
            {
                // Custom Buffer path (space=1, Descriptor Table)
                success = RegisterCustomBuffer(registerSlot, typeId, bufferSize, frequency, maxDraws, spaceValue);
            }

            if (!success)
            {
                throw UniformBufferException("Internal registration failed", registerSlot, spaceValue);
            }

            // Record TypeId mapping on success (with slot and space info)
            m_typeToSlotMap[typeId] = SlotSpaceInfo{registerSlot, spaceValue};
            m_slotToTypeMap.insert({SlotSpaceKey{registerSlot, spaceValue}, typeId});
        }
        catch (const UniformBufferException& e)
        {
            // [ADD] Recoverable error - log and notify via ERROR_RECOVERABLE
            LogError(LogUniform, "RegisterBuffer failed: %s (slot=%u, space=%u)",
                     e.what(), e.GetSlot(), e.GetSpace());
            ERROR_RECOVERABLE(e.what());
            throw; // Re-throw for caller to handle
        }
        catch (const DescriptorHeapException& e)
        {
            // [ADD] Fatal error - descriptor exhaustion should never happen
            LogError(LogUniform, "RegisterBuffer descriptor allocation failed: %s (slot=%u, space=%u)",
                     e.what(), registerSlot, spaceValue);
            ERROR_AND_DIE(e.what());
        }
        catch (const std::exception& e)
        {
            // [ADD] Unexpected error - treat as fatal
            LogError(LogUniform, "RegisterBuffer unexpected error: %s", e.what());
            ERROR_AND_DIE(e.what());
        }
    }

    template <typename T>
    void UniformManager::UploadBuffer(const T& data)
    {
        std::type_index typeId = std::type_index(typeid(T));

        // 获取已注册的(slot, space)信息
        auto slotIt = m_typeToSlotMap.find(typeId);
        if (slotIt == m_typeToSlotMap.end())
        {
            LogError(LogUniform, "Buffer not registered: %s", typeid(T).name());
            return;
        }

        const SlotSpaceInfo& info = slotIt->second;

        // [FIX] Routing based on the space parameter during registration
        // space=0: Engine Buffer (Root CBV)
        // space=1: Custom Buffer (Descriptor Table)
        if (info.space == 1)
        {
            UploadCustomBuffer(info.slot, &data, sizeof(T));
        }
        else
        {
            UploadEngineBuffer(info.slot, &data, sizeof(T));
        }
    }

    // ========================================================================
    // [ADD] UploadBuffer with explicit slot and space parameter
    // ========================================================================
    template <typename T>
    void UniformManager::UploadBuffer(const T& data, uint32_t slot, uint32_t space)
    {
        // [ADD] Validate space parameter
        if (space != 0 && space != 1)
        {
            LogError(LogUniform, "Invalid space parameter: %u (must be 0 or 1)", space);
            throw UniformBufferException("Invalid space parameter", slot, space);
        }

        // [ADD] Route to appropriate upload method based on space
        if (space == 0)
        {
            // Engine Buffer path (space=0, Root CBV)
            UploadEngineBuffer(slot, &data, sizeof(T));
        }
        else
        {
            // Custom Buffer path (space=1, Descriptor Table)
            UploadCustomBuffer(slot, &data, sizeof(T));
        }
    }
} // namespace enigma::graphic
