#include <gtest/gtest.h>

#include "Engine/Graphic/Font/Font.hpp"

#include <filesystem>
#include <string>

using namespace enigma::graphic;

namespace
{
    std::filesystem::path GetRobotoFixturePath()
    {
        return "F:/p4/Personal/SD/Engine/Code/ThirdParty/imgui/misc/fonts/Roboto-Medium.ttf";
    }

    GlyphAtlasBuildSettings MakeAtlasSettings()
    {
        GlyphAtlasBuildSettings settings;
        settings.pixelHeight  = 32.0f;
        settings.padding      = 1;
        settings.maxAtlasSize = IntVec2(512, 512);
        return settings;
    }

    TrueTypeFont LoadFixtureFont()
    {
        const std::filesystem::path fontPath = GetRobotoFixturePath();
        EXPECT_TRUE(std::filesystem::exists(fontPath));
        return TrueTypeFont::LoadFromFile(fontPath);
    }
} // namespace

TEST(FontTextLayoutTests, MeasuresProportionalWidths)
{
    TrueTypeFont font  = LoadFixtureFont();
    GlyphAtlas   atlas = GlyphAtlas::BuildFromUtf8Text(font, "WWWiii", MakeAtlasSettings());

    TextLayoutResult wide = TextLayout::Build("WWW", atlas);
    TextLayoutResult thin = TextLayout::Build("iii", atlas);

    EXPECT_EQ(wide.lineCount, 1);
    EXPECT_EQ(thin.lineCount, 1);
    EXPECT_GT(wide.advanceWidth, thin.advanceWidth);
    EXPECT_EQ(wide.glyphs.size(), 3u);
    EXPECT_EQ(thin.glyphs.size(), 3u);
}

TEST(FontTextLayoutTests, HandlesNewlineWithPenReset)
{
    TrueTypeFont font  = LoadFixtureFont();
    GlyphAtlas   atlas = GlyphAtlas::BuildFromUtf8Text(font, "AB", MakeAtlasSettings());

    TextLayoutResult layout = TextLayout::Build("A\nB", atlas);

    ASSERT_EQ(layout.glyphs.size(), 2u);
    EXPECT_EQ(layout.lineCount, 2);
    EXPECT_FLOAT_EQ(layout.glyphs[0].baselinePosition.x, 0.0f);
    EXPECT_FLOAT_EQ(layout.glyphs[1].baselinePosition.x, 0.0f);
    EXPECT_LT(layout.glyphs[1].baselinePosition.y, layout.glyphs[0].baselinePosition.y);
    EXPECT_FLOAT_EQ(layout.lineHeight, atlas.GetVerticalMetrics().lineHeight);
}

TEST(FontTextLayoutTests, SpacesAdvanceWithoutVisibleGeometry)
{
    TrueTypeFont font  = LoadFixtureFont();
    GlyphAtlas   atlas = GlyphAtlas::BuildFromUtf8Text(font, "A A", MakeAtlasSettings());

    TextLayoutResult layout = TextLayout::Build("A A", atlas);

    ASSERT_EQ(layout.glyphs.size(), 3u);
    EXPECT_TRUE(layout.glyphs[0].hasVisiblePixels);
    EXPECT_FALSE(layout.glyphs[1].hasVisiblePixels);
    EXPECT_GT(layout.glyphs[1].advanceX, 0.0f);
    EXPECT_TRUE(layout.glyphs[2].hasVisiblePixels);
    EXPECT_GT(layout.advanceWidth, layout.glyphs[0].advanceX + layout.glyphs[2].advanceX);
}

TEST(FontTextLayoutTests, RejectsInvalidUtf8)
{
    TrueTypeFont font  = LoadFixtureFont();
    GlyphAtlas   atlas = GlyphAtlas::BuildFromUtf8Text(font, "A", MakeAtlasSettings());

    EXPECT_THROW((void)TextLayout::Build(std::string("\xE2\x82", 2), atlas), FontConfigurationException);
}

TEST(FontTextLayoutTests, MissingAtlasEntryThrowsInStrictMode)
{
    TrueTypeFont font  = LoadFixtureFont();
    GlyphAtlas   atlas = GlyphAtlas::BuildFromUtf8Text(font, "A", MakeAtlasSettings());

    TextLayoutSettings settings;
    settings.missingPolicy = TextLayoutMissingPolicy::Strict;

    EXPECT_THROW((void)TextLayout::Build("B", atlas, settings), FontConfigurationException);
}

TEST(FontTextLayoutTests, MissingAtlasEntryCanUseFallbackGlyph)
{
    TrueTypeFont font  = LoadFixtureFont();
    GlyphAtlas   atlas = GlyphAtlas::BuildFromCodepoints(font, {0x10FFFFu}, MakeAtlasSettings());

    TextLayoutResult layout = TextLayout::Build("A", atlas);

    ASSERT_EQ(layout.glyphs.size(), 1u);
    EXPECT_EQ(layout.glyphs[0].codepoint, static_cast<uint32_t>('A'));
    EXPECT_EQ(layout.glyphs[0].glyphIndex, 0u);
    EXPECT_TRUE(layout.glyphs[0].isFallbackGlyph);
}

TEST(FontTextLayoutTests, ProducesStablePositionsAndUvReferences)
{
    TrueTypeFont font  = LoadFixtureFont();
    GlyphAtlas   atlas = GlyphAtlas::BuildFromUtf8Text(font, "Wi", MakeAtlasSettings());

    TextLayoutResult layout = TextLayout::Build("Wi", atlas);

    ASSERT_EQ(layout.glyphs.size(), 2u);
    EXPECT_FLOAT_EQ(layout.glyphs[0].baselinePosition.x, 0.0f);
    EXPECT_GT(layout.glyphs[1].baselinePosition.x, layout.glyphs[0].baselinePosition.x);

    for (const TextLayoutGlyphQuad& quad : layout.glyphs)
    {
        ASSERT_TRUE(quad.hasVisiblePixels);
        EXPECT_GE(quad.uvBounds.m_mins.x, 0.0f);
        EXPECT_GE(quad.uvBounds.m_mins.y, 0.0f);
        EXPECT_LE(quad.uvBounds.m_maxs.x, 1.0f);
        EXPECT_LE(quad.uvBounds.m_maxs.y, 1.0f);
        EXPECT_LT(quad.localBounds.m_mins.x, quad.localBounds.m_maxs.x);
        EXPECT_LT(quad.localBounds.m_mins.y, quad.localBounds.m_maxs.y);
    }
}
