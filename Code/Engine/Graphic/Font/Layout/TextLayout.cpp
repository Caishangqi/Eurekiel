#include "Engine/Graphic/Font/Layout/TextLayout.hpp"

#include <algorithm>
#include <limits>
#include <string>

#include "Engine/Graphic/Font/Exception/FontException.hpp"
#include "Engine/Graphic/Font/Layout/Utf8Text.hpp"

namespace enigma::graphic
{
    namespace
    {
        bool IsNewline(uint32_t codepoint)
        {
            return codepoint == static_cast<uint32_t>('\n');
        }

        const GlyphAtlasEntry& ResolveEntry(
            const GlyphAtlas&         atlas,
            uint32_t                  codepoint,
            TextLayoutMissingPolicy   missingPolicy,
            bool&                     usedFallback)
        {
            if (const GlyphAtlasEntry* entry = atlas.FindEntry(codepoint))
            {
                usedFallback = false;
                return *entry;
            }

            if (missingPolicy == TextLayoutMissingPolicy::UseAtlasFallback)
            {
                if (const GlyphAtlasEntry* fallback = atlas.FindFallbackEntry())
                {
                    usedFallback = true;
                    return *fallback;
                }
            }

            throw FontConfigurationException(
                "Text layout codepoint was not built into the glyph atlas: " + std::to_string(codepoint));
        }

        AABB2 BuildLocalBounds(float penX, float baselineY, const GlyphAtlasEntry& entry)
        {
            const float left   = penX + static_cast<float>(entry.bitmapOffset.x);
            const float top    = baselineY - static_cast<float>(entry.bitmapOffset.y);
            const float right  = left + static_cast<float>(entry.bitmapSize.x);
            const float bottom = top - static_cast<float>(entry.bitmapSize.y);

            return AABB2(left, bottom, right, top);
        }

        void ExpandVisibleBounds(AABB2& bounds, const AABB2& glyphBounds, bool& hasVisibleBounds)
        {
            if (!hasVisibleBounds)
            {
                bounds           = glyphBounds;
                hasVisibleBounds = true;
                return;
            }

            bounds.m_mins.x = std::min(bounds.m_mins.x, glyphBounds.m_mins.x);
            bounds.m_mins.y = std::min(bounds.m_mins.y, glyphBounds.m_mins.y);
            bounds.m_maxs.x = std::max(bounds.m_maxs.x, glyphBounds.m_maxs.x);
            bounds.m_maxs.y = std::max(bounds.m_maxs.y, glyphBounds.m_maxs.y);
        }
    } // namespace

    TextLayout::TextLayout() = default;

    TextLayout::~TextLayout() = default;

    TextLayout::TextLayout(TextLayout&&) noexcept = default;

    TextLayout& TextLayout::operator=(TextLayout&&) noexcept = default;

    TextLayoutResult TextLayout::Build(
        std::string_view          utf8Text,
        const GlyphAtlas&         atlas,
        const TextLayoutSettings& settings)
    {
        if (!atlas.IsBuilt())
        {
            throw FontConfigurationException("Cannot build text layout from an unbuilt glyph atlas");
        }

        const std::vector<DecodedCodepoint> decodedText = DecodeUtf8Text(utf8Text);
        const FontVerticalMetrics&          vertical    = atlas.GetVerticalMetrics();

        TextLayoutResult result;
        result.lineHeight = vertical.lineHeight;
        result.lineCount  = 1;
        result.glyphs.reserve(decodedText.size());

        float penX      = settings.origin.x;
        float baselineY = settings.origin.y;

        AABB2 visibleBounds(0.0f, 0.0f, 0.0f, 0.0f);
        bool  hasVisibleBounds = false;

        for (const DecodedCodepoint& decoded : decodedText)
        {
            if (IsNewline(decoded.codepoint))
            {
                result.advanceWidth = std::max(result.advanceWidth, penX - settings.origin.x);
                penX                = settings.origin.x;
                baselineY -= result.lineHeight;
                ++result.lineCount;
                continue;
            }

            bool                   usedFallback = false;
            const GlyphAtlasEntry& entry        =
                ResolveEntry(atlas, decoded.codepoint, settings.missingPolicy, usedFallback);

            TextLayoutGlyphQuad quad;
            quad.codepoint        = decoded.codepoint;
            quad.glyphIndex       = entry.glyphIndex;
            quad.sourceByteOffset = decoded.byteOffset;
            quad.baselinePosition = Vec2(penX, baselineY);
            quad.uvBounds         = entry.uvBounds;
            quad.advanceX         = entry.metrics.advanceX;
            quad.hasVisiblePixels = entry.hasVisiblePixels;
            quad.isFallbackGlyph  = usedFallback || entry.isFallbackGlyph;

            if (entry.hasVisiblePixels)
            {
                quad.localBounds = BuildLocalBounds(penX, baselineY, entry);
                ExpandVisibleBounds(visibleBounds, quad.localBounds, hasVisibleBounds);
            }

            result.glyphs.push_back(quad);
            penX += entry.metrics.advanceX;
        }

        result.advanceWidth = std::max(result.advanceWidth, penX - settings.origin.x);
        result.visibleBounds = hasVisibleBounds ? visibleBounds : AABB2(0.0f, 0.0f, 0.0f, 0.0f);

        return result;
    }
} // namespace enigma::graphic
