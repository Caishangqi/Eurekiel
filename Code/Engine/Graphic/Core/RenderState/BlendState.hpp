/**
 * @file BlendState.hpp
 * @brief Mixed state enumeration definition - BlendMode
 *
 * Teaching focus:
 * 1. Mixed state in DirectX 12 is part of PSO (Pipeline State Object)
 * 2. Understand the application scenarios of different mixing modes
 * 3. Learn static state management of modern graphics API
 */

#pragma once

#include <cstdint>

namespace enigma::graphic
{
    /**
     * @brief mixed mode enumeration
     * @details
     * Defines commonly used color mixing modes for configuring the mixing state of PSO.
     * The blending state in DirectX 12 is static and needs to be specified when creating the PSO.
     *
     * Teaching points:
     * - Opaque: opaque rendering, the most commonly used modes (skybox, solid object)
     * - Alpha: Standard Alpha blending, used for translucent objects (glass, particle effects)
     * - Additive: Additive blending, used for glowing effects (halos, flames)
     * - Multiply: Multiplicative blending, used for shadow and darkening effects
     * - Premultiplied: Premultiplied Alpha for correct translucent blending
     */
    enum class BlendMode : uint8_t
    {
        /**
         * @brief opacity mode - no blending
         * @details Completely opaque, directly covering the target color
         *
         * DirectX 12 configuration:
         * - BlendEnable = FALSE
         *
         * Application scenarios:
         * - All solid objects (terrain, buildings, characters)
         * - skybox
         * - G-Buffer Pass for deferred rendering
         */
        Opaque,

        /**
         * @brief Standard Alpha blending
         * @details Linear interpolation blending based on source Alpha value
         *
         * DirectX 12 configuration:
         * - BlendEnable = TRUE
         * - SrcBlend = D3D12_BLEND_SRC_ALPHA
         * - DestBlend = D3D12_BLEND_INV_SRC_ALPHA
         * - BlendOp = D3D12_BLEND_OP_ADD
         *
         * Mixing formula: FinalColor = SrcColor * SrcAlpha + DstColor * (1 - SrcAlpha)
         *
         * Application scenarios:
         * - Translucent objects (glass, water)
         * - UI elements
         * - Particle effects (smoke, fog)
         */
        Alpha,

        /**
         * @brief additive mixing
         * @details Source and target colors are added together for glowing effects
         *
         * DirectX 12 configuration:
         * - BlendEnable = TRUE
         * - SrcBlend = D3D12_BLEND_ONE
         * - DestBlend = D3D12_BLEND_ONE
         * - BlendOp = D3D12_BLEND_OP_ADD
         *
         * Mixing formula: FinalColor = SrcColor + DstColor
         *
         * Application scenarios:
         * - Glowing effects (halo, Bloom)
         * - Fire, explosion
         * - Particle accumulation effect
         *
         *Note: Color values will overlap, possibly resulting in overexposure
         */
        Additive,

        /**
         * @brief multiplicative mixing
         * @details Source and target colors are multiplied for darkening effects
         *
         * DirectX 12 configuration:
         * - BlendEnable = TRUE
         * - SrcBlend = D3D12_BLEND_DEST_COLOR
         * - DestBlend = D3D12_BLEND_ZERO
         * - BlendOp = D3D12_BLEND_OP_ADD
         *
         * Mixing formula: FinalColor = SrcColor * DstColor
         *
         * Application scenarios:
         * - Shadow map blending
         * - Light falloff
         * - color filter
         */
        Multiply,

        /**
         * @brief Premultiplied Alpha blending
         * @details Assume source color is alpha premultiplied for correct translucent blending
         *
         * DirectX 12 configuration:
         * - BlendEnable = TRUE
         * - SrcBlend = D3D12_BLEND_ONE
         * - DestBlend = D3D12_BLEND_INV_SRC_ALPHA
         * - BlendOp = D3D12_BLEND_OP_ADD
         *
         * Mixing formula: FinalColor = SrcColor + DstColor * (1 - SrcAlpha)
         * Note: SrcColor has been pre-multiplied by Alpha, that is, SrcColor = OriginalColor * SrcAlpha
         *
         * Application scenarios:
         * - Premultiplied Alpha texture for image library
         * - Correct mix of UI frameworks
         * - Avoid black border problem
         */
        Premultiplied,

        /**
         * @brief Non-premultiplied Alpha blending (same as Alpha)
         * @details The source color is not premultiplied by Alpha, the same as Alpha mode
         *
         * Application scenarios:
         * - Explicitly mark non-premultiplied textures
         * - Same functionality as Alpha mode
         */
        NonPremultiplied,

        /**
         * @brief disable mixing
         * @details Explicitly disable mixing, the same function as Opaque
         *
         * Application scenarios:
         * - Explicitly mark Passes that do not require mixing
         */
        Disabled
    };
} // namespace enigma::graphic
