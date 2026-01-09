#pragma once

// ============================================================================
// ColorTargetsIndexBuffer.hpp - [REFACTOR] Main/Alt double-buffer index management
// Part of Shader RT Fetching Feature for Flexible Deferred Rendering
// ============================================================================

#include <cstdint>
#include "Engine/Graphic/Target/RenderTargetProviderCommon.hpp"

namespace enigma::graphic
{
    /**
     * @brief ColorTargetsIndexBuffer - Main/Alt double-buffer index management for colortex0-15
     *
     * Key points:
     * 1. Stores read/write indices for 16 colortex slots (Iris colortex0-15)
     * 2. Main/Alt Ping-Pong mechanism: eliminates ResourceBarrier overhead
     * 3. Uploaded to GPU cbuffer before each Pass execution
     * 4. Must match HLSL ColorTargetsBuffer struct (128 bytes)
     *
     * Flip state behavior:
     * - flip = false: Main as read source, Alt as write target
     * - flip = true:  Alt as read source, Main as write target
     *
     * @note Size must be exactly 128 bytes to match HLSL cbuffer
     */
    struct ColorTargetsIndexBuffer
    {
        // Read indices for colortex0-15 (points to Main or Alt based on flip state)
        uint32_t readIndices[CBUFFER_COLOR_TARGETS_SIZE];

        // Write indices for colortex0-15 (reserved for UAV extension)
        uint32_t writeIndices[CBUFFER_COLOR_TARGETS_SIZE];

        // ===== Total: (16 + 16) * 4 = 128 bytes =====

        /**
         * @brief Default constructor - initializes all indices to INVALID_BINDLESS_INDEX
         */
        ColorTargetsIndexBuffer()
        {
            Reset();
        }

        // ========== [NEW] Unified API ==========

        /**
         * @brief Set single read index (unified API)
         * @param slot Slot index (0-15)
         * @param bindlessIndex Bindless texture index
         */
        void SetIndex(uint32_t slot, uint32_t bindlessIndex)
        {
            if (slot < CBUFFER_COLOR_TARGETS_SIZE)
            {
                readIndices[slot] = bindlessIndex;
            }
        }

        /**
         * @brief Get single read index (unified API)
         * @param slot Slot index (0-15)
         * @return Bindless index, or INVALID_BINDLESS_INDEX if out of range
         */
        uint32_t GetIndex(uint32_t slot) const
        {
            return (slot < CBUFFER_COLOR_TARGETS_SIZE) ? readIndices[slot] : INVALID_BINDLESS_INDEX;
        }

        /**
         * @brief Check if buffer has any valid indices
         * @return true if at least one index is valid
         */
        bool IsValid() const
        {
            for (uint32_t i = 0; i < CBUFFER_COLOR_TARGETS_SIZE; ++i)
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
            for (uint32_t i = 0; i < CBUFFER_COLOR_TARGETS_SIZE; ++i)
            {
                readIndices[i]  = INVALID_BINDLESS_INDEX;
                writeIndices[i] = INVALID_BINDLESS_INDEX;
            }
        }

        // ========== Batch Operations ==========

        /**
         * @brief Set all read indices (batch operation)
         * @param indices Array of indices (must have CBUFFER_COLOR_TARGETS_SIZE elements)
         */
        void SetReadIndices(const uint32_t indices[CBUFFER_COLOR_TARGETS_SIZE])
        {
            for (uint32_t i = 0; i < CBUFFER_COLOR_TARGETS_SIZE; ++i)
            {
                readIndices[i] = indices[i];
            }
        }

        /**
         * @brief Set all write indices (batch operation)
         * @param indices Array of indices (must have CBUFFER_COLOR_TARGETS_SIZE elements)
         */
        void SetWriteIndices(const uint32_t indices[CBUFFER_COLOR_TARGETS_SIZE])
        {
            for (uint32_t i = 0; i < CBUFFER_COLOR_TARGETS_SIZE; ++i)
            {
                writeIndices[i] = indices[i];
            }
        }

        /**
         * @brief Set single read index
         * @param rtIndex RenderTarget index (0-15)
         * @param textureIndex Bindless texture index
         */
        void SetReadIndex(uint32_t rtIndex, uint32_t textureIndex)
        {
            if (rtIndex < CBUFFER_COLOR_TARGETS_SIZE)
            {
                readIndices[rtIndex] = textureIndex;
            }
        }

        /**
         * @brief Set single write index
         * @param rtIndex RenderTarget index (0-15)
         * @param textureIndex Bindless texture index
         */
        void SetWriteIndex(uint32_t rtIndex, uint32_t textureIndex)
        {
            if (rtIndex < CBUFFER_COLOR_TARGETS_SIZE)
            {
                writeIndices[rtIndex] = textureIndex;
            }
        }

        /**
         * @brief Flip operation - swap Main and Alt indices
         * @param mainIndices Main texture index array
         * @param altIndices Alt texture index array
         * @param useAlt true=read Alt write Main, false=read Main write Alt
         */
        void Flip(const uint32_t mainIndices[CBUFFER_COLOR_TARGETS_SIZE],
                  const uint32_t altIndices[CBUFFER_COLOR_TARGETS_SIZE],
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
    };

    // Compile-time validation: ensure struct size is 128 bytes
    static_assert(sizeof(ColorTargetsIndexBuffer) == 128,
                  "ColorTargetsIndexBuffer must be exactly 128 bytes to match HLSL cbuffer");

    // Compile-time validation: ensure proper alignment
    static_assert(alignof(ColorTargetsIndexBuffer) == 4,
                  "ColorTargetsIndexBuffer must be 4-byte aligned for GPU upload");
} // namespace enigma::graphic
