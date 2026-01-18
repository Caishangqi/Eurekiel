#pragma once

// ============================================================================
// Sampler.hpp - [NEW] RAII Sampler class for Dynamic Sampler System
// Part of Dynamic Sampler System
// ============================================================================

#include "SamplerConfig.hpp"
#include "SamplerProviderCommon.hpp"
#include "Engine/Graphic/Resource/GlobalDescriptorHeapManager.hpp"

namespace enigma::graphic
{
    /**
     * @brief RAII Sampler class managing single D3D12 sampler descriptor
     *
     * Key points:
     * 1. Allocates from GlobalDescriptorHeapManager on construction
     * 2. Frees descriptor on destruction (RAII)
     * 3. Supports move semantics, no copy
     * 4. Provides bindless index for shader access via SamplerDescriptorHeap[index]
     */
    class Sampler final
    {
    public:
        /**
         * @brief Construct sampler with configuration
         * @param heapManager Reference to GlobalDescriptorHeapManager
         * @param config Sampler configuration
         * @throws SamplerHeapAllocationException if allocation fails
         */
        Sampler(GlobalDescriptorHeapManager& heapManager, const SamplerConfig& config);

        /**
         * @brief Destructor - frees sampler descriptor
         */
        ~Sampler();

        // Delete copy operations
        Sampler(const Sampler&)            = delete;
        Sampler& operator=(const Sampler&) = delete;

        // Move operations
        Sampler(Sampler&& other) noexcept;
        Sampler& operator=(Sampler&& other) noexcept;

        // ========== Accessors ==========

        /**
         * @brief Get bindless index for shader access
         * @return Heap index, or INVALID_SAMPLER_INDEX if invalid
         */
        uint32_t GetBindlessIndex() const { return m_bindlessIndex; }

        /**
         * @brief Get current sampler configuration
         */
        const SamplerConfig& GetConfig() const { return m_config; }

        /**
         * @brief Check if sampler is valid
         */
        bool IsValid() const { return m_bindlessIndex != INVALID_SAMPLER_INDEX; }

        /**
         * @brief Update sampler configuration (recreates descriptor)
         * @param config New sampler configuration
         */
        void UpdateConfig(const SamplerConfig& config);

    private:
        /**
         * @brief Create D3D12 sampler descriptor at allocated index
         */
        void CreateSamplerDescriptor();

        /**
         * @brief Release sampler descriptor
         */
        void Release();

        GlobalDescriptorHeapManager*                      m_heapManager = nullptr;
        GlobalDescriptorHeapManager::DescriptorAllocation m_allocation;
        SamplerConfig                                     m_config;
        uint32_t                                          m_bindlessIndex = INVALID_SAMPLER_INDEX;
    };
} // namespace enigma::graphic
