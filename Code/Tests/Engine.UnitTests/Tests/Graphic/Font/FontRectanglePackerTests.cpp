#include <gtest/gtest.h>

#include "Engine/Graphic/Font/Atlas/RectanglePacker.hpp"
#include "Engine/Graphic/Font/Exception/FontException.hpp"

using namespace enigma::graphic;

TEST(FontRectanglePackerTests, PacksRectanglesDeterministically)
{
    const std::vector<RectanglePackItem> items = {
        {1u, IntVec2(4, 2)},
        {2u, IntVec2(3, 3)},
        {3u, IntVec2(2, 1)}
    };

    RectanglePackSettings settings;
    settings.maxSize = IntVec2(16, 16);

    RectanglePackResult first  = PackRectangles(items, settings);
    RectanglePackResult second = PackRectangles(items, settings);

    ASSERT_EQ(first.rectangles.size(), items.size());
    ASSERT_EQ(second.rectangles.size(), items.size());
    EXPECT_EQ(first.dimensions, second.dimensions);

    for (size_t index = 0; index < first.rectangles.size(); ++index)
    {
        EXPECT_EQ(first.rectangles[index].id, second.rectangles[index].id);
        EXPECT_EQ(first.rectangles[index].position, second.rectangles[index].position);
        EXPECT_EQ(first.rectangles[index].size, second.rectangles[index].size);
    }
}

TEST(FontRectanglePackerTests, PackedRectanglesStayInsideAtlasBounds)
{
    const std::vector<RectanglePackItem> items = {
        {10u, IntVec2(5, 7)},
        {11u, IntVec2(4, 4)},
        {12u, IntVec2(3, 2)}
    };

    RectanglePackSettings settings;
    settings.maxSize = IntVec2(16, 16);

    RectanglePackResult result = PackRectangles(items, settings);

    EXPECT_GT(result.dimensions.x, 0);
    EXPECT_GT(result.dimensions.y, 0);
    ASSERT_EQ(result.rectangles.size(), items.size());

    for (const PackedRectangle& rect : result.rectangles)
    {
        EXPECT_GE(rect.position.x, 0);
        EXPECT_GE(rect.position.y, 0);
        EXPECT_LE(rect.position.x + rect.size.x, result.dimensions.x);
        EXPECT_LE(rect.position.y + rect.size.y, result.dimensions.y);
    }
}

TEST(FontRectanglePackerTests, EmptyInputReturnsEmptyResult)
{
    RectanglePackSettings settings;
    settings.maxSize = IntVec2(16, 16);

    RectanglePackResult result = PackRectangles({}, settings);

    EXPECT_EQ(result.dimensions, IntVec2());
    EXPECT_TRUE(result.rectangles.empty());
}

TEST(FontRectanglePackerTests, RejectsInvalidSizes)
{
    RectanglePackSettings settings;
    settings.maxSize = IntVec2(16, 16);

    EXPECT_THROW((void)PackRectangles({{1u, IntVec2(0, 1)}}, settings), FontConfigurationException);
    EXPECT_THROW((void)PackRectangles({{1u, IntVec2(1, -1)}}, settings), FontConfigurationException);

    settings.maxSize = IntVec2(0, 16);
    EXPECT_THROW((void)PackRectangles({{1u, IntVec2(1, 1)}}, settings), FontConfigurationException);
}

TEST(FontRectanglePackerTests, FailsWhenAtlasIsTooSmall)
{
    RectanglePackSettings settings;
    settings.maxSize = IntVec2(4, 4);

    EXPECT_THROW((void)PackRectangles({{1u, IntVec2(5, 1)}}, settings), FontConfigurationException);
    EXPECT_THROW((void)PackRectangles({{1u, IntVec2(4, 4)}, {2u, IntVec2(1, 1)}}, settings), FontConfigurationException);
}
