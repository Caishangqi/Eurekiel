#pragma once

// ============================================================================
// SamplerProviderCommon.hpp - [NEW] Common definitions for Sampler Provider module
// Part of Dynamic Sampler System
// ============================================================================

#include "Engine/Core/LogCategory/LogCategory.hpp"
#include <cstdint>

namespace enigma::graphic
{
    // ========================================================================
    // Log Category Declaration
    // ========================================================================

    DECLARE_LOG_CATEGORY_EXTERN(LogSamplerProvider);

    // ========================================================================
    // Sampler Provider Constants
    // ========================================================================

    // Sampler slot count (matches SamplerIndicesBuffer capacity)
    constexpr uint32_t MAX_SAMPLERS = 16; // sampler0-15

    // cbuffer register slot (must match HLSL declaration)
    constexpr uint32_t SLOT_SAMPLER_INDICES = 8; // register(b8)

    // Invalid sampler index marker
    constexpr uint32_t INVALID_SAMPLER_INDEX = 0xFFFFFFFF;
} // namespace enigma::graphic
