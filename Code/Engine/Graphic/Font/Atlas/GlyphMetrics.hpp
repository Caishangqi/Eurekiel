#pragma once

#include <cstdint>

namespace enigma::graphic
{
    struct FontVerticalMetrics
    {
        float pixelHeight = 0.0f;
        float ascent      = 0.0f;
        float descent     = 0.0f;
        float lineGap     = 0.0f;
        float lineHeight  = 0.0f;
    };

    struct GlyphLookupResult
    {
        uint32_t codepoint       = 0;
        uint32_t glyphIndex      = 0;
        bool     found           = false;
        bool     isFallbackGlyph = false;
    };

    struct GlyphMetrics
    {
        uint32_t codepoint  = 0;
        uint32_t glyphIndex = 0;
        bool     found      = false;
        float    advanceX   = 0.0f;
        float    bearingX   = 0.0f;
        float    bearingY   = 0.0f;
        float    width      = 0.0f;
        float    height     = 0.0f;
    };
} // namespace enigma::graphic
