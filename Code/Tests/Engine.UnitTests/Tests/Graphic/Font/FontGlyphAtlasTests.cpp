#include <gtest/gtest.h>

#include "Engine/Graphic/Font/Font.hpp"

#include <algorithm>
#include <filesystem>

using namespace enigma::graphic;

namespace
{
    std::filesystem::path GetRobotoFixturePath()
    {
        return "F:/p4/Personal/SD/Engine/Code/ThirdParty/imgui/misc/fonts/Roboto-Medium.ttf";
    }

    GlyphAtlasBuildSettings MakeSettings(float pixelHeight = 32.0f)
    {
        GlyphAtlasBuildSettings settings;
        settings.pixelHeight  = pixelHeight;
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

TEST(FontGlyphAtlasTests, BuildFromUtf8TextCreatesAlphaAtlasAndDiagnostics)
{
    TrueTypeFont font = LoadFixtureFont();

    GlyphAtlas atlas = GlyphAtlas::BuildFromUtf8Text(font, "Wi ", MakeSettings());

    EXPECT_TRUE(atlas.IsBuilt());
    const GlyphAtlasDiagnostics& diagnostics = atlas.GetDiagnostics();
    EXPECT_EQ(diagnostics.glyphCount, 3u);
    EXPECT_GE(diagnostics.visibleGlyphCount, 2u);
    EXPECT_EQ(diagnostics.missingGlyphCount, 0u);
    EXPECT_EQ(diagnostics.pixelFormat, GlyphAtlasPixelFormat::AlphaCoverage8);
    EXPECT_EQ(diagnostics.stride, diagnostics.dimensions.x);
    ASSERT_EQ(atlas.GetPixels().size(),
              static_cast<size_t>(diagnostics.dimensions.x) * static_cast<size_t>(diagnostics.dimensions.y));
    EXPECT_TRUE(std::any_of(atlas.GetPixels().begin(), atlas.GetPixels().end(), [](uint8_t value) {
        return value > 0u;
    }));
}

TEST(FontGlyphAtlasTests, DuplicateCodepointsAreDeduplicatedInFirstSeenOrder)
{
    TrueTypeFont font = LoadFixtureFont();

    GlyphAtlas atlas = GlyphAtlas::BuildFromUtf8Text(font, "WiW", MakeSettings());

    const std::vector<GlyphAtlasEntry>& entries = atlas.GetEntries();
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].codepoint, static_cast<uint32_t>('W'));
    EXPECT_EQ(entries[1].codepoint, static_cast<uint32_t>('i'));
}

TEST(FontGlyphAtlasTests, BuildsDeterministicRectsAndUvs)
{
    TrueTypeFont font = LoadFixtureFont();

    GlyphAtlas first  = GlyphAtlas::BuildFromUtf8Text(font, "Wi", MakeSettings());
    GlyphAtlas second = GlyphAtlas::BuildFromUtf8Text(font, "Wi", MakeSettings());

    ASSERT_EQ(first.GetEntries().size(), second.GetEntries().size());
    EXPECT_EQ(first.GetDiagnostics().dimensions, second.GetDiagnostics().dimensions);

    for (size_t index = 0; index < first.GetEntries().size(); ++index)
    {
        const GlyphAtlasEntry& a = first.GetEntries()[index];
        const GlyphAtlasEntry& b = second.GetEntries()[index];

        EXPECT_EQ(a.codepoint, b.codepoint);
        EXPECT_EQ(a.atlasPosition, b.atlasPosition);
        EXPECT_EQ(a.bitmapSize, b.bitmapSize);
        EXPECT_FLOAT_EQ(a.uvBounds.m_mins.x, b.uvBounds.m_mins.x);
        EXPECT_FLOAT_EQ(a.uvBounds.m_mins.y, b.uvBounds.m_mins.y);
        EXPECT_FLOAT_EQ(a.uvBounds.m_maxs.x, b.uvBounds.m_maxs.x);
        EXPECT_FLOAT_EQ(a.uvBounds.m_maxs.y, b.uvBounds.m_maxs.y);

        if (a.hasVisiblePixels)
        {
            EXPECT_GE(a.uvBounds.m_mins.x, 0.0f);
            EXPECT_GE(a.uvBounds.m_mins.y, 0.0f);
            EXPECT_LE(a.uvBounds.m_maxs.x, 1.0f);
            EXPECT_LE(a.uvBounds.m_maxs.y, 1.0f);
            EXPECT_LT(a.uvBounds.m_mins.x, a.uvBounds.m_maxs.x);
            EXPECT_LT(a.uvBounds.m_mins.y, a.uvBounds.m_maxs.y);
        }
    }
}

TEST(FontGlyphAtlasTests, SpaceKeepsAdvanceWithoutVisiblePixels)
{
    TrueTypeFont font = LoadFixtureFont();

    GlyphAtlas atlas = GlyphAtlas::BuildFromUtf8Text(font, " ", MakeSettings());

    const GlyphAtlasEntry* space = atlas.FindEntry(static_cast<uint32_t>(' '));
    ASSERT_NE(space, nullptr);
    EXPECT_TRUE(space->found);
    EXPECT_FALSE(space->hasVisiblePixels);
    EXPECT_GT(space->metrics.advanceX, 0.0f);
    EXPECT_TRUE(atlas.GetPixels().empty());
    EXPECT_EQ(atlas.GetDiagnostics().visibleGlyphCount, 0u);
}

TEST(FontGlyphAtlasTests, MissingGlyphIsRecoverableFallbackData)
{
    TrueTypeFont font = LoadFixtureFont();

    GlyphAtlas atlas = GlyphAtlas::BuildFromCodepoints(font, {0x10FFFFu}, MakeSettings());

    const GlyphAtlasEntry* missing = atlas.FindEntry(0x10FFFFu);
    ASSERT_NE(missing, nullptr);
    EXPECT_FALSE(missing->found);
    EXPECT_TRUE(missing->isFallbackGlyph);
    EXPECT_EQ(missing->glyphIndex, 0u);
    EXPECT_EQ(atlas.GetDiagnostics().missingGlyphCount, 1u);
    EXPECT_EQ(atlas.FindFallbackEntry(), missing);
}

TEST(FontGlyphAtlasTests, StrictMissingGlyphPolicyThrows)
{
    TrueTypeFont font = LoadFixtureFont();

    GlyphAtlasBuildSettings settings = MakeSettings();
    settings.missingPolicy = GlyphMissingPolicy::Strict;

    EXPECT_THROW((void)GlyphAtlas::BuildFromCodepoints(font, {0x10FFFFu}, settings), AtlasBuildException);
}

TEST(FontGlyphAtlasTests, TooSmallAtlasThrows)
{
    TrueTypeFont font = LoadFixtureFont();

    GlyphAtlasBuildSettings settings = MakeSettings();
    settings.maxAtlasSize = IntVec2(1, 1);

    EXPECT_THROW((void)GlyphAtlas::BuildFromUtf8Text(font, "W", settings), AtlasBuildException);
}

TEST(FontGlyphAtlasTests, InvalidBuildInputsThrow)
{
    TrueTypeFont font = LoadFixtureFont();

    GlyphAtlasBuildSettings settings = MakeSettings();
    EXPECT_THROW((void)GlyphAtlas::BuildFromUtf8Text(font, "\n", settings), FontConfigurationException);

    settings.pixelHeight = 0.0f;
    EXPECT_THROW((void)GlyphAtlas::BuildFromUtf8Text(font, "W", settings), FontConfigurationException);
}
