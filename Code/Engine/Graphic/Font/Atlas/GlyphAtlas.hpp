#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Engine/Graphic/Font/Atlas/GlyphMetrics.hpp"
#include "Engine/Graphic/Font/TrueType/TrueTypeFont.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/IntVec2.hpp"

namespace enigma::graphic
{
    enum class GlyphAtlasPixelFormat
    {
        AlphaCoverage8
    };

    enum class GlyphMissingPolicy
    {
        UseFallbackGlyph,
        Strict
    };

    struct GlyphAtlasBuildSettings
    {
        float              pixelHeight   = 0.0f;
        int                padding       = 1;
        IntVec2            maxAtlasSize  = IntVec2(2048, 2048);
        GlyphMissingPolicy missingPolicy = GlyphMissingPolicy::UseFallbackGlyph;
    };

    struct GlyphAtlasEntry
    {
        uint32_t codepoint       = 0;
        uint32_t glyphIndex      = 0;
        bool     found           = false;
        bool     isFallbackGlyph = false;

        GlyphMetrics metrics;
        IntVec2      atlasPosition;
        IntVec2      bitmapSize;
        IntVec2      bitmapOffset;
        AABB2        uvBounds;
        bool         hasVisiblePixels = false;
    };

    struct GlyphAtlasDiagnostics
    {
        IntVec2               dimensions;
        int                   stride            = 0;
        float                 pixelHeight       = 0.0f;
        GlyphAtlasPixelFormat pixelFormat       = GlyphAtlasPixelFormat::AlphaCoverage8;
        size_t                glyphCount        = 0;
        size_t                visibleGlyphCount = 0;
        size_t                missingGlyphCount = 0;
    };

    class GlyphAtlas final
    {
    public:
        GlyphAtlas();
        ~GlyphAtlas();

        GlyphAtlas(const GlyphAtlas&)                = delete;
        GlyphAtlas& operator=(const GlyphAtlas&)     = delete;
        GlyphAtlas(GlyphAtlas&&) noexcept;
        GlyphAtlas& operator=(GlyphAtlas&&) noexcept;

        static GlyphAtlas BuildFromCodepoints(
            const TrueTypeFont&             font,
            const std::vector<uint32_t>&    codepoints,
            const GlyphAtlasBuildSettings&  settings);

        static GlyphAtlas BuildFromUtf8Text(
            const TrueTypeFont&            font,
            std::string_view               utf8Text,
            const GlyphAtlasBuildSettings& settings);

        bool IsBuilt() const noexcept;

        const FontVerticalMetrics&          GetVerticalMetrics() const;
        const GlyphAtlasDiagnostics&        GetDiagnostics() const;
        const std::vector<uint8_t>&         GetPixels() const;
        const std::vector<GlyphAtlasEntry>& GetEntries() const;
        const GlyphAtlasEntry*              FindEntry(uint32_t codepoint) const;
        const GlyphAtlasEntry*              FindFallbackEntry() const;

    private:
        bool                                    m_isBuilt = false;
        FontVerticalMetrics                     m_verticalMetrics;
        GlyphAtlasDiagnostics                   m_diagnostics;
        std::vector<uint8_t>                    m_pixels;
        std::vector<GlyphAtlasEntry>            m_entries;
        std::unordered_map<uint32_t, size_t>    m_codepointToEntryIndex;
        size_t                                  m_fallbackEntryIndex = static_cast<size_t>(-1);
    };
} // namespace enigma::graphic
