#include "Engine/Graphic/Font/Atlas/GlyphAtlas.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include "Engine/Graphic/Font/Atlas/RectanglePacker.hpp"
#include "Engine/Graphic/Font/Exception/FontException.hpp"
#include "Engine/Graphic/Font/Layout/Utf8Text.hpp"

namespace enigma::graphic
{
    namespace
    {
        constexpr size_t kInvalidEntryIndex = static_cast<size_t>(-1);

        struct PendingGlyph
        {
            GlyphAtlasEntry entry;
            GlyphBitmap     bitmap;
        };

        bool ShouldSkipAtlasCodepoint(uint32_t codepoint)
        {
            return codepoint == static_cast<uint32_t>('\n') ||
                   codepoint == static_cast<uint32_t>('\r');
        }

        void ValidateBuildSettings(const TrueTypeFont& font, const GlyphAtlasBuildSettings& settings)
        {
            if (!font.IsLoaded())
            {
                throw FontConfigurationException("Cannot build glyph atlas from an unloaded TrueType font");
            }

            if (!std::isfinite(settings.pixelHeight) || settings.pixelHeight <= 0.0f)
            {
                throw FontConfigurationException(
                    "Glyph atlas pixel height must be positive and finite: " + std::to_string(settings.pixelHeight));
            }

            if (settings.padding < 0)
            {
                throw FontConfigurationException(
                    "Glyph atlas padding must be non-negative: " + std::to_string(settings.padding));
            }

            if (settings.maxAtlasSize.x <= 0 || settings.maxAtlasSize.y <= 0)
            {
                throw FontConfigurationException(
                    "Glyph atlas max size must be positive: " + settings.maxAtlasSize.toString());
            }
        }

        std::vector<uint32_t> DeduplicateCodepoints(const std::vector<uint32_t>& codepoints)
        {
            std::vector<uint32_t> result;
            result.reserve(codepoints.size());

            for (uint32_t codepoint : codepoints)
            {
                if (ShouldSkipAtlasCodepoint(codepoint))
                {
                    continue;
                }

                if (std::find(result.begin(), result.end(), codepoint) == result.end())
                {
                    result.push_back(codepoint);
                }
            }

            return result;
        }

        std::string BuildAtlasContext(const TrueTypeFont& font, const GlyphAtlasBuildSettings& settings)
        {
            return "path='" + font.GetFilePath().string() +
                   "', pixelHeight=" + std::to_string(settings.pixelHeight) +
                   ", maxAtlasSize=" + settings.maxAtlasSize.toString();
        }

        PendingGlyph BuildPendingGlyph(
            const TrueTypeFont&            font,
            uint32_t                       codepoint,
            const GlyphAtlasBuildSettings& settings)
        {
            GlyphLookupResult lookup = font.FindGlyph(codepoint);
            if (!lookup.found && settings.missingPolicy == GlyphMissingPolicy::Strict)
            {
                throw AtlasBuildException(
                    "Missing glyph U+" + std::to_string(codepoint) +
                    " while building strict glyph atlas for " + BuildAtlasContext(font, settings));
            }

            GlyphMetrics metrics = font.GetGlyphMetrics(codepoint, settings.pixelHeight);
            GlyphBitmap  bitmap  = font.RasterizeGlyph(codepoint, settings.pixelHeight);

            PendingGlyph pending;
            pending.bitmap = std::move(bitmap);

            pending.entry.codepoint       = codepoint;
            pending.entry.glyphIndex      = lookup.glyphIndex;
            pending.entry.found           = lookup.found;
            pending.entry.isFallbackGlyph = lookup.isFallbackGlyph;
            pending.entry.metrics         = metrics;
            pending.entry.bitmapSize      = IntVec2(pending.bitmap.width, pending.bitmap.height);
            pending.entry.bitmapOffset    = IntVec2(pending.bitmap.offsetX, pending.bitmap.offsetY);
            pending.entry.hasVisiblePixels =
                pending.bitmap.width > 0 &&
                pending.bitmap.height > 0 &&
                !pending.bitmap.pixels.empty();

            return pending;
        }

        RectanglePackResult PackVisibleGlyphs(
            const std::vector<PendingGlyph>& pendingGlyphs,
            const GlyphAtlasBuildSettings&   settings)
        {
            std::vector<RectanglePackItem> packItems;
            for (size_t index = 0; index < pendingGlyphs.size(); ++index)
            {
                const GlyphAtlasEntry& entry = pendingGlyphs[index].entry;
                if (!entry.hasVisiblePixels)
                {
                    continue;
                }

                packItems.push_back({
                    static_cast<uint32_t>(index),
                    IntVec2(
                        entry.bitmapSize.x + settings.padding * 2,
                        entry.bitmapSize.y + settings.padding * 2)
                });
            }

            if (packItems.empty())
            {
                return {};
            }

            RectanglePackSettings packSettings;
            packSettings.maxSize              = settings.maxAtlasSize;
            packSettings.powerOfTwoDimensions = true;

            try
            {
                return PackRectangles(packItems, packSettings);
            }
            catch (const FontException& exception)
            {
                throw AtlasBuildException(
                    "Failed to pack glyph atlas with " + std::to_string(packItems.size()) +
                    " visible glyphs: " + exception.what());
            }
        }

        const PackedRectangle* FindPackedRect(const RectanglePackResult& packResult, size_t pendingIndex)
        {
            const uint32_t id = static_cast<uint32_t>(pendingIndex);
            for (const PackedRectangle& rect : packResult.rectangles)
            {
                if (rect.id == id)
                {
                    return &rect;
                }
            }

            return nullptr;
        }

        AABB2 CalculateUvBounds(const IntVec2& position, const IntVec2& size, const IntVec2& atlasDimensions)
        {
            const float invWidth  = 1.0f / static_cast<float>(atlasDimensions.x);
            const float invHeight = 1.0f / static_cast<float>(atlasDimensions.y);

            return AABB2(
                static_cast<float>(position.x) * invWidth,
                static_cast<float>(position.y) * invHeight,
                static_cast<float>(position.x + size.x) * invWidth,
                static_cast<float>(position.y + size.y) * invHeight);
        }
    } // namespace

    GlyphAtlas::GlyphAtlas() = default;

    GlyphAtlas::~GlyphAtlas() = default;

    GlyphAtlas::GlyphAtlas(GlyphAtlas&&) noexcept = default;

    GlyphAtlas& GlyphAtlas::operator=(GlyphAtlas&&) noexcept = default;

    GlyphAtlas GlyphAtlas::BuildFromCodepoints(
        const TrueTypeFont&            font,
        const std::vector<uint32_t>&   codepoints,
        const GlyphAtlasBuildSettings& settings)
    {
        ValidateBuildSettings(font, settings);

        std::vector<uint32_t> uniqueCodepoints = DeduplicateCodepoints(codepoints);
        if (uniqueCodepoints.empty())
        {
            throw FontConfigurationException("Cannot build glyph atlas with no glyph codepoints");
        }

        std::vector<PendingGlyph> pendingGlyphs;
        pendingGlyphs.reserve(uniqueCodepoints.size());

        GlyphAtlas atlas;
        atlas.m_verticalMetrics = font.GetVerticalMetrics(settings.pixelHeight);

        for (uint32_t codepoint : uniqueCodepoints)
        {
            PendingGlyph pending = BuildPendingGlyph(font, codepoint, settings);
            if (!pending.entry.found)
            {
                ++atlas.m_diagnostics.missingGlyphCount;
            }
            pendingGlyphs.push_back(std::move(pending));
        }

        RectanglePackResult packResult = PackVisibleGlyphs(pendingGlyphs, settings);

        atlas.m_diagnostics.dimensions  = packResult.dimensions;
        atlas.m_diagnostics.stride      = packResult.dimensions.x;
        atlas.m_diagnostics.pixelHeight = settings.pixelHeight;
        atlas.m_diagnostics.pixelFormat = GlyphAtlasPixelFormat::AlphaCoverage8;
        atlas.m_diagnostics.glyphCount  = pendingGlyphs.size();

        if (packResult.dimensions.x > 0 && packResult.dimensions.y > 0)
        {
            const size_t pixelCount = static_cast<size_t>(packResult.dimensions.x) *
                                      static_cast<size_t>(packResult.dimensions.y);
            atlas.m_pixels.assign(pixelCount, 0u);
        }

        atlas.m_entries.reserve(pendingGlyphs.size());
        atlas.m_codepointToEntryIndex.reserve(pendingGlyphs.size());

        for (size_t index = 0; index < pendingGlyphs.size(); ++index)
        {
            PendingGlyph&     pending = pendingGlyphs[index];
            GlyphAtlasEntry&  entry   = pending.entry;
            const GlyphBitmap& bitmap = pending.bitmap;

            if (entry.hasVisiblePixels)
            {
                const PackedRectangle* packedRect = FindPackedRect(packResult, index);
                if (packedRect == nullptr)
                {
                    throw AtlasBuildException(
                        "Missing packed rectangle for glyph U+" + std::to_string(entry.codepoint));
                }

                entry.atlasPosition = packedRect->position + IntVec2(settings.padding, settings.padding);
                entry.uvBounds      = CalculateUvBounds(entry.atlasPosition, entry.bitmapSize, packResult.dimensions);

                for (int y = 0; y < bitmap.height; ++y)
                {
                    const size_t sourceOffset = static_cast<size_t>(y) * static_cast<size_t>(bitmap.stride);
                    const size_t destOffset   =
                        static_cast<size_t>(entry.atlasPosition.y + y) * static_cast<size_t>(atlas.m_diagnostics.stride) +
                        static_cast<size_t>(entry.atlasPosition.x);
                    for (int x = 0; x < bitmap.width; ++x)
                    {
                        atlas.m_pixels[destOffset + static_cast<size_t>(x)] =
                            bitmap.pixels[sourceOffset + static_cast<size_t>(x)];
                    }
                }

                ++atlas.m_diagnostics.visibleGlyphCount;
            }

            const size_t entryIndex = atlas.m_entries.size();
            if (entry.isFallbackGlyph || entry.glyphIndex == 0u)
            {
                atlas.m_fallbackEntryIndex = entryIndex;
            }

            atlas.m_codepointToEntryIndex[entry.codepoint] = entryIndex;
            atlas.m_entries.push_back(std::move(entry));
        }

        atlas.m_isBuilt = true;

        return atlas;
    }

    GlyphAtlas GlyphAtlas::BuildFromUtf8Text(
        const TrueTypeFont&            font,
        std::string_view               utf8Text,
        const GlyphAtlasBuildSettings& settings)
    {
        return BuildFromCodepoints(font, DecodeUniqueCodepoints(utf8Text), settings);
    }

    bool GlyphAtlas::IsBuilt() const noexcept
    {
        return m_isBuilt;
    }

    const FontVerticalMetrics& GlyphAtlas::GetVerticalMetrics() const
    {
        if (!m_isBuilt)
        {
            throw FontConfigurationException("Cannot query vertical metrics from an unbuilt glyph atlas");
        }

        return m_verticalMetrics;
    }

    const GlyphAtlasDiagnostics& GlyphAtlas::GetDiagnostics() const
    {
        if (!m_isBuilt)
        {
            throw FontConfigurationException("Cannot query diagnostics from an unbuilt glyph atlas");
        }

        return m_diagnostics;
    }

    const std::vector<uint8_t>& GlyphAtlas::GetPixels() const
    {
        if (!m_isBuilt)
        {
            throw FontConfigurationException("Cannot query pixels from an unbuilt glyph atlas");
        }

        return m_pixels;
    }

    const std::vector<GlyphAtlasEntry>& GlyphAtlas::GetEntries() const
    {
        if (!m_isBuilt)
        {
            throw FontConfigurationException("Cannot query entries from an unbuilt glyph atlas");
        }

        return m_entries;
    }

    const GlyphAtlasEntry* GlyphAtlas::FindEntry(uint32_t codepoint) const
    {
        if (!m_isBuilt)
        {
            throw FontConfigurationException("Cannot query glyph entry from an unbuilt glyph atlas");
        }

        const auto iter = m_codepointToEntryIndex.find(codepoint);
        if (iter == m_codepointToEntryIndex.end())
        {
            return nullptr;
        }

        return &m_entries[iter->second];
    }

    const GlyphAtlasEntry* GlyphAtlas::FindFallbackEntry() const
    {
        if (!m_isBuilt)
        {
            throw FontConfigurationException("Cannot query fallback entry from an unbuilt glyph atlas");
        }

        if (m_fallbackEntryIndex == kInvalidEntryIndex)
        {
            return nullptr;
        }

        return &m_entries[m_fallbackEntryIndex];
    }
} // namespace enigma::graphic
