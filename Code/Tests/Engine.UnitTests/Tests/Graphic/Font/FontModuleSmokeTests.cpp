#include <gtest/gtest.h>

#include "Engine/Graphic/Font/Font.hpp"

#include <type_traits>

using namespace enigma::graphic;

TEST(FontModuleSmokeTests, AggregateHeaderExposesM1ShellTypes)
{
    TrueTypeFont trueTypeFont;
    GlyphAtlas glyphAtlas;
    TextLayout textLayout;
    VertexFont vertexFont;
    FontRenderer fontRenderer;
    FontShaderPreset userPreset(FontShaderPresetOwner::User);

    GlyphMetrics metrics;
    EXPECT_EQ(metrics.glyphIndex, 0u);
    EXPECT_FLOAT_EQ(metrics.advanceX, 0.0f);
    EXPECT_FALSE(glyphAtlas.IsBuilt());

    GlyphAtlasBuildSettings atlasSettings;
    EXPECT_FLOAT_EQ(atlasSettings.pixelHeight, 0.0f);
    GlyphAtlasDiagnostics atlasDiagnostics;
    EXPECT_EQ(atlasDiagnostics.pixelFormat, GlyphAtlasPixelFormat::AlphaCoverage8);

    TextLayoutSettings layoutSettings;
    TextLayoutResult layoutResult;
    EXPECT_EQ(layoutResult.lineCount, 0);
    EXPECT_EQ(layoutSettings.missingPolicy, TextLayoutMissingPolicy::UseAtlasFallback);

    EXPECT_EQ(userPreset.GetOwner(), FontShaderPresetOwner::User);

    (void)trueTypeFont;
    (void)glyphAtlas;
    (void)textLayout;
    (void)vertexFont;
    (void)fontRenderer;
}

TEST(FontModuleSmokeTests, ResourceShellsAreMoveOnly)
{
    static_assert(!std::is_copy_constructible_v<TrueTypeFont>);
    static_assert(!std::is_copy_assignable_v<TrueTypeFont>);
    static_assert(std::is_move_constructible_v<TrueTypeFont>);
    static_assert(std::is_move_assignable_v<TrueTypeFont>);

    static_assert(!std::is_copy_constructible_v<GlyphAtlas>);
    static_assert(!std::is_copy_assignable_v<GlyphAtlas>);
    static_assert(std::is_move_constructible_v<GlyphAtlas>);
    static_assert(std::is_move_assignable_v<GlyphAtlas>);

    static_assert(!std::is_copy_constructible_v<TextLayout>);
    static_assert(!std::is_copy_assignable_v<TextLayout>);
    static_assert(std::is_move_constructible_v<TextLayout>);
    static_assert(std::is_move_assignable_v<TextLayout>);

    static_assert(!std::is_copy_constructible_v<FontRenderer>);
    static_assert(!std::is_copy_assignable_v<FontRenderer>);
    static_assert(std::is_move_constructible_v<FontRenderer>);
    static_assert(std::is_move_assignable_v<FontRenderer>);
}

TEST(FontModuleSmokeTests, FontExceptionStoresMessage)
{
    FontResourceException exception("Missing default engine font");
    EXPECT_STREQ(exception.what(), "Missing default engine font");
}
