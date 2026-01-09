#pragma once

// ============================================================================
// RenderTargetProviderCommon.hpp - [NEW] Common definitions for RenderTarget Provider module
// Part of RenderTarget Manager Unified Architecture Refactoring
// ============================================================================

#include "Engine/Core/LogCategory/LogCategory.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // Log Category Declaration
    // ========================================================================

    DECLARE_LOG_CATEGORY_EXTERN(LogRenderTargetProvider);

    // ========================================================================
    // [NEW] RT Provider Constants - Shader RT Fetching Feature
    // ========================================================================

    // RT slot count constants (Provider capacity)
    constexpr int MAX_COLOR_TEXTURES  = 16; // colortex0-15
    constexpr int MIN_COLOR_TEXTURES  = 1;
    constexpr int MAX_DEPTH_TEXTURES  = 3; // depthtex0-2 (Iris spec)
    constexpr int MIN_DEPTH_TEXTURES  = 1;
    constexpr int MAX_SHADOW_COLORS   = 8; // shadowcolor0-7
    constexpr int MIN_SHADOW_COLORS   = 1;
    constexpr int MAX_SHADOW_TEXTURES = 2; // shadowtex0-1
    constexpr int MIN_SHADOW_TEXTURES = 1;

    // cbuffer array size constants (HLSL buffer capacity)
    constexpr uint32_t CBUFFER_COLOR_TARGETS_SIZE   = 16; // colortex cbuffer array size
    constexpr uint32_t CBUFFER_DEPTH_TEXTURES_SIZE  = 16; // depthtex cbuffer array size
    constexpr uint32_t CBUFFER_SHADOW_COLORS_SIZE   = 8; // shadowcolor cbuffer array size
    constexpr uint32_t CBUFFER_SHADOW_TEXTURES_SIZE = 2; // shadowtex cbuffer array size

    // cbuffer register slot constants (must match HLSL declarations)
    constexpr uint32_t SLOT_COLOR_TARGETS   = 3; // register(b3)
    constexpr uint32_t SLOT_DEPTH_TEXTURES  = 4; // register(b4)
    constexpr uint32_t SLOT_SHADOW_COLOR    = 5; // register(b5)
    constexpr uint32_t SLOT_SHADOW_TEXTURES = 6; // register(b6)

    // Invalid bindless index marker
    constexpr uint32_t INVALID_BINDLESS_INDEX = 0xFFFFFFFF;
} // namespace enigma::graphic
