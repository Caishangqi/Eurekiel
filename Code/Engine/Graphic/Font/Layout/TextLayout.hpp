#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "Engine/Graphic/Font/Atlas/GlyphAtlas.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/Vec2.hpp"

namespace enigma::graphic
{
    enum class TextLayoutMissingPolicy
    {
        UseAtlasFallback,
        Strict
    };

    struct TextLayoutSettings
    {
        Vec2                    origin;
        TextLayoutMissingPolicy missingPolicy = TextLayoutMissingPolicy::UseAtlasFallback;
    };

    struct TextLayoutGlyphQuad
    {
        uint32_t codepoint        = 0;
        uint32_t glyphIndex       = 0;
        size_t   sourceByteOffset = 0;
        Vec2     baselinePosition;
        AABB2    localBounds;
        AABB2    uvBounds;
        float    advanceX         = 0.0f;
        bool     hasVisiblePixels = false;
        bool     isFallbackGlyph  = false;
    };

    struct TextLayoutResult
    {
        std::vector<TextLayoutGlyphQuad> glyphs;
        AABB2                            visibleBounds;
        float                            advanceWidth = 0.0f;
        float                            lineHeight   = 0.0f;
        int                              lineCount    = 0;
    };

    class TextLayout final
    {
    public:
        TextLayout();
        ~TextLayout();

        TextLayout(const TextLayout&)                = delete;
        TextLayout& operator=(const TextLayout&)     = delete;
        TextLayout(TextLayout&&) noexcept;
        TextLayout& operator=(TextLayout&&) noexcept;

        static TextLayoutResult Build(
            std::string_view         utf8Text,
            const GlyphAtlas&        atlas,
            const TextLayoutSettings& settings = TextLayoutSettings{});
    };
} // namespace enigma::graphic
