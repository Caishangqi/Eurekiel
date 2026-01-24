#pragma once

// ============================================================================
// SamplerProvider.hpp - [NEW] Sampler provider for Dynamic Sampler System
// Manages multiple Sampler objects and registers SamplerIndicesUniforms
// ============================================================================

#include "Sampler.hpp"
#include "SamplerConfig.hpp"
#include "SamplerProviderCommon.hpp"
#include "SamplerProviderException.hpp"
#include "Engine/Graphic/Shader/Uniform/SamplerIndicesUniforms.hpp"

#include <vector>
#include <memory>

namespace enigma::graphic
{
    // Forward declarations
    class UniformManager;
    class GlobalDescriptorHeapManager;

    /**
     * @brief SamplerProvider - Manages multiple bindless samplers
     *
     * Key points:
     * 1. Creates and manages Sampler objects from SamplerConfig vector
     * 2. Registers SamplerIndicesBuffer with UniformManager at slot b7
     * 3. Provides SetSamplerConfig API for runtime sampler updates
     * 4. RAII pattern - samplers created in constructor
     *
     * Usage:
     * - sampler0: Linear (default texture sampling)
     * - sampler1: Point (pixel-perfect sampling)
     * - sampler2: Shadow (depth comparison)
     * - sampler3: PointWrap (tiled textures)
     */
    class SamplerProvider final
    {
    public:
        // ========================================================================
        // Constructor / Destructor
        // ========================================================================

        /**
         * @brief RAII constructor - creates samplers from config vector
         * @param heapManager GlobalDescriptorHeapManager for sampler allocation
         * @param configs Vector of SamplerConfig for each sampler slot
         * @param uniformMgr UniformManager for index buffer registration
         * @throws std::invalid_argument if configs empty or uniformMgr null
         * @throws SamplerHeapAllocationException if sampler allocation fails
         */
        SamplerProvider(
            GlobalDescriptorHeapManager&      heapManager,
            const std::vector<SamplerConfig>& configs,
            UniformManager*                   uniformMgr
        );

        ~SamplerProvider() = default;

        // Non-copyable, movable
        SamplerProvider(const SamplerProvider&)            = delete;
        SamplerProvider& operator=(const SamplerProvider&) = delete;
        SamplerProvider(SamplerProvider&&)                 = default;
        SamplerProvider& operator=(SamplerProvider&&)      = default;

        // ========================================================================
        // Sampler Access API
        // ========================================================================

        /**
         * @brief Update sampler configuration at index
         * @param index Sampler slot (0-15)
         * @param config New sampler configuration
         * @throws InvalidSamplerIndexException if index out of range
         */
        void SetSamplerConfig(uint32_t index, const SamplerConfig& config);

        /**
         * @brief Get sampler configuration at index
         * @param index Sampler slot (0-15)
         * @return SamplerConfig reference
         * @throws InvalidSamplerIndexException if index out of range
         */
        const SamplerConfig& GetSamplerConfig(uint32_t index) const;

        /**
         * @brief Get bindless index for sampler at slot
         * @param index Sampler slot (0-15)
         * @return Bindless heap index, or INVALID_SAMPLER_INDEX if invalid
         */
        uint32_t GetBindlessIndex(uint32_t index) const;

        /**
         * @brief Get number of active samplers
         */
        uint32_t GetCount() const { return m_activeCount; }

        // ========================================================================
        // Uniform Update API
        // ========================================================================

        /**
         * @brief Update and upload sampler indices to GPU
         * @note Call after SetSamplerConfig or sampler recreation
         */
        void UpdateIndices();

    private:
        // ========================================================================
        // Private Methods
        // ========================================================================

        /**
         * @brief Register index buffer to UniformManager
         * @param uniformMgr UniformManager pointer
         */
        void RegisterUniform(UniformManager* uniformMgr);

        /**
         * @brief Validate index and throw if out of range
         */
        void ValidateIndex(uint32_t index) const;

        /**
         * @brief Check if index is valid (no throw)
         */
        bool IsValidIndex(uint32_t index) const;

        // ========================================================================
        // Private Members
        // ========================================================================

        GlobalDescriptorHeapManager*          m_heapManager = nullptr;
        std::vector<std::unique_ptr<Sampler>> m_samplers;
        std::vector<SamplerConfig>            m_configs;

        uint32_t m_activeCount = 0;

        // Uniform registration
        UniformManager*        m_uniformManager = nullptr;
        SamplerIndicesUniforms m_indexBuffer;
    };
} // namespace enigma::graphic
