#pragma once

#include <cstdint>
#include <vector>

#include "Engine/Math/IntVec2.hpp"

namespace enigma::graphic
{
    struct RectanglePackItem
    {
        uint32_t id = 0;
        IntVec2  size;
    };

    struct PackedRectangle
    {
        uint32_t id = 0;
        IntVec2  position;
        IntVec2  size;
    };

    struct RectanglePackSettings
    {
        IntVec2 maxSize              = IntVec2(2048, 2048);
        bool    powerOfTwoDimensions = true;
    };

    struct RectanglePackResult
    {
        IntVec2                      dimensions;
        std::vector<PackedRectangle> rectangles;
    };

    RectanglePackResult PackRectangles(
        const std::vector<RectanglePackItem>& items,
        const RectanglePackSettings&          settings);
} // namespace enigma::graphic
