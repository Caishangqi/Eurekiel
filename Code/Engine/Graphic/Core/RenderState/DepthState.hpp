/**
 * @file DepthState.hpp
 * @brief Depth state configuration - DepthMode enumeration and DepthConfig structure
 *
 *Teaching focus:
 * 1. The depth state in DirectX 12 is part of PSO (Pipeline State Object)
 * 2. Understand the configuration of depth testing and depth writing
 * 3. DepthConfig provides flexible depth configuration, replacing the fixed DepthMode enumeration
 */

#pragma once

#include <cstdint>
#include <d3d12.h>

namespace enigma::graphic
{
    // ========================================
    // Depth Configuration Type Aliases
    // ========================================

    /**
     * @brief Depth comparison function type alias
     * @details Maps to D3D12_COMPARISON_FUNC for depth testing
     */
    using DepthFunc = D3D12_COMPARISON_FUNC;

    /**
     * @brief Depth comparison function constants
     * @details
     * Defines how fragment depth is compared with depth buffer value.
     * 
     * Educational Points:
     * - LessEqual: Standard depth test (default, most common)
     * - Less: Strict comparison, avoids Z-fighting
     * - GreaterEqual/Greater: Reverse-Z for improved precision
     * - Always: Depth test always passes (useful for sky rendering)
     */
    namespace DepthComparison
    {
        constexpr DepthFunc Never        = D3D12_COMPARISON_FUNC_NEVER; ///< Test never passes
        constexpr DepthFunc Less         = D3D12_COMPARISON_FUNC_LESS; ///< Pass if fragment < buffer (strict)
        constexpr DepthFunc Equal        = D3D12_COMPARISON_FUNC_EQUAL; ///< Pass if fragment == buffer
        constexpr DepthFunc LessEqual    = D3D12_COMPARISON_FUNC_LESS_EQUAL; ///< Pass if fragment <= buffer (default)
        constexpr DepthFunc Greater      = D3D12_COMPARISON_FUNC_GREATER; ///< Pass if fragment > buffer
        constexpr DepthFunc NotEqual     = D3D12_COMPARISON_FUNC_NOT_EQUAL; ///< Pass if fragment != buffer
        constexpr DepthFunc GreaterEqual = D3D12_COMPARISON_FUNC_GREATER_EQUAL; ///< Pass if fragment >= buffer (reverse-Z)
        constexpr DepthFunc Always       = D3D12_COMPARISON_FUNC_ALWAYS; ///< Test always passes
    }

    /**
     * @brief Depth configuration structure
     * @details
     * Complete depth testing state mapping to D3D12_DEPTH_STENCIL_DESC (depth portion).
     * Provides flexible control over depth testing, writing, and comparison functions.
     * 
     * Design Philosophy:
     * - Follows RasterizationConfig pattern for consistency
     * - Provides static preset methods for common configurations
     * - Supports custom configurations for advanced use cases
     * - Replaces inflexible DepthMode enum for PSO configuration
     * 
     * Memory Layout:
     * - 3 configurable fields
     * - Total size: 8 bytes (aligned for cache efficiency)
     */
    struct DepthConfig
    {
        // [KEY] Core depth configuration
        bool      depthTestEnabled  = true; ///< Enable depth testing
        bool      depthWriteEnabled = true; ///< Enable depth writing
        DepthFunc depthFunc         = DepthComparison::LessEqual; ///< Depth comparison function

        // ========================================
        // Static Preset Methods
        // ========================================

        /**
         * @brief Standard depth test with write (default)
         * @details Full depth functionality for opaque geometry
         * @return DepthConfig configured for standard depth testing
         */
        static inline DepthConfig Enabled()
        {
            DepthConfig config;
            config.depthTestEnabled  = true;
            config.depthWriteEnabled = true;
            config.depthFunc         = DepthComparison::LessEqual;
            return config;
        }

        /**
         * @brief Read-only depth test (test but don't write)
         * @details Essential for translucent rendering
         * @return DepthConfig configured for read-only depth testing
         */
        static inline DepthConfig ReadOnly()
        {
            DepthConfig config;
            config.depthTestEnabled  = true;
            config.depthWriteEnabled = false;
            config.depthFunc         = DepthComparison::LessEqual;
            return config;
        }

        /**
         * @brief Write-only depth (write but don't test)
         * @details Rarely used, for depth buffer initialization
         * @return DepthConfig configured for write-only depth
         */
        static inline DepthConfig WriteOnly()
        {
            DepthConfig config;
            config.depthTestEnabled  = true;
            config.depthWriteEnabled = true;
            config.depthFunc         = DepthComparison::Always;
            return config;
        }

        /**
         * @brief Disabled depth (no test, no write)
         * @details For fullscreen post-processing and UI
         * @return DepthConfig configured with depth disabled
         */
        static inline DepthConfig Disabled()
        {
            DepthConfig config;
            config.depthTestEnabled  = false;
            config.depthWriteEnabled = false;
            config.depthFunc         = DepthComparison::Always;
            return config;
        }

        /**
         * @brief Translucent rendering configuration
         * @details Alias for ReadOnly() with clearer semantic meaning
         * @return DepthConfig configured for translucent rendering
         */
        static inline DepthConfig Translucent()
        {
            return ReadOnly();
        }

        /**
         * @brief Reverse-Z depth configuration
         * @details Uses GreaterEqual for improved depth precision
         * @return DepthConfig configured for reverse-Z rendering
         */
        static inline DepthConfig ReverseZ()
        {
            DepthConfig config;
            config.depthTestEnabled  = true;
            config.depthWriteEnabled = true;
            config.depthFunc         = DepthComparison::GreaterEqual;
            return config;
        }

        /**
         * @brief Reverse-Z read-only configuration
         * @details For translucent objects in reverse-Z pipelines
         * @return DepthConfig configured for reverse-Z translucent rendering
         */
        static inline DepthConfig ReverseZReadOnly()
        {
            DepthConfig config;
            config.depthTestEnabled  = true;
            config.depthWriteEnabled = false;
            config.depthFunc         = DepthComparison::GreaterEqual;
            return config;
        }

        /**
         * @brief Strict less-than comparison
         * @details Avoids Z-fighting on coplanar surfaces
         * @return DepthConfig configured with strict less comparison
         */
        static inline DepthConfig LessStrict()
        {
            DepthConfig config;
            config.depthTestEnabled  = true;
            config.depthWriteEnabled = true;
            config.depthFunc         = DepthComparison::Less;
            return config;
        }

        /**
         * @brief Custom depth configuration builder
         * @param testEnabled Enable depth testing
         * @param writeEnabled Enable depth writing
         * @param func Depth comparison function
         * @return DepthConfig with custom settings
         */
        static inline DepthConfig Custom(bool testEnabled, bool writeEnabled, DepthFunc func)
        {
            DepthConfig config;
            config.depthTestEnabled  = testEnabled;
            config.depthWriteEnabled = writeEnabled;
            config.depthFunc         = func;
            return config;
        }

        /**
         * @brief Equality comparison operator for PSO caching
         */
        bool operator==(const DepthConfig& other) const
        {
            return depthTestEnabled == other.depthTestEnabled &&
                depthWriteEnabled == other.depthWriteEnabled &&
                depthFunc == other.depthFunc;
        }

        /**
         * @brief Inequality comparison operator
         */
        bool operator!=(const DepthConfig& other) const
        {
            return !(*this == other);
        }
    };
} // namespace enigma::graphic
