/**
 * @file BlendState.hpp
 * @brief [REFACTOR] Blend state configuration - BlendConfig struct
 *
 * Follows RasterizationConfig pattern:
 * - Type aliases mapping to D3D12 types
 * - Constexpr namespaces for constants
 * - Static preset methods for common configurations
 * - operator== for PSO caching
 */

#pragma once

#include <cstdint>
#include <d3d12.h>

namespace enigma::graphic
{
    // ========================================
    // [NEW] Blend Configuration Type Aliases
    // ========================================

    /**
     * @brief Blend factor type alias
     * @details Maps to D3D12_BLEND for source/dest blend factors
     */
    using BlendFactor = D3D12_BLEND;

    /**
     * @brief Blend operation type alias
     * @details Maps to D3D12_BLEND_OP for blend operations
     */
    using BlendOp = D3D12_BLEND_OP;

    /**
     * @brief Color write mask type alias
     * @details Maps to D3D12_COLOR_WRITE_ENABLE for render target write mask
     */
    using ColorWriteMask = uint8_t;

    // ========================================
    // [NEW] Blend Factor Constants
    // ========================================

    namespace BlendFactorMode
    {
        constexpr BlendFactor Zero         = D3D12_BLEND_ZERO;
        constexpr BlendFactor One          = D3D12_BLEND_ONE;
        constexpr BlendFactor SrcColor     = D3D12_BLEND_SRC_COLOR;
        constexpr BlendFactor InvSrcColor  = D3D12_BLEND_INV_SRC_COLOR;
        constexpr BlendFactor SrcAlpha     = D3D12_BLEND_SRC_ALPHA;
        constexpr BlendFactor InvSrcAlpha  = D3D12_BLEND_INV_SRC_ALPHA;
        constexpr BlendFactor DestAlpha    = D3D12_BLEND_DEST_ALPHA;
        constexpr BlendFactor InvDestAlpha = D3D12_BLEND_INV_DEST_ALPHA;
        constexpr BlendFactor DestColor    = D3D12_BLEND_DEST_COLOR;
        constexpr BlendFactor InvDestColor = D3D12_BLEND_INV_DEST_COLOR;
        constexpr BlendFactor SrcAlphaSat  = D3D12_BLEND_SRC_ALPHA_SAT;
    }

    // ========================================
    // [NEW] Blend Operation Constants
    // ========================================

    namespace BlendOperation
    {
        constexpr BlendOp Add         = D3D12_BLEND_OP_ADD;
        constexpr BlendOp Subtract    = D3D12_BLEND_OP_SUBTRACT;
        constexpr BlendOp RevSubtract = D3D12_BLEND_OP_REV_SUBTRACT;
        constexpr BlendOp Min         = D3D12_BLEND_OP_MIN;
        constexpr BlendOp Max         = D3D12_BLEND_OP_MAX;
    }

    // ========================================
    // [NEW] Color Write Mask Constants
    // ========================================

    namespace ColorWriteEnable
    {
        constexpr ColorWriteMask Red   = D3D12_COLOR_WRITE_ENABLE_RED;
        constexpr ColorWriteMask Green = D3D12_COLOR_WRITE_ENABLE_GREEN;
        constexpr ColorWriteMask Blue  = D3D12_COLOR_WRITE_ENABLE_BLUE;
        constexpr ColorWriteMask Alpha = D3D12_COLOR_WRITE_ENABLE_ALPHA;
        constexpr ColorWriteMask All   = D3D12_COLOR_WRITE_ENABLE_ALL;
    }

    // ========================================
    // [NEW] BlendConfig Structure
    // ========================================

    /**
     * @brief Blend configuration structure
     * @details
     * Complete blend state mapping to D3D12_RENDER_TARGET_BLEND_DESC.
     * Controls color and alpha blending for render targets.
     */
    struct BlendConfig
    {
        // [KEY] Enable/disable blending
        bool blendEnabled = false;

        // [KEY] Color blend configuration
        BlendFactor srcBlend  = BlendFactorMode::One;
        BlendFactor destBlend = BlendFactorMode::Zero;
        BlendOp     blendOp   = BlendOperation::Add;

        // [KEY] Alpha blend configuration
        BlendFactor srcBlendAlpha  = BlendFactorMode::One;
        BlendFactor destBlendAlpha = BlendFactorMode::Zero;
        BlendOp     blendOpAlpha   = BlendOperation::Add;

        // [KEY] Render target write mask
        ColorWriteMask renderTargetWriteMask = ColorWriteEnable::All;

        // ========================================
        // Static Preset Methods
        // ========================================

        /**
         * @brief Opaque rendering - no blending
         * @return BlendConfig for solid objects, G-Buffer pass
         */
        static inline BlendConfig Opaque()
        {
            BlendConfig config;
            config.blendEnabled = false;
            return config;
        }

        /**
         * @brief Standard Alpha blending
         * @return BlendConfig for translucent objects (glass, particles)
         * @details Formula: Final = Src * SrcAlpha + Dst * (1 - SrcAlpha)
         */
        static inline BlendConfig Alpha()
        {
            BlendConfig config;
            config.blendEnabled   = true;
            config.srcBlend       = BlendFactorMode::SrcAlpha;
            config.destBlend      = BlendFactorMode::InvSrcAlpha;
            config.blendOp        = BlendOperation::Add;
            config.srcBlendAlpha  = BlendFactorMode::One;
            config.destBlendAlpha = BlendFactorMode::InvSrcAlpha;
            config.blendOpAlpha   = BlendOperation::Add;
            return config;
        }

        /**
         * @brief Additive blending
         * @return BlendConfig for glow effects (bloom, fire, explosions)
         * @details Formula: Final = Src + Dst
         */
        static inline BlendConfig Additive()
        {
            BlendConfig config;
            config.blendEnabled   = true;
            config.srcBlend       = BlendFactorMode::One;
            config.destBlend      = BlendFactorMode::One;
            config.blendOp        = BlendOperation::Add;
            config.srcBlendAlpha  = BlendFactorMode::One;
            config.destBlendAlpha = BlendFactorMode::One;
            config.blendOpAlpha   = BlendOperation::Add;
            return config;
        }

        /**
         * @brief Multiplicative blending
         * @return BlendConfig for shadow, darkening effects
         * @details Formula: Final = Src * Dst
         */
        static inline BlendConfig Multiply()
        {
            BlendConfig config;
            config.blendEnabled   = true;
            config.srcBlend       = BlendFactorMode::DestColor;
            config.destBlend      = BlendFactorMode::Zero;
            config.blendOp        = BlendOperation::Add;
            config.srcBlendAlpha  = BlendFactorMode::DestAlpha;
            config.destBlendAlpha = BlendFactorMode::Zero;
            config.blendOpAlpha   = BlendOperation::Add;
            return config;
        }

        /**
         * @brief Premultiplied Alpha blending
         * @return BlendConfig for premultiplied alpha textures (UI frameworks)
         * @details Formula: Final = Src + Dst * (1 - SrcAlpha)
         */
        static inline BlendConfig Premultiplied()
        {
            BlendConfig config;
            config.blendEnabled   = true;
            config.srcBlend       = BlendFactorMode::One;
            config.destBlend      = BlendFactorMode::InvSrcAlpha;
            config.blendOp        = BlendOperation::Add;
            config.srcBlendAlpha  = BlendFactorMode::One;
            config.destBlendAlpha = BlendFactorMode::InvSrcAlpha;
            config.blendOpAlpha   = BlendOperation::Add;
            return config;
        }

        // ========================================
        // Comparison Operators
        // ========================================

        /**
         * @brief Equality comparison for PSO caching
         */
        bool operator==(const BlendConfig& other) const
        {
            return blendEnabled == other.blendEnabled &&
                srcBlend == other.srcBlend &&
                destBlend == other.destBlend &&
                blendOp == other.blendOp &&
                srcBlendAlpha == other.srcBlendAlpha &&
                destBlendAlpha == other.destBlendAlpha &&
                blendOpAlpha == other.blendOpAlpha &&
                renderTargetWriteMask == other.renderTargetWriteMask;
        }

        bool operator!=(const BlendConfig& other) const
        {
            return !(*this == other);
        }
    };

    // ========================================
    // [OLD] BlendMode enum - DEPRECATED
    // Keep for backward compatibility, will be removed
    // ========================================

    enum class BlendMode : uint8_t
    {
        Opaque,
        Alpha,
        Additive,
        Multiply,
        Premultiplied,
        NonPremultiplied,
        Disabled
    };
} // namespace enigma::graphic
