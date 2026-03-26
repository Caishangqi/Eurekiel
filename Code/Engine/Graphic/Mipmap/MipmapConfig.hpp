#pragma once
#include "MipmapCommon.hpp"
#include <cstdint>
#include <climits>

namespace enigma::graphic
{
    // Forward declaration - compute shader program for custom downsampling
    class ShaderProgram;

    // ============================================================================
    // Mip Count Utility
    // ============================================================================

    // Calculate full mip chain count: floor(log2(max(w,h))) + 1
    // Example: 1920x1080 -> 11 mip levels (1920 -> 960 -> ... -> 1)
    static inline uint32_t CalculateMipCount(uint32_t width, uint32_t height)
    {
        uint32_t maxDim = (width > height) ? width : height;
        uint32_t mipCount = 1;
        while (maxDim > 1)
        {
            maxDim >>= 1;
            mipCount++;
        }
        return mipCount;
    }

    // ============================================================================
    // MipmapConfig - Configuration for mipmap generation
    // Follows BlendState/DepthState static factory pattern
    // ============================================================================

    struct MipmapConfig
    {
        // Which compute shader to use for downsampling
        // nullptr = use engine built-in shader selected by filterMode
        ShaderProgram* computeShader = nullptr;

        // Filter mode for built-in shader selection (defined in MipmapCommon.hpp)
        // Custom ShaderProgram (via computeShader) bypasses this entirely
        MipFilterMode filterMode = MipFilterMode::Box;

        // Mip range control
        uint32_t startMipLevel = 0;          // Start from this mip (usually 0)
        uint32_t mipLevelCount = UINT32_MAX; // Generate all remaining mips

        // Sampler slot index (0-15) used for downsampling filter
        // Maps to SamplerProvider slot -> SamplerDescriptorHeap[bindlessIndex]
        // Default: slot 0 (linear clamp sampler)
        uint32_t samplerSlot = 0;

        // --- Static factory presets ---

        // Default box filter (bilinear sampling = free 2x2 average)
        static inline MipmapConfig Default()
        {
            return MipmapConfig{};
        }

        // Custom compute shader (advanced users - Kaiser, Lanczos, etc.)
        static inline MipmapConfig Custom(ShaderProgram* shader)
        {
            MipmapConfig config;
            config.computeShader = shader;
            return config;
        }

        // Partial mip generation (e.g., only first 4 levels)
        static inline MipmapConfig Partial(uint32_t startMip, uint32_t count)
        {
            MipmapConfig config;
            config.startMipLevel = startMip;
            config.mipLevelCount = count;
            return config;
        }

        // Specific sampler slot for downsampling
        static inline MipmapConfig WithSampler(uint32_t slot)
        {
            MipmapConfig config;
            config.samplerSlot = slot;
            return config;
        }

        // Alpha-weighted downsampling (prevents cutout texture fringe)
        static inline MipmapConfig AlphaWeighted()
        {
            MipmapConfig config;
            config.filterMode = MipFilterMode::AlphaWeighted;
            return config;
        }

        // sRGB-correct downsampling (prevents color darkening at distance)
        static inline MipmapConfig Srgb()
        {
            MipmapConfig config;
            config.filterMode = MipFilterMode::SRGB;
            return config;
        }

        // Alpha-weighted + sRGB (highest quality, best for cutout textures)
        static inline MipmapConfig AlphaWeightedSrgb()
        {
            MipmapConfig config;
            config.filterMode = MipFilterMode::AlphaWeightedSRGB;
            return config;
        }

        // --- Comparison operators (for PSO caching) ---

        bool operator==(const MipmapConfig& other) const
        {
            return computeShader == other.computeShader
                && filterMode == other.filterMode
                && startMipLevel == other.startMipLevel
                && mipLevelCount == other.mipLevelCount
                && samplerSlot == other.samplerSlot;
        }

        bool operator!=(const MipmapConfig& other) const
        {
            return !(*this == other);
        }
    };
}
