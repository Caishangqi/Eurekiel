#pragma once

// ============================================================================
// ShadowTexturesIndexBuffer.hpp - [REFACTOR] Shadow depth texture index management
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
     * 4. Must match HLSL ShadowTexturesBuffer struct (16 bytes)
     *
     * Shadow depth texture semantics:
     * - shadowtex0: Full shadow depth (all objects including translucent)
     * - shadowtex1: Shadow depth before translucent objects
     *
     * @note Size must be exactly 16 bytes to match HLSL cbuffer
     */
    struct ShadowTexturesIndexBuffer
    {
        // Shadow texture indices array (shadowtex0-1)
        uint32_t indices[CBUFFER_SHADOW_TEXTURES_SIZE];

        // Padding to align to 16 bytes (DirectX 12 cbuffer requirement)
        uint32_t padding[2];

        // ===== Total: (2 + 2) * 4 = 16 bytes =====

        /**
         * @brief Default constructor - initializes all indices to INVALID_BINDLESS_INDEX
         */
        ShadowTexturesIndexBuffer()
            : indices{INVALID_BINDLESS_INDEX, INVALID_BINDLESS_INDEX}
              , padding{0, 0}
        {
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
         * @brief Check if both shadow textures are valid
         * @return true if both indices are valid
         */
        bool HasBothTextures() const
        {
            return indices[0] != INVALID_BINDLESS_INDEX &&
                indices[1] != INVALID_BINDLESS_INDEX;
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
            padding[0] = 0;
            padding[1] = 0;
        }

        // ========== Batch Operations ==========

        /**
         * @brief Set both shadow texture indices (batch operation)
         * @param shadowtex0 shadowtex0 bindless index (full shadow depth)
         * @param shadowtex1 shadowtex1 bindless index (no translucents)
         */
        void SetIndices(uint32_t shadowtex0, uint32_t shadowtex1)
        {
            indices[0] = shadowtex0;
            indices[1] = shadowtex1;
        }
    };

    // Compile-time validation: ensure struct size is 16 bytes
    static_assert(sizeof(ShadowTexturesIndexBuffer) == 16,
                  "ShadowTexturesIndexBuffer must be exactly 16 bytes to match HLSL cbuffer");

    // Compile-time validation: ensure 16-byte alignment for GPU
    static_assert(sizeof(ShadowTexturesIndexBuffer) % 16 == 0,
                  "ShadowTexturesIndexBuffer must be aligned to 16 bytes for GPU upload");

    // Compile-time validation: ensure proper element alignment
    static_assert(alignof(ShadowTexturesIndexBuffer) == 4,
                  "ShadowTexturesIndexBuffer must be 4-byte aligned for GPU upload");
} // namespace enigma::graphic
