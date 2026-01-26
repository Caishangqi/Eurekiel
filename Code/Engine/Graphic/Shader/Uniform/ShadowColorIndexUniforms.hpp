#pragma once

// ============================================================================
// ShadowColorIndexUniforms.hpp - [REFACTOR] Shadow color texture index management
// Part of Shader RT Fetching Feature for Flexible Deferred Rendering
// ============================================================================

#include <cstdint>
#include "Engine/Graphic/Target/RenderTargetProviderCommon.hpp"

namespace enigma::graphic
{
    /**
     * @brief ShadowColorIndexBuffer - Main/Alt double-buffer index management for shadowcolor0-7
     *
     * Key points:
     * 1. Manages shadowcolor0-7 Main/Alt double-buffer indices
     * 2. Supports Ping-Pong Flip mechanism (eliminates ResourceBarrier overhead)
     * 3. Separated from ShadowTexturesIndexBuffer (single responsibility)
     * 4. Must match HLSL ShadowColorIndexUniforms struct (64 bytes)
     *
     * Flip state behavior:
     * - flip = false: Main as read source, Alt as write target
     * - flip = true:  Alt as read source, Main as write target
     *
     * @note Size must be exactly 64 bytes to match HLSL cbuffer
     */
    struct ShadowColorIndexUniforms
    {
        // Read indices for shadowcolor0-7 (points to Main or Alt based on flip state)
        uint32_t readIndices[CBUFFER_SHADOW_COLORS_SIZE];

        // Write indices for shadowcolor0-7 (reserved for UAV extension)
        uint32_t writeIndices[CBUFFER_SHADOW_COLORS_SIZE];

        // ===== Total: (8 + 8) * 4 = 64 bytes =====

        /**
         * @brief Default constructor - initializes all indices to INVALID_BINDLESS_INDEX
         */
        ShadowColorIndexUniforms()
        {
            Reset();
        }

        // ========== [NEW] Unified API ==========

        /**
         * @brief Set single read index (unified API)
         * @param slot Slot index (0-7)
         * @param bindlessIndex Bindless texture index
         */
        void SetIndex(uint32_t slot, uint32_t bindlessIndex)
        {
            if (slot < CBUFFER_SHADOW_COLORS_SIZE)
            {
                readIndices[slot] = bindlessIndex;
            }
        }

        /**
         * @brief Get single read index (unified API)
         * @param slot Slot index (0-7)
         * @return Bindless index, or INVALID_BINDLESS_INDEX if out of range
         */
        uint32_t GetIndex(uint32_t slot) const
        {
            return (slot < CBUFFER_SHADOW_COLORS_SIZE) ? readIndices[slot] : INVALID_BINDLESS_INDEX;
        }

        /**
         * @brief Check if buffer has any valid indices
         * @return true if at least one index is valid
         */
        bool IsValid() const
        {
            for (uint32_t i = 0; i < CBUFFER_SHADOW_COLORS_SIZE; ++i)
            {
                if (readIndices[i] != INVALID_BINDLESS_INDEX)
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
            for (uint32_t i = 0; i < CBUFFER_SHADOW_COLORS_SIZE; ++i)
            {
                readIndices[i]  = INVALID_BINDLESS_INDEX;
                writeIndices[i] = INVALID_BINDLESS_INDEX;
            }
        }

        // ========== Batch Operations ==========

        /**
         * @brief Set all read indices (batch operation)
         * @param indices Array of indices (must have CBUFFER_SHADOW_COLORS_SIZE elements)
         */
        void SetReadIndices(const uint32_t indices[CBUFFER_SHADOW_COLORS_SIZE])
        {
            for (uint32_t i = 0; i < CBUFFER_SHADOW_COLORS_SIZE; ++i)
            {
                readIndices[i] = indices[i];
            }
        }

        /**
         * @brief Set all write indices (batch operation)
         * @param indices Array of indices (must have CBUFFER_SHADOW_COLORS_SIZE elements)
         */
        void SetWriteIndices(const uint32_t indices[CBUFFER_SHADOW_COLORS_SIZE])
        {
            for (uint32_t i = 0; i < CBUFFER_SHADOW_COLORS_SIZE; ++i)
            {
                writeIndices[i] = indices[i];
            }
        }

        /**
         * @brief Flip operation - swap Main and Alt indices
         * @param mainIndices Main texture index array
         * @param altIndices Alt texture index array
         * @param useAlt true=read Alt write Main, false=read Main write Alt
         */
        void Flip(const uint32_t mainIndices[CBUFFER_SHADOW_COLORS_SIZE],
                  const uint32_t altIndices[CBUFFER_SHADOW_COLORS_SIZE],
                  bool           useAlt)
        {
            if (useAlt)
            {
                // Read Alt, Write Main
                SetReadIndices(altIndices);
                SetWriteIndices(mainIndices);
            }
            else
            {
                // Read Main, Write Alt
                SetReadIndices(mainIndices);
                SetWriteIndices(altIndices);
            }
        }

        // ========== Utility Methods ==========

        /**
         * @brief Get count of active (valid) shadow colors
         * @return Number of valid indices
         */
        uint32_t GetActiveCount() const
        {
            uint32_t count = 0;
            for (uint32_t i = 0; i < CBUFFER_SHADOW_COLORS_SIZE; ++i)
            {
                if (readIndices[i] != INVALID_BINDLESS_INDEX)
                {
                    ++count;
                }
            }
            return count;
        }
    };

    // Compile-time validation: ensure struct size is 64 bytes
    static_assert(sizeof(ShadowColorIndexUniforms) == 64,
                  "ShadowColorIndexUniforms must be exactly 64 bytes to match HLSL cbuffer");

    // Compile-time validation: ensure proper alignment
    static_assert(alignof(ShadowColorIndexUniforms) == 4,
                  "ShadowColorIndexUniforms must be 4-byte aligned for GPU upload");
} // namespace enigma::graphic
