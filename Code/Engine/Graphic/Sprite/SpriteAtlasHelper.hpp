/**
 * @file SpriteAtlasHelper.hpp
 * @brief SpriteAtlas pure static tool class - extract repeated verification and calculation logic
 */

#pragma once
#include "Engine/Math/IntVec2.hpp"
#include <memory>
#include <string>
#include <unordered_map>
class Image;

namespace enigma::graphic
{
    class D12Texture;

    /**
     * @brief SpriteAtlas pure static tool class
     * @note Strictly follow the Helper class design specifications: pure static, stateless, instantiation prohibited
     */
    class SpriteAtlasHelper
    {
    public:
        // [REQUIRED] prohibit instantiation
        SpriteAtlasHelper()                                    = delete;
        ~SpriteAtlasHelper()                                   = delete;
        SpriteAtlasHelper(const SpriteAtlasHelper&)            = delete;
        SpriteAtlasHelper& operator=(const SpriteAtlasHelper&) = delete;

        //Image validity verification
        static bool ValidateImage(const Image& image, const char* context);

        // Grid layout verification
        static bool ValidateGridLayout(const IntVec2& gridLayout, const char* context);

        //Sprite name duplicate verification
        static bool ValidateSpriteName(
            const std::string&                             name,
            const std::unordered_map<std::string, size_t>& nameMap,
            const char*                                    context
        );

        //Create Texture from Image
        static std::shared_ptr<D12Texture> CreateTextureFromImage(
            const Image&   image,
            const IntVec2& dimensions
        );

        // Calculate Grid cell size
        static IntVec2 CalculateCellSize(
            const IntVec2& textureDims,
            const IntVec2& gridLayout
        );

        // Calculate Grid packaged Atlas size
        static IntVec2 CalculateGridAtlasSize(
            int            imageCount,
            const IntVec2& cellSize,
            const IntVec2& maxSize
        );
    };
}
