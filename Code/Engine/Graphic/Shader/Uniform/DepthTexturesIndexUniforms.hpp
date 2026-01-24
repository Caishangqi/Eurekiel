#pragma once

// ============================================================================
// DepthTexturesIndexUniforms.hpp - [REFACTOR] Depth texture index management
// Part of Shader RT Fetching Feature for Flexible Deferred Rendering
// ============================================================================

#include <cstdint>
#include "Engine/Graphic/Target/RenderTargetProviderCommon.hpp"

namespace enigma::graphic
{
    /**
     * @brief DepthTexturesIndexBuffer - Depth texture index management for depthtex0-15
     *
     * Key points:
     * 1. Stores indices for up to 16 depth textures (Iris depthtex0-2 + custom)
     * 2. No Main/Alt flip needed (depth textures are generated each frame)
     * 3. Uploaded to GPU cbuffer before each Pass execution
     * 4. Must match HLSL DepthTexturesBuffer struct (64 bytes)
     *
     * Iris depth texture semantics:
     * - depthtex0: Full depth (after translucent objects)
     * - depthtex1: Depth before translucents (no translucent objects)
     * - depthtex2: Depth before hand (no hand)
     *
     * @note Size must be exactly 64 bytes to match HLSL cbuffer
     */
    struct DepthTexturesIndexUniforms
    {
        // Depth texture indices array (depthtex0-15)
        uint32_t depthTextureIndices[CBUFFER_DEPTH_TEXTURES_SIZE];

        // ===== Total: 16 * 4 = 64 bytes =====

        /**
         * @brief Default constructor - initializes all indices to INVALID_BINDLESS_INDEX
         */
        DepthTexturesIndexUniforms()
        {
            Reset();
        }

        // ========== [NEW] Unified API ==========

        /**
         * @brief Set single depth texture index (unified API)
         * @param slot Slot index (0-15)
         * @param bindlessIndex Bindless texture index
         */
        void SetIndex(uint32_t slot, uint32_t bindlessIndex)
        {
            if (slot < CBUFFER_DEPTH_TEXTURES_SIZE)
            {
                depthTextureIndices[slot] = bindlessIndex;
            }
        }

        /**
         * @brief Get single depth texture index (unified API)
         * @param slot Slot index (0-15)
         * @return Bindless index, or INVALID_BINDLESS_INDEX if out of range
         */
        uint32_t GetIndex(uint32_t slot) const
        {
            return (slot < CBUFFER_DEPTH_TEXTURES_SIZE) ? depthTextureIndices[slot] : INVALID_BINDLESS_INDEX;
        }

        /**
         * @brief Check if buffer has any valid indices
         * @return true if at least one index is valid
         */
        bool IsValid() const
        {
            for (uint32_t i = 0; i < CBUFFER_DEPTH_TEXTURES_SIZE; ++i)
            {
                if (depthTextureIndices[i] != INVALID_BINDLESS_INDEX)
                {
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief Check if specified count of indices are valid
         * @param count Number of indices to check (1-16)
         * @return true if first 'count' indices are all valid
         */
        bool IsValidCount(uint32_t count) const
        {
            if (count < 1 || count > CBUFFER_DEPTH_TEXTURES_SIZE) return false;

            for (uint32_t i = 0; i < count; ++i)
            {
                if (depthTextureIndices[i] == INVALID_BINDLESS_INDEX)
                {
                    return false;
                }
            }
            return true;
        }

        /**
         * @brief Reset all indices to invalid state
         */
        void Reset()
        {
            for (uint32_t i = 0; i < CBUFFER_DEPTH_TEXTURES_SIZE; ++i)
            {
                depthTextureIndices[i] = INVALID_BINDLESS_INDEX;
            }
        }

        // ========== Batch Operations ==========

        /**
         * @brief Set first 3 depth texture indices (Iris compatibility)
         * @param depth0 depthtex0 bindless index (full depth)
         * @param depth1 depthtex1 bindless index (no translucents)
         * @param depth2 depthtex2 bindless index (no hand)
         */
        void SetIndices(uint32_t depth0, uint32_t depth1, uint32_t depth2)
        {
            depthTextureIndices[0] = depth0;
            depthTextureIndices[1] = depth1;
            depthTextureIndices[2] = depth2;
        }
    };

    // Compile-time validation: ensure struct size is 64 bytes
    static_assert(sizeof(DepthTexturesIndexUniforms) == 64,
                  "DepthTexturesIndexUniforms must be exactly 64 bytes to match HLSL cbuffer");

    // Compile-time validation: ensure 16-byte alignment for GPU
    static_assert(sizeof(DepthTexturesIndexUniforms) % 16 == 0,
                  "DepthTexturesIndexUniforms must be aligned to 16 bytes for GPU upload");

    // Compile-time validation: ensure proper element alignment
    static_assert(alignof(DepthTexturesIndexUniforms) == 4,
                  "DepthTexturesIndexUniforms must be 4-byte aligned for GPU upload");
} // namespace enigma::graphic
