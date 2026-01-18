/**
 * @file SamplerConfig.hpp
 * @brief [NEW] Sampler configuration struct for Dynamic Sampler System
 * 
 * Design Pattern: Following RasterizationConfig pattern
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
    // Sampler Configuration Type Aliases
    // ========================================

    using SamplerFilter     = D3D12_FILTER;
    using SamplerAddress    = D3D12_TEXTURE_ADDRESS_MODE;
    using SamplerComparison = D3D12_COMPARISON_FUNC;

    // ========================================
    // Sampler Filter Mode Constants
    // ========================================

    namespace SamplerFilterMode
    {
        constexpr SamplerFilter Point            = D3D12_FILTER_MIN_MAG_MIP_POINT;
        constexpr SamplerFilter Linear           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        constexpr SamplerFilter Anisotropic      = D3D12_FILTER_ANISOTROPIC;
        constexpr SamplerFilter ComparisonLinear = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        constexpr SamplerFilter ComparisonPoint  = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
    }

    // ========================================
    // Sampler Address Mode Constants
    // ========================================

    namespace SamplerAddressMode
    {
        constexpr SamplerAddress Wrap   = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        constexpr SamplerAddress Clamp  = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        constexpr SamplerAddress Mirror = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        constexpr SamplerAddress Border = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    }

    // ========================================
    // Sampler Comparison Function Constants
    // ========================================

    namespace SamplerComparisonFunc
    {
        constexpr SamplerComparison Never        = D3D12_COMPARISON_FUNC_NEVER;
        constexpr SamplerComparison Less         = D3D12_COMPARISON_FUNC_LESS;
        constexpr SamplerComparison Equal        = D3D12_COMPARISON_FUNC_EQUAL;
        constexpr SamplerComparison LessEqual    = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        constexpr SamplerComparison Greater      = D3D12_COMPARISON_FUNC_GREATER;
        constexpr SamplerComparison NotEqual     = D3D12_COMPARISON_FUNC_NOT_EQUAL;
        constexpr SamplerComparison GreaterEqual = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        constexpr SamplerComparison Always       = D3D12_COMPARISON_FUNC_ALWAYS;
    }

    // ========================================
    // Sampler Border Color Constants
    // ========================================

    namespace SamplerBorderColor
    {
        constexpr D3D12_STATIC_BORDER_COLOR TransparentBlack = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        constexpr D3D12_STATIC_BORDER_COLOR OpaqueBlack      = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
        constexpr D3D12_STATIC_BORDER_COLOR OpaqueWhite      = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    }

    /**
     * @brief Sampler configuration structure for Dynamic Sampler System
     * @details
     * Maps to D3D12_SAMPLER_DESC for bindless sampler creation.
     * Supports all standard sampler parameters with static presets.
     */
    struct SamplerConfig
    {
        // [KEY] Filter and address modes
        SamplerFilter  filter   = SamplerFilterMode::Linear;
        SamplerAddress addressU = SamplerAddressMode::Clamp;
        SamplerAddress addressV = SamplerAddressMode::Clamp;
        SamplerAddress addressW = SamplerAddressMode::Clamp;

        // [KEY] Anisotropic filtering
        uint32_t maxAnisotropy = 1;

        // [KEY] Comparison function (for shadow sampling)
        SamplerComparison comparisonFunc = SamplerComparisonFunc::Never;

        // [KEY] Border color (when using Border address mode)
        float borderColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        // [KEY] LOD configuration
        float minLOD     = 0.0f;
        float maxLOD     = D3D12_FLOAT32_MAX;
        float mipLODBias = 0.0f;

        // ========================================
        // Static Preset Methods
        // ========================================

        /**
         * @brief Linear filtering with wrap addressing (default)
         * [FIX] Changed from Clamp to Wrap to match original Static Sampler configuration
         * Original: D3D12_TEXTURE_ADDRESS_MODE_WRAP (for seamless texture tiling)
         */
        static inline SamplerConfig Linear()
        {
            SamplerConfig config;
            config.filter   = SamplerFilterMode::Linear;
            config.addressU = SamplerAddressMode::Wrap;
            config.addressV = SamplerAddressMode::Wrap;
            config.addressW = SamplerAddressMode::Wrap;
            return config;
        }

        /**
         * @brief Point filtering with clamp addressing
         */
        static inline SamplerConfig Point()
        {
            SamplerConfig config;
            config.filter   = SamplerFilterMode::Point;
            config.addressU = SamplerAddressMode::Clamp;
            config.addressV = SamplerAddressMode::Clamp;
            config.addressW = SamplerAddressMode::Clamp;
            return config;
        }

        /**
         * @brief Shadow sampler with comparison function
         */
        static inline SamplerConfig Shadow()
        {
            SamplerConfig config;
            config.filter         = SamplerFilterMode::ComparisonLinear;
            config.addressU       = SamplerAddressMode::Border;
            config.addressV       = SamplerAddressMode::Border;
            config.addressW       = SamplerAddressMode::Border;
            config.comparisonFunc = SamplerComparisonFunc::LessEqual;
            config.borderColor[0] = 1.0f;
            config.borderColor[1] = 1.0f;
            config.borderColor[2] = 1.0f;
            config.borderColor[3] = 1.0f;
            return config;
        }

        /**
         * @brief Point filtering with wrap addressing
         */
        static inline SamplerConfig PointWrap()
        {
            SamplerConfig config;
            config.filter   = SamplerFilterMode::Point;
            config.addressU = SamplerAddressMode::Wrap;
            config.addressV = SamplerAddressMode::Wrap;
            config.addressW = SamplerAddressMode::Wrap;
            return config;
        }

        /**
         * @brief Linear filtering with wrap addressing
         */
        static inline SamplerConfig LinearWrap()
        {
            SamplerConfig config;
            config.filter   = SamplerFilterMode::Linear;
            config.addressU = SamplerAddressMode::Wrap;
            config.addressV = SamplerAddressMode::Wrap;
            config.addressW = SamplerAddressMode::Wrap;
            return config;
        }

        /**
         * @brief Anisotropic filtering with specified level
         * @param level Anisotropy level (1-16)
         */
        static inline SamplerConfig Anisotropic(uint32_t level = 16)
        {
            SamplerConfig config;
            config.filter        = SamplerFilterMode::Anisotropic;
            config.addressU      = SamplerAddressMode::Wrap;
            config.addressV      = SamplerAddressMode::Wrap;
            config.addressW      = SamplerAddressMode::Wrap;
            config.maxAnisotropy = (level > 16) ? 16 : ((level < 1) ? 1 : level);
            return config;
        }

        /**
         * @brief Equality comparison operator for PSO caching
         */
        bool operator==(const SamplerConfig& other) const
        {
            return filter == other.filter &&
                addressU == other.addressU &&
                addressV == other.addressV &&
                addressW == other.addressW &&
                maxAnisotropy == other.maxAnisotropy &&
                comparisonFunc == other.comparisonFunc &&
                borderColor[0] == other.borderColor[0] &&
                borderColor[1] == other.borderColor[1] &&
                borderColor[2] == other.borderColor[2] &&
                borderColor[3] == other.borderColor[3] &&
                minLOD == other.minLOD &&
                maxLOD == other.maxLOD &&
                mipLODBias == other.mipLODBias;
        }

        bool operator!=(const SamplerConfig& other) const
        {
            return !(*this == other);
        }
    };
} // namespace enigma::graphic
