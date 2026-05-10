#pragma once

#include <cstdint>
#include <vector>

namespace enigma::graphic
{
    struct GlyphBitmap
    {
        uint32_t             codepoint   = 0;
        uint32_t             glyphIndex  = 0;
        float                pixelHeight = 0.0f;
        int                  width       = 0;
        int                  height      = 0;
        int                  offsetX     = 0;
        int                  offsetY     = 0;
        int                  stride      = 0;
        std::vector<uint8_t> pixels;
    };
} // namespace enigma::graphic
