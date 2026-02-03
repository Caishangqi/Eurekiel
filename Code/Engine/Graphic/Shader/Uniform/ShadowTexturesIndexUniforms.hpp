#pragma once

// ============================================================================
// ShadowTexturesIndexUniforms.hpp - [REFACTOR] Shadow depth texture index management
// Part of Shader RT Fetching Feature for Flexible Deferred Rendering
// ============================================================================

#include <cstdint>
#include "Engine/Graphic/Target/RenderTargetProviderCommon.hpp"

namespace enigma::graphic
{
    /**
     * @brief ShadowTexturesIndexBuffer - Shadow depth texture index management for shadowtex0-1
     *
     * Key points:
     * 1. Manages shadowtex0/1 read-only depth texture indices
     * 2. No Flip mechanism needed (generated each frame by Shadow Pass)
     * 3. Separated from ShadowColorIndexBuffer (single responsibility)
     * 4. Must match HLSL ShadowTexturesIndexUniforms struct (16 bytes)
     *
     * Shadow depth texture semantics:
     * - shadowtex0: Full shadow depth (all objects including translucent)
     * - shadowtex1: Shadow depth before translucent objects
     *
     * @note Size must be exactly 16 bytes to match HLSL cbuffer
     */
    struct ShadowTexturesIndexUniforms
    {
        // Shadow texture indices array (shadowtex0-1)
        uint32_t indices[CBUFFER_SHADOW_TEXTURES_SIZE];

        /**
         * @brief Default constructor - initializes all indices to INVALID_BINDLESS_INDEX
         */
        ShadowTexturesIndexUniforms()
        {
            Reset();
        }

        // ========== [NEW] Unified API ==========

        /**
         * @brief Set single shadow texture index (unified API)
         * @param slot Slot index (0-1)
         * @param bindlessIndex Bindless texture index
         */
        void SetIndex(uint32_t slot, uint32_t bindlessIndex)
        {
            if (slot < CBUFFER_SHADOW_TEXTURES_SIZE)
            {
                indices[slot] = bindlessIndex;
            }
        }

        /**
         * @brief Get single shadow texture index (unified API)
         * @param slot Slot index (0-1)
         * @return Bindless index, or INVALID_BINDLESS_INDEX if out of range
         */
        uint32_t GetIndex(uint32_t slot) const
        {
            return (slot < CBUFFER_SHADOW_TEXTURES_SIZE) ? indices[slot] : INVALID_BINDLESS_INDEX;
        }

        /**
         * @brief Check if buffer has any valid indices
         * @return true if at least one index is valid
         */
        bool IsValid() const
        {
            for (uint32_t i = 0; i < CBUFFER_SHADOW_TEXTURES_SIZE; ++i)
            {
                if (indices[i] != INVALID_BINDLESS_INDEX)
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
            for (uint32_t i = 0; i < CBUFFER_SHADOW_TEXTURES_SIZE; ++i)
            {
                indices[i] = INVALID_BINDLESS_INDEX;
            }
        }
    };

    // Compile-time validation: ensure proper element alignment
    static_assert(alignof(ShadowTexturesIndexUniforms) == 4,
                  "ShadowTexturesIndexUniforms must be 4-byte aligned for GPU upload");
} // namespace enigma::graphic
