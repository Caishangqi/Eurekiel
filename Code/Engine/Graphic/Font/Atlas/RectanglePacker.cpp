#include "Engine/Graphic/Font/Atlas/RectanglePacker.hpp"

#include <algorithm>
#include <limits>
#include <string>

#include "Engine/Graphic/Font/Exception/FontException.hpp"

namespace enigma::graphic
{
    namespace
    {
        int NextPowerOfTwo(int value)
        {
            if (value <= 1)
            {
                return 1;
            }

            int result = 1;
            while (result < value)
            {
                if (result > std::numeric_limits<int>::max() / 2)
                {
                    throw FontConfigurationException("Rectangle pack dimension overflow");
                }
                result *= 2;
            }

            return result;
        }

        void ValidateSettings(const RectanglePackSettings& settings)
        {
            if (settings.maxSize.x <= 0 || settings.maxSize.y <= 0)
            {
                throw FontConfigurationException(
                    "Rectangle pack max size must be positive: " + settings.maxSize.toString());
            }
        }

        void ValidateItems(const std::vector<RectanglePackItem>& items, const RectanglePackSettings& settings)
        {
            for (const RectanglePackItem& item : items)
            {
                if (item.size.x <= 0 || item.size.y <= 0)
                {
                    throw FontConfigurationException(
                        "Rectangle pack item size must be positive for id " + std::to_string(item.id));
                }

                if (item.size.x > settings.maxSize.x || item.size.y > settings.maxSize.y)
                {
                    throw FontConfigurationException(
                        "Rectangle pack item exceeds max size for id " + std::to_string(item.id));
                }
            }
        }

        bool TryPackShelf(
            const std::vector<RectanglePackItem>& items,
            int                                   width,
            int                                   maxHeight,
            std::vector<PackedRectangle>&         packed,
            int&                                  usedHeight)
        {
            packed.clear();
            packed.reserve(items.size());

            int cursorX     = 0;
            int cursorY     = 0;
            int shelfHeight = 0;
            usedHeight      = 0;

            for (const RectanglePackItem& item : items)
            {
                if (cursorX > 0 && cursorX + item.size.x > width)
                {
                    cursorY += shelfHeight;
                    cursorX     = 0;
                    shelfHeight = 0;
                }

                if (item.size.x > width || cursorY + item.size.y > maxHeight)
                {
                    packed.clear();
                    usedHeight = 0;
                    return false;
                }

                packed.push_back({item.id, IntVec2(cursorX, cursorY), item.size});

                cursorX += item.size.x;
                shelfHeight = std::max(shelfHeight, item.size.y);
                usedHeight  = std::max(usedHeight, cursorY + shelfHeight);
            }

            return true;
        }
    } // namespace

    RectanglePackResult PackRectangles(
        const std::vector<RectanglePackItem>& items,
        const RectanglePackSettings&          settings)
    {
        ValidateSettings(settings);
        ValidateItems(items, settings);

        if (items.empty())
        {
            return {};
        }

        int maxItemWidth = 0;
        for (const RectanglePackItem& item : items)
        {
            maxItemWidth = std::max(maxItemWidth, item.size.x);
        }

        int width = settings.powerOfTwoDimensions ? NextPowerOfTwo(maxItemWidth) : maxItemWidth;
        while (width <= settings.maxSize.x)
        {
            std::vector<PackedRectangle> packed;
            int                          usedHeight = 0;
            if (TryPackShelf(items, width, settings.maxSize.y, packed, usedHeight))
            {
                int height = settings.powerOfTwoDimensions ? NextPowerOfTwo(usedHeight) : usedHeight;
                if (height <= settings.maxSize.y)
                {
                    RectanglePackResult result;
                    result.dimensions = IntVec2(width, height);
                    result.rectangles = std::move(packed);
                    return result;
                }
            }

            if (settings.powerOfTwoDimensions)
            {
                if (width > settings.maxSize.x / 2)
                {
                    break;
                }
                width *= 2;
            }
            else
            {
                ++width;
            }
        }

        throw FontConfigurationException(
            "Unable to pack " + std::to_string(items.size()) +
            " rectangles within max size " + settings.maxSize.toString());
    }
} // namespace enigma::graphic
