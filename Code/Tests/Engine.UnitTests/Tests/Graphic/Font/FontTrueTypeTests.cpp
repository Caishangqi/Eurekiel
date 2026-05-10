#include <gtest/gtest.h>

#include "Engine/Graphic/Font/Font.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>

using namespace enigma::graphic;

namespace
{
    std::filesystem::path GetRobotoFixturePath()
    {
        return "F:/p4/Personal/SD/Engine/Code/ThirdParty/imgui/misc/fonts/Roboto-Medium.ttf";
    }

    std::filesystem::path GetInvalidFontFixturePath()
    {
        return GetRobotoFixturePath().parent_path() / "binary_to_compressed_c.cpp";
    }
} // namespace

TEST(FontTrueTypeTests, LoadFromFileSucceedsForRepoLocalTtf)
{
    const std::filesystem::path fontPath = GetRobotoFixturePath();
    ASSERT_TRUE(std::filesystem::exists(fontPath));

    TrueTypeFont font = TrueTypeFont::LoadFromFile(fontPath);
    EXPECT_TRUE(font.IsLoaded());
    EXPECT_EQ(font.GetFilePath(), fontPath);
}

TEST(FontTrueTypeTests, LoadFromFileThrowsForMissingFile)
{
    const std::filesystem::path missingPath = GetRobotoFixturePath().parent_path() / "missing-font-does-not-exist.ttf";

    EXPECT_THROW((void)TrueTypeFont::LoadFromFile(missingPath), FontResourceException);
}

TEST(FontTrueTypeTests, LoadFromFileThrowsForInvalidFontFile)
{
    const std::filesystem::path invalidPath = GetInvalidFontFixturePath();
    ASSERT_TRUE(std::filesystem::exists(invalidPath));

    EXPECT_THROW((void)TrueTypeFont::LoadFromFile(invalidPath), FontResourceException);
}

TEST(FontTrueTypeTests, UnloadedFontRejectsMetricQueries)
{
    TrueTypeFont font;

    EXPECT_FALSE(font.IsLoaded());
    EXPECT_THROW((void)font.GetVerticalMetrics(32.0f), FontConfigurationException);
    EXPECT_THROW((void)font.FindGlyph(static_cast<uint32_t>('A')), FontConfigurationException);
    EXPECT_THROW((void)font.GetGlyphMetrics(static_cast<uint32_t>('A'), 32.0f), FontConfigurationException);
    EXPECT_THROW((void)font.RasterizeGlyph(static_cast<uint32_t>('A'), 32.0f), FontConfigurationException);
}

TEST(FontTrueTypeTests, VerticalMetricsAreScaledForPixelHeight)
{
    TrueTypeFont font = TrueTypeFont::LoadFromFile(GetRobotoFixturePath());

    FontVerticalMetrics metrics = font.GetVerticalMetrics(32.0f);
    EXPECT_FLOAT_EQ(metrics.pixelHeight, 32.0f);
    EXPECT_GT(metrics.ascent, 0.0f);
    EXPECT_LT(metrics.descent, 0.0f);
    EXPECT_GT(metrics.lineHeight, 0.0f);
}

TEST(FontTrueTypeTests, GlyphMetricsShowProportionalAdvance)
{
    TrueTypeFont font = TrueTypeFont::LoadFromFile(GetRobotoFixturePath());

    GlyphLookupResult wideLookup = font.FindGlyph(static_cast<uint32_t>('W'));
    GlyphLookupResult thinLookup = font.FindGlyph(static_cast<uint32_t>('i'));
    ASSERT_TRUE(wideLookup.found);
    ASSERT_TRUE(thinLookup.found);

    GlyphMetrics wideMetrics = font.GetGlyphMetrics(static_cast<uint32_t>('W'), 48.0f);
    GlyphMetrics thinMetrics = font.GetGlyphMetrics(static_cast<uint32_t>('i'), 48.0f);
    EXPECT_TRUE(wideMetrics.found);
    EXPECT_TRUE(thinMetrics.found);
    EXPECT_GT(wideMetrics.advanceX, thinMetrics.advanceX);
    EXPECT_GT(wideMetrics.width, thinMetrics.width);
}

TEST(FontTrueTypeTests, MissingGlyphReturnsRecoverableFallbackIdentity)
{
    TrueTypeFont font = TrueTypeFont::LoadFromFile(GetRobotoFixturePath());

    GlyphLookupResult missing = font.FindGlyph(0x10FFFFu);
    EXPECT_EQ(missing.codepoint, 0x10FFFFu);
    EXPECT_EQ(missing.glyphIndex, 0u);
    EXPECT_FALSE(missing.found);
    EXPECT_TRUE(missing.isFallbackGlyph);

    GlyphMetrics missingMetrics = font.GetGlyphMetrics(0x10FFFFu, 32.0f);
    EXPECT_EQ(missingMetrics.codepoint, 0x10FFFFu);
    EXPECT_EQ(missingMetrics.glyphIndex, 0u);
    EXPECT_FALSE(missingMetrics.found);
}

TEST(FontTrueTypeTests, RasterizeGlyphReturnsOwnedAlphaBitmap)
{
    TrueTypeFont font = TrueTypeFont::LoadFromFile(GetRobotoFixturePath());

    GlyphBitmap bitmap = font.RasterizeGlyph(static_cast<uint32_t>('A'), 48.0f);
    EXPECT_EQ(bitmap.codepoint, static_cast<uint32_t>('A'));
    EXPECT_GT(bitmap.glyphIndex, 0u);
    EXPECT_FLOAT_EQ(bitmap.pixelHeight, 48.0f);
    ASSERT_GT(bitmap.width, 0);
    ASSERT_GT(bitmap.height, 0);
    EXPECT_EQ(bitmap.stride, bitmap.width);
    ASSERT_EQ(bitmap.pixels.size(), static_cast<size_t>(bitmap.width) * static_cast<size_t>(bitmap.height));
    EXPECT_TRUE(std::any_of(bitmap.pixels.begin(), bitmap.pixels.end(), [](uint8_t value) {
        return value > 0u;
    }));
}
