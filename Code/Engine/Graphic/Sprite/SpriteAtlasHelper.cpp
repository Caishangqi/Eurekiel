/**
 * @file SpriteAtlasHelper.cpp
 * @brief SpriteAtlas pure static tool class implementation
 */

#include "Engine/Graphic/Sprite/SpriteAtlasHelper.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"

namespace enigma::graphic
{
    bool SpriteAtlasHelper::ValidateImage(const Image& image, const char* context)
    {
        IntVec2 dims = image.GetDimensions();
        if (dims.x == 0 || dims.y == 0)
        {
            ERROR_RECOVERABLE(Stringf("%s: Invalid image", context));
            return false;
        }
        return true;
    }

    bool SpriteAtlasHelper::ValidateGridLayout(const IntVec2& gridLayout, const char* context)
    {
        if (gridLayout.x <= 0 || gridLayout.y <= 0)
        {
            ERROR_RECOVERABLE(Stringf("%s: Invalid gridLayout", context));
            return false;
        }
        return true;
    }

    bool SpriteAtlasHelper::ValidateSpriteName(
        const std::string&                             name,
        const std::unordered_map<std::string, size_t>& nameMap,
        const char*                                    context)
    {
        if (nameMap.find(name) != nameMap.end())
        {
            ERROR_RECOVERABLE(Stringf("%s: Sprite name already exists: %s", context, name.c_str()));
            return false;
        }
        return true;
    }

    std::shared_ptr<D12Texture> SpriteAtlasHelper::CreateTextureFromImage(
        const Image&   image,
        const IntVec2& dimensions)
    {
        return g_theRendererSubsystem->CreateTexture2D(
            dimensions.x,
            dimensions.y,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            image.GetRawData()
        );
    }

    IntVec2 SpriteAtlasHelper::CalculateCellSize(
        const IntVec2& textureDims,
        const IntVec2& gridLayout)
    {
        return IntVec2(
            textureDims.x / gridLayout.x,
            textureDims.y / gridLayout.y
        );
    }

    IntVec2 SpriteAtlasHelper::CalculateGridAtlasSize(
        int            imageCount,
        const IntVec2& cellSize,
        const IntVec2& maxSize)
    {
        int cols = maxSize.x / cellSize.x;
        int rows = (imageCount + cols - 1) / cols;

        IntVec2 atlasSize(cols * cellSize.x, rows * cellSize.y);

        if (atlasSize.x > maxSize.x || atlasSize.y > maxSize.y)
        {
            ERROR_RECOVERABLE("CalculateGridAtlasSize: Atlas size exceeds maximum");
        }

        return atlasSize;
    }
}
