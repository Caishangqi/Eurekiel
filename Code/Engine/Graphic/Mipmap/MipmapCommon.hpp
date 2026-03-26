#pragma once

// ============================================================================
// MipmapCommon.hpp - Shared mipmap definitions (enums, shader paths)
//
// Central location for FilterMode enum and shader path registry.
// Used by MipmapConfig (API layer) and MipmapGenerator (implementation).
// Adding a new filter: add enum value + path entry here + new .cs.hlsl file.
// ============================================================================

namespace enigma::graphic
{
    /// Mipmap downsampling filter mode
    enum class MipFilterMode
    {
        Box,                 // Naive bilinear 2x2 average (fast, default)
        AlphaWeighted,       // Alpha-premultiplied average (prevents cutout fringe)
        SRGB,                // sRGB-correct average (prevents color darkening)
        AlphaWeightedSRGB,   // Combined: alpha-weighted + sRGB (highest quality)

        COUNT                // Must be last
    };

    /// Compile-time shader path registry indexed by MipFilterMode
    /// Paths are relative to working directory (Run/)
    static constexpr const wchar_t* MIP_FILTER_SHADER_PATHS[] =
    {
        L".enigma/assets/engine/shaders/core/mipmapGeneration.cs.hlsl",                  // Box
        L".enigma/assets/engine/shaders/core/mipmapGeneration_alphaWeighted.cs.hlsl",     // AlphaWeighted
        L".enigma/assets/engine/shaders/core/mipmapGeneration_srgb.cs.hlsl",              // SRGB
        L".enigma/assets/engine/shaders/core/mipmapGeneration_alphaWeightedSrgb.cs.hlsl", // AlphaWeightedSRGB
    };

    static_assert(
        sizeof(MIP_FILTER_SHADER_PATHS) / sizeof(MIP_FILTER_SHADER_PATHS[0]) == static_cast<int>(MipFilterMode::COUNT),
        "MIP_FILTER_SHADER_PATHS must match MipFilterMode::COUNT");

} // namespace enigma::graphic
