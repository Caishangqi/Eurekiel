#pragma once

#include <cstdint>

namespace enigma::graphic
{
    struct GlyphMetrics
    {
        uint32_t glyphIndex = 0;
        float    advanceX   = 0.0f;
        float    bearingX   = 0.0f;
        float    bearingY   = 0.0f;
        float    width      = 0.0f;
        float    height     = 0.0f;
    };
} // namespace enigma::graphic
