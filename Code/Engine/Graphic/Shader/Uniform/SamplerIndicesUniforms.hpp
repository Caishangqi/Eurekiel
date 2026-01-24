#pragma once

// ============================================================================
// SamplerIndicesUniforms.hpp - [NEW] Sampler bindless indices uniform buffer
// Part of Dynamic Sampler System
// ============================================================================

#include <cstdint>
#include "Engine/Graphic/Sampler/SamplerProviderCommon.hpp"

namespace enigma::graphic
{
    /**
     * @brief SamplerIndicesBuffer - Bindless sampler indices for shader access
     *
     * Key points:
     * 1. Stores bindless indices for 16 sampler slots (sampler0-15)
     * 2. Uploaded to GPU cbuffer at register(b7) before each Pass
     * 3. HLSL accesses via SamplerDescriptorHeap[samplerIndices[n]]
     * 4. Must match HLSL SamplerIndicesBuffer struct (64 bytes)
     *
     * @note Size must be exactly 64 bytes to match HLSL cbuffer
     */
    struct SamplerIndicesUniforms
    {
        // Bindless indices for sampler0-15
        uint32_t samplerIndices[MAX_SAMPLERS];

        // ===== Total: 16 * 4 = 64 bytes =====

        /**
         * @brief Default constructor - initializes all indices to INVALID_SAMPLER_INDEX
         */
        SamplerIndicesUniforms()
        {
            Reset();
        }

        // ========== Unified API ==========

        /**
         * @brief Set single sampler index
         * @param slot Slot index (0-15)
         * @param bindlessIndex Bindless sampler heap index
         */
        void SetIndex(uint32_t slot, uint32_t bindlessIndex)
        {
            if (slot < MAX_SAMPLERS)
            {
                samplerIndices[slot] = bindlessIndex;
            }
        }

        /**
         * @brief Get single sampler index
         * @param slot Slot index (0-15)
         * @return Bindless index, or INVALID_SAMPLER_INDEX if out of range
         */
        uint32_t GetIndex(uint32_t slot) const
        {
            return (slot < MAX_SAMPLERS) ? samplerIndices[slot] : INVALID_SAMPLER_INDEX;
        }

        /**
         * @brief Check if buffer has any valid indices
         * @return true if at least one index is valid
         */
        bool IsValid() const
        {
            for (uint32_t i = 0; i < MAX_SAMPLERS; ++i)
            {
                if (samplerIndices[i] != INVALID_SAMPLER_INDEX)
                {
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief Reset all indices to invalid state
         */
        void Reset()
        {
            for (uint32_t i = 0; i < MAX_SAMPLERS; ++i)
            {
                samplerIndices[i] = INVALID_SAMPLER_INDEX;
            }
        }

        // ========== Batch Operations ==========

        /**
         * @brief Set all sampler indices (batch operation)
         * @param indices Array of indices (must have MAX_SAMPLERS elements)
         */
        void SetAllIndices(const uint32_t indices[MAX_SAMPLERS])
        {
            for (uint32_t i = 0; i < MAX_SAMPLERS; ++i)
            {
                samplerIndices[i] = indices[i];
            }
        }
    };

    // Compile-time validation: ensure struct size is 64 bytes
    static_assert(sizeof(SamplerIndicesUniforms) == 64,
                  "SamplerIndicesUniforms must be exactly 64 bytes to match HLSL cbuffer");

    // Compile-time validation: ensure proper alignment
    static_assert(alignof(SamplerIndicesUniforms) == 4,
                  "SamplerIndicesUniforms must be 4-byte aligned for GPU upload");
} // namespace enigma::graphic
