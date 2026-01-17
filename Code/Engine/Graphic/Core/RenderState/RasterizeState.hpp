/**
 * @file RasterizeState.hpp
 * @brief Rasterization status configuration - RasterizationConfig structure
 *
 * Teaching focus:
 * 1. Rasterization state in DirectX 12 is part of PSO (Pipeline State Object)
 * 2. Understand configurations such as fill mode, culling mode, depth offset, etc.
 * 3. Learn static state management of modern graphics API
 */

#pragma once

#include <cstdint>
#include <d3d12.h>

namespace enigma::graphic
{
    // ========================================
    // Rasterization Configuration Type Aliases
    // ========================================

    /**
     * @brief Fill mode type alias
     * @details Maps to D3D12_FILL_MODE for rasterizer fill behavior
     */
    using RasterizeFill = D3D12_FILL_MODE;

    /**
     * @brief Cull mode type alias
     * @details Maps to D3D12_CULL_MODE for face culling configuration
     */
    using RasterizeCull = D3D12_CULL_MODE;

    /**
     * @brief Fill mode constants
     */
    namespace RasterizeFillMode
    {
        constexpr RasterizeFill Solid     = D3D12_FILL_MODE_SOLID; ///< Filled triangles (default)
        constexpr RasterizeFill Wireframe = D3D12_FILL_MODE_WIREFRAME; ///< Wireframe debug mode
    }

    /**
     * @brief Cull mode constants
     */
    namespace RasterizeCullMode
    {
        constexpr RasterizeCull BackFace  = D3D12_CULL_MODE_BACK; ///< Cull back faces (default)
        constexpr RasterizeCull FrontFace = D3D12_CULL_MODE_FRONT; ///< Cull front faces
        constexpr RasterizeCull None      = D3D12_CULL_MODE_NONE; ///< No culling (double-sided)
    }

    /**
     * @brief Winding order enumeration
     */
    enum class RasterizeWindingOrder : uint8_t
    {
        CounterClockwise, ///< Counter-clockwise winding (OpenGL default)
        Clockwise ///< Clockwise winding (DirectX default)
    };

    /**
     * @brief Rasterization configuration structure
     * @details
     * Complete rasterization state mapping to D3D12_RASTERIZER_DESC.
     * Controls triangle filling, face culling, depth bias, and advanced features.
     * 
     * Memory Layout:
     * - 11 configurable fields aligned for cache efficiency
     * - Total size: 32 bytes (8-byte aligned)
     */
    struct RasterizationConfig
    {
        // [KEY] Fill and culling configuration
        RasterizeFill         fillMode     = RasterizeFillMode::Solid;
        RasterizeCull         cullMode     = RasterizeCullMode::BackFace;
        RasterizeWindingOrder windingOrder = RasterizeWindingOrder::CounterClockwise;

        // [KEY] Depth bias configuration (for shadow mapping)
        int32_t depthBias            = 0;
        float   depthBiasClamp       = 0.0f;
        float   slopeScaledDepthBias = 0.0f;

        // [KEY] Advanced rasterization features
        bool depthClipEnabled          = true;
        bool multisampleEnabled        = false;
        bool antialiasedLineEnabled    = false;
        bool conservativeRasterEnabled = false;

        // [KEY] Forced sample count (0 = use MSAA settings)
        uint32_t forcedSampleCount = 0;

        // ========================================
        // Static Preset Methods
        // ========================================

        /**
         * @brief Default rasterization with back-face culling
         * @return RasterizationConfig for standard opaque rendering
         */
        static inline RasterizationConfig CullBack()
        {
            RasterizationConfig config;
            config.fillMode     = RasterizeFillMode::Solid;
            config.cullMode     = RasterizeCullMode::BackFace;
            config.windingOrder = RasterizeWindingOrder::CounterClockwise;
            return config;
        }

        /**
         * @brief No culling for double-sided geometry
         * @return RasterizationConfig for double-sided rendering
         */
        static inline RasterizationConfig NoCull()
        {
            RasterizationConfig config;
            config.fillMode     = RasterizeFillMode::Solid;
            config.cullMode     = RasterizeCullMode::None;
            config.windingOrder = RasterizeWindingOrder::CounterClockwise;
            return config;
        }

        /**
         * @brief Front-face culling
         * @return RasterizationConfig for front-face culling
         */
        static inline RasterizationConfig CullFront()
        {
            RasterizationConfig config;
            config.fillMode     = RasterizeFillMode::Solid;
            config.cullMode     = RasterizeCullMode::FrontFace;
            config.windingOrder = RasterizeWindingOrder::CounterClockwise;
            return config;
        }

        /**
         * @brief Wireframe rendering without culling
         * @return RasterizationConfig for wireframe debug rendering
         */
        static inline RasterizationConfig Wireframe()
        {
            RasterizationConfig config;
            config.fillMode     = RasterizeFillMode::Wireframe;
            config.cullMode     = RasterizeCullMode::None;
            config.windingOrder = RasterizeWindingOrder::CounterClockwise;
            return config;
        }

        /**
         * @brief Wireframe with back-face culling
         * @return RasterizationConfig for optimized wireframe rendering
         */
        static inline RasterizationConfig WireframeCullBack()
        {
            RasterizationConfig config;
            config.fillMode     = RasterizeFillMode::Wireframe;
            config.cullMode     = RasterizeCullMode::BackFace;
            config.windingOrder = RasterizeWindingOrder::CounterClockwise;
            return config;
        }

        /**
         * @brief Equality comparison operator for PSO caching
         */
        bool operator==(const RasterizationConfig& other) const
        {
            return fillMode == other.fillMode &&
                cullMode == other.cullMode &&
                windingOrder == other.windingOrder &&
                depthBias == other.depthBias &&
                depthBiasClamp == other.depthBiasClamp &&
                slopeScaledDepthBias == other.slopeScaledDepthBias &&
                depthClipEnabled == other.depthClipEnabled &&
                multisampleEnabled == other.multisampleEnabled &&
                antialiasedLineEnabled == other.antialiasedLineEnabled &&
                conservativeRasterEnabled == other.conservativeRasterEnabled &&
                forcedSampleCount == other.forcedSampleCount;
        }
    };
} // namespace enigma::graphic
