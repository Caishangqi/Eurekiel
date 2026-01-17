/**
 * @file StencilState.hpp
 * @brief Template test status configuration - StencilTestDetail structure
 *
 * Teaching focus:
 * 1. Template state in DirectX 12 is part of PSO (Pipeline State Object)
 * 2. Understand the three operations of template testing: pass, fail, and deep failure
 * 3. Learn the application scenarios of double-sided template testing
 */

#pragma once

#include <cstdint>
#include <d3d12.h>

namespace enigma::graphic
{
    // ========================================
    // Stencil Test Type Aliases
    // ========================================

    /**
     * @brief Stencil operation type alias
     * @details Maps to D3D12_STENCIL_OP for stencil buffer operations
     */
    using StencilOp = D3D12_STENCIL_OP;

    /**
     * @brief Stencil comparison function type alias
     * @details Maps to D3D12_COMPARISON_FUNC for stencil testing
     */
    using StencilFunc = D3D12_COMPARISON_FUNC;

    /**
     * @brief Stencil operation constants
     */
    namespace StencilOperation
    {
        constexpr StencilOp Keep    = D3D12_STENCIL_OP_KEEP; ///< Keep existing value
        constexpr StencilOp Zero    = D3D12_STENCIL_OP_ZERO; ///< Set to 0
        constexpr StencilOp Replace = D3D12_STENCIL_OP_REPLACE; ///< Replace with reference
        constexpr StencilOp IncrSat = D3D12_STENCIL_OP_INCR_SAT; ///< Increment with saturation
        constexpr StencilOp DecrSat = D3D12_STENCIL_OP_DECR_SAT; ///< Decrement with saturation
        constexpr StencilOp Invert  = D3D12_STENCIL_OP_INVERT; ///< Bitwise invert
        constexpr StencilOp Incr    = D3D12_STENCIL_OP_INCR; ///< Increment with wrapping
        constexpr StencilOp Decr    = D3D12_STENCIL_OP_DECR; ///< Decrement with wrapping
    }

    /**
     * @brief Stencil comparison function constants
     */
    namespace StencilComparison
    {
        constexpr StencilFunc Never        = D3D12_COMPARISON_FUNC_NEVER;
        constexpr StencilFunc Less         = D3D12_COMPARISON_FUNC_LESS;
        constexpr StencilFunc Equal        = D3D12_COMPARISON_FUNC_EQUAL;
        constexpr StencilFunc LessEqual    = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        constexpr StencilFunc Greater      = D3D12_COMPARISON_FUNC_GREATER;
        constexpr StencilFunc NotEqual     = D3D12_COMPARISON_FUNC_NOT_EQUAL;
        constexpr StencilFunc GreaterEqual = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        constexpr StencilFunc Always       = D3D12_COMPARISON_FUNC_ALWAYS;
    }

    /**
     * @brief Stencil test configuration detail
     * @details
     * Complete stencil testing configuration mapping to D3D12_DEPTH_STENCIL_DESC.
     * Supports both single-sided and dual-sided stencil testing.
     * 
     * Memory Layout:
     * - 14 fields aligned for cache efficiency
     * - Total size: ~32 bytes (aligned to 16 bytes)
     */
    struct StencilTestDetail
    {
        bool    enable   = false; ///< Enable stencil testing
        uint8_t refValue = 0; ///< Reference value for comparison (0-255)

        // Front face stencil operations
        StencilFunc stencilFunc        = StencilComparison::Always;
        StencilOp   stencilPassOp      = StencilOperation::Keep;
        StencilOp   stencilFailOp      = StencilOperation::Keep;
        StencilOp   stencilDepthFailOp = StencilOperation::Keep;

        uint8_t stencilReadMask  = 0xFF; ///< Mask for reading stencil buffer
        uint8_t stencilWriteMask = 0xFF; ///< Mask for writing stencil buffer

        bool depthWriteEnable = true; ///< Enable depth write during stencil test

        // Back face stencil operations (only used if useSeparateFrontBack == true)
        bool        useSeparateFrontBack       = false;
        StencilFunc backFaceStencilFunc        = StencilComparison::Always;
        StencilOp   backFaceStencilPassOp      = StencilOperation::Keep;
        StencilOp   backFaceStencilFailOp      = StencilOperation::Keep;
        StencilOp   backFaceStencilDepthFailOp = StencilOperation::Keep;

        // ========================================
        // Static Preset Methods
        // ========================================

        /**
         * @brief Disabled stencil test
         * @return StencilTestDetail with enable = false
         */
        static inline StencilTestDetail Disabled()
        {
            StencilTestDetail detail;
            detail.enable = false;
            return detail;
        }

        /**
         * @brief Mark pixels unconditionally
         * @return StencilTestDetail for unconditional marking
         */
        static inline StencilTestDetail MarkAlways()
        {
            StencilTestDetail detail;
            detail.enable           = true;
            detail.stencilFunc      = StencilComparison::Always;
            detail.stencilPassOp    = StencilOperation::Replace;
            detail.stencilWriteMask = 0xFF;
            detail.depthWriteEnable = true;
            return detail;
        }

        /**
         * @brief Test for equal stencil values
         * @return StencilTestDetail for equality testing
         */
        static inline StencilTestDetail TestEqual()
        {
            StencilTestDetail detail;
            detail.enable           = true;
            detail.stencilFunc      = StencilComparison::Equal;
            detail.stencilPassOp    = StencilOperation::Keep;
            detail.stencilFailOp    = StencilOperation::Keep;
            detail.stencilWriteMask = 0x00; // Read-only test
            detail.depthWriteEnable = true;
            return detail;
        }

        /**
         * @brief Test for non-equal stencil values
         * @return StencilTestDetail for inequality testing
         */
        static inline StencilTestDetail TestNotEqual()
        {
            StencilTestDetail detail;
            detail.enable           = true;
            detail.stencilFunc      = StencilComparison::NotEqual;
            detail.stencilPassOp    = StencilOperation::Keep;
            detail.stencilFailOp    = StencilOperation::Keep;
            detail.stencilWriteMask = 0x00; // Read-only test
            detail.depthWriteEnable = true;
            return detail;
        }

        /**
         * @brief Outline rendering using stencil
         * @return StencilTestDetail for outline rendering
         */
        static inline StencilTestDetail OutlineNotEqual()
        {
            StencilTestDetail detail;
            detail.enable           = true;
            detail.stencilFunc      = StencilComparison::NotEqual;
            detail.stencilPassOp    = StencilOperation::Keep;
            detail.stencilFailOp    = StencilOperation::Keep;
            detail.stencilWriteMask = 0x00;
            detail.depthWriteEnable = false;
            return detail;
        }

        /**
         * @brief Shadow volume front face rendering
         * @return StencilTestDetail for shadow volume front faces
         */
        static inline StencilTestDetail ShadowVolumeFront()
        {
            StencilTestDetail detail;
            detail.enable             = true;
            detail.stencilFunc        = StencilComparison::Always;
            detail.stencilPassOp      = StencilOperation::Keep;
            detail.stencilFailOp      = StencilOperation::Keep;
            detail.stencilDepthFailOp = StencilOperation::Incr;
            detail.depthWriteEnable   = false;
            return detail;
        }

        /**
         * @brief Shadow volume back face rendering
         * @return StencilTestDetail for shadow volume back faces
         */
        static inline StencilTestDetail ShadowVolumeBack()
        {
            StencilTestDetail detail;
            detail.enable             = true;
            detail.stencilFunc        = StencilComparison::Always;
            detail.stencilPassOp      = StencilOperation::Keep;
            detail.stencilFailOp      = StencilOperation::Keep;
            detail.stencilDepthFailOp = StencilOperation::Decr;
            detail.depthWriteEnable   = false;
            return detail;
        }

        /**
         * @brief Equality comparison operator for PSO caching
         */
        bool operator==(const StencilTestDetail& other) const
        {
            return enable == other.enable &&
                refValue == other.refValue &&
                stencilFunc == other.stencilFunc &&
                stencilPassOp == other.stencilPassOp &&
                stencilFailOp == other.stencilFailOp &&
                stencilDepthFailOp == other.stencilDepthFailOp &&
                stencilReadMask == other.stencilReadMask &&
                stencilWriteMask == other.stencilWriteMask &&
                depthWriteEnable == other.depthWriteEnable &&
                useSeparateFrontBack == other.useSeparateFrontBack &&
                backFaceStencilFunc == other.backFaceStencilFunc &&
                backFaceStencilPassOp == other.backFaceStencilPassOp &&
                backFaceStencilFailOp == other.backFaceStencilFailOp &&
                backFaceStencilDepthFailOp == other.backFaceStencilDepthFailOp;
        }
    };
} // namespace enigma::graphic
