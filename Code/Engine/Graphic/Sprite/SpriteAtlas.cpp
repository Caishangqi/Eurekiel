/**
 * @file SpriteAtlas.cpp
 * @brief SpriteAtlas 类实现
 */

#include "Engine/Graphic/Sprite/SpriteAtlas.hpp"
#include "Engine/Graphic/Sprite/Sprite.hpp"
#include "Engine/Graphic/Sprite/SpriteAtlasHelper.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include <cstring>

#include "Engine/Graphic/Integration/RendererSubsystem.hpp"

namespace enigma::graphic
{
    struct Rectangle
    {
        int x, y, width, height;

        Rectangle() : x(0), y(0), width(0), height(0)
        {
        }

        Rectangle(int x_, int y_, int w, int h) : x(x_), y(y_), width(w), height(h)
        {
        }

        bool Intersects(const Rectangle& other) const
        {
            return !(x >= other.x + other.width || x + width <= other.x ||
                y >= other.y + other.height || y + height <= other.y);
        }

        bool Contains(const Rectangle& other) const
        {
            return other.x >= x && other.y >= y &&
                other.x + other.width <= x + width &&
                other.y + other.height <= y + height;
        }
    };

    // ==================== Constructor ====================

    SpriteAtlas::SpriteAtlas(const std::string& atlasName)
        : m_atlasName(atlasName)
          , m_atlasDimensions(0, 0)
          , m_maxAtlasSize(4096, 4096)
          , m_isPacked(false)
    {
    }

    // ==================== Grid cutting interface ====================

    void SpriteAtlas::BuildFromGrid(
        const char*    imagePath,
        const IntVec2& gridLayout,
        const char*    spritePrefix
    )
    {
        Image image(imagePath);
        BuildFromGrid(image, gridLayout, spritePrefix);
    }

    void SpriteAtlas::BuildFromGrid(
        const Image&   image,
        const IntVec2& gridLayout,
        const char*    spritePrefix
    )
    {
        if (!SpriteAtlasHelper::ValidateGridLayout(gridLayout, "BuildFromGrid"))
        {
            return;
        }

        if (!SpriteAtlasHelper::ValidateImage(image, "BuildFromGrid"))
        {
            return;
        }

        IntVec2 textureDims = image.GetDimensions();
        Clear();

        m_atlasTexture = SpriteAtlasHelper::CreateTextureFromImage(image, textureDims);

        if (!m_atlasTexture)
        {
            ERROR_RECOVERABLE("BuildFromGrid: Failed to create texture");
            return;
        }

        IntVec2 cellSize     = SpriteAtlasHelper::CalculateCellSize(textureDims, gridLayout);
        int     totalSprites = gridLayout.x * gridLayout.y;

        for (int index = 0; index < totalSprites; ++index)
        {
            int row = index / gridLayout.x;
            int col = index % gridLayout.x;

            AABB2       uvBounds   = CalculateUVCoordinates(row, col, gridLayout, textureDims);
            std::string spriteName = std::string(spritePrefix) + "_" + std::to_string(index);

            SpriteData spriteData(spriteName, uvBounds, Vec2(0.5f, 0.5f), cellSize);
            m_spriteData.push_back(spriteData);
            m_nameToIndex[spriteName] = m_spriteData.size() - 1;
        }

        m_isPacked        = true;
        m_atlasDimensions = textureDims;
    }

    void SpriteAtlas::BuildFromGrid(
        const char*                     imagePath,
        const IntVec2&                  gridLayout,
        const std::vector<std::string>& customNames
    )
    {
        int expectedCount = gridLayout.x * gridLayout.y;
        if (static_cast<int>(customNames.size()) != expectedCount)
        {
            ERROR_RECOVERABLE("BuildFromGrid: customNames count mismatch");
            return;
        }

        Image image(imagePath);

        if (!SpriteAtlasHelper::ValidateImage(image, "BuildFromGrid"))
        {
            return;
        }

        IntVec2 textureDims = image.GetDimensions();
        Clear();

        m_atlasTexture = SpriteAtlasHelper::CreateTextureFromImage(image, textureDims);

        if (!m_atlasTexture)
        {
            ERROR_RECOVERABLE("BuildFromGrid: Failed to create texture");
            return;
        }

        IntVec2 cellSize     = SpriteAtlasHelper::CalculateCellSize(textureDims, gridLayout);
        int     totalSprites = expectedCount;

        for (int index = 0; index < totalSprites; ++index)
        {
            int row = index / gridLayout.x;
            int col = index % gridLayout.x;

            AABB2              uvBounds   = CalculateUVCoordinates(row, col, gridLayout, textureDims);
            const std::string& spriteName = customNames[index];

            SpriteData spriteData(spriteName, uvBounds, Vec2(0.5f, 0.5f), cellSize);
            m_spriteData.push_back(spriteData);
            m_nameToIndex[spriteName] = m_spriteData.size() - 1;
        }

        m_isPacked        = true;
        m_atlasDimensions = textureDims;
    }

    // ==================== Multi-image packaging interface ====================

    void SpriteAtlas::AddSprite(
        const std::string& spriteName,
        const char*        imagePath,
        const Vec2&        pivot
    )
    {
        if (spriteName.empty())
        {
            ERROR_RECOVERABLE("AddSprite: Empty sprite name");
            return;
        }

        if (!SpriteAtlasHelper::ValidateSpriteName(spriteName, m_nameToIndex, "AddSprite"))
        {
            return;
        }

        auto image = std::make_unique<Image>(imagePath);

        if (!SpriteAtlasHelper::ValidateImage(*image, "AddSprite"))
        {
            return;
        }

        IntVec2 dims = image->GetDimensions();

        SpriteData data(spriteName, AABB2(0, 0, 0, 0), pivot, dims);
        m_spriteData.push_back(data);
        m_nameToIndex[spriteName] = m_spriteData.size() - 1;
        m_pendingImages.push_back(std::move(image));
        m_isPacked = false;
    }

    void SpriteAtlas::AddSprite(
        const std::string& spriteName,
        const Image&       image,
        const Vec2&        pivot
    )
    {
        if (spriteName.empty())
        {
            ERROR_RECOVERABLE("AddSprite: Empty sprite name");
            return;
        }

        if (!SpriteAtlasHelper::ValidateImage(image, "AddSprite"))
        {
            return;
        }

        if (!SpriteAtlasHelper::ValidateSpriteName(spriteName, m_nameToIndex, "AddSprite"))
        {
            return;
        }

        IntVec2 dims = image.GetDimensions();

        auto       imageCopy = std::make_unique<Image>(image);
        SpriteData data(spriteName, AABB2(0, 0, 0, 0), pivot, dims);
        m_spriteData.push_back(data);
        m_nameToIndex[spriteName] = m_spriteData.size() - 1;
        m_pendingImages.push_back(std::move(imageCopy));
        m_isPacked = false;
    }

    void SpriteAtlas::PackAtlas(PackingMode mode)
    {
        if (m_pendingImages.empty())
        {
            ERROR_RECOVERABLE("PackAtlas: No pending images");
            return;
        }

        switch (mode)
        {
        case PackingMode::SimpleGrid:
            PackSimpleGrid();
            break;
        case PackingMode::MaxRects:
            PackMaxRects();
            break;
        case PackingMode::Auto:
            {
                bool uniformSize = true;
                if (!m_pendingImages.empty())
                {
                    IntVec2 firstSize = m_pendingImages[0]->GetDimensions();
                    for (size_t i = 1; i < m_pendingImages.size(); ++i)
                    {
                        IntVec2 size = m_pendingImages[i]->GetDimensions();
                        if (size.x != firstSize.x || size.y != firstSize.y)
                        {
                            uniformSize = false;
                            break;
                        }
                    }
                }

                if (uniformSize)
                {
                    PackSimpleGrid();
                }
                else
                {
                    PackMaxRects();
                }
                break;
            }
        }
    }

    // ==================== Sprite access interface ====================

    Sprite SpriteAtlas::GetSprite(const std::string& name) const
    {
        auto it = m_nameToIndex.find(name);
        if (it == m_nameToIndex.end())
        {
            ERROR_RECOVERABLE(Stringf("Sprite not found: %s", name.c_str()));
            return Sprite();
        }

        const SpriteData& data = m_spriteData[it->second];
        return Sprite(m_atlasTexture, data.uvBounds, data.name, data.pivot, data.dimensions);
    }

    Sprite SpriteAtlas::GetSprite(int index) const
    {
        if (index < 0 || index >= static_cast<int>(m_spriteData.size()))
        {
            ERROR_RECOVERABLE(Stringf("Invalid sprite index: %d", index));
            return Sprite();
        }

        const SpriteData& data = m_spriteData[index];
        return Sprite(m_atlasTexture, data.uvBounds, data.name, data.pivot, data.dimensions);
    }

    bool SpriteAtlas::HasSprite(const std::string& name) const
    {
        return m_nameToIndex.find(name) != m_nameToIndex.end();
    }

    // ==================== Query interface ====================

    std::vector<std::string> SpriteAtlas::GetAllSpriteNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_spriteData.size());
        for (const auto& data : m_spriteData)
        {
            names.push_back(data.name);
        }
        return names;
    }

    // ==================== Management interface ====================

    bool SpriteAtlas::RemoveSprite(const std::string& name)
    {
        auto it = m_nameToIndex.find(name);
        if (it == m_nameToIndex.end())
        {
            ERROR_RECOVERABLE(Stringf("RemoveSprite: Sprite not found: %s", name.c_str()));
            return false;
        }

        size_t index = it->second;
        m_spriteData.erase(m_spriteData.begin() + index);

        m_nameToIndex.clear();
        for (size_t i = 0; i < m_spriteData.size(); ++i)
        {
            m_nameToIndex[m_spriteData[i].name] = i;
        }

        if (m_spriteData.empty())
        {
            m_atlasTexture.reset();
            m_isPacked = false;
        }

        return true;
    }

    void SpriteAtlas::Clear()
    {
        m_spriteData.clear();
        m_nameToIndex.clear();
        m_atlasTexture.reset();
        m_atlasDimensions = IntVec2(0, 0);
        m_isPacked        = false;
        m_pendingImages.clear();
    }

    // ==================== Private helper methods ====================

    AABB2 SpriteAtlas::CalculateUVCoordinates(
        int            row,
        int            col,
        const IntVec2& gridLayout,
        const IntVec2& textureDimensions
    ) const
    {
        float uvMinX = static_cast<float>(col) / gridLayout.x;
        // [FIX] Flip Y axis: row 0 (visual top) should map to higher UV values
        // This accounts for how image files are typically stored (top-to-bottom)
        float uvMinY = static_cast<float>(gridLayout.y - 1 - row) / gridLayout.y;
        float uvMaxX = static_cast<float>(col + 1) / gridLayout.x;
        float uvMaxY = static_cast<float>(gridLayout.y - row) / gridLayout.y;

        float texelOffsetU = 0.5f / textureDimensions.x;
        float texelOffsetV = 0.5f / textureDimensions.y;
        uvMinX             += texelOffsetU;
        uvMinY             += texelOffsetV;
        uvMaxX             -= texelOffsetU;
        uvMaxY             -= texelOffsetV;

        return AABB2(uvMinX, uvMinY, uvMaxX, uvMaxY);
    }

    void SpriteAtlas::PackSimpleGrid()
    {
        if (m_pendingImages.empty())
        {
            return;
        }

        int     imageCount = static_cast<int>(m_pendingImages.size());
        IntVec2 cellSize   = m_pendingImages[0]->GetDimensions();

        IntVec2 atlasSize = SpriteAtlasHelper::CalculateGridAtlasSize(imageCount, cellSize, m_maxAtlasSize);

        if (atlasSize.x > m_maxAtlasSize.x || atlasSize.y > m_maxAtlasSize.y)
        {
            return;
        }

        int cols = m_maxAtlasSize.x / cellSize.x;
        int rows = (imageCount + cols - 1) / cols;

        Image atlasImage(atlasSize, Rgba8::BLACK);

        for (int i = 0; i < imageCount; ++i)
        {
            int row   = i / cols;
            int col   = i % cols;
            int destX = col * cellSize.x;
            int destY = row * cellSize.y;

            const Image& srcImage = *m_pendingImages[i];
            const Rgba8* srcData  = static_cast<const Rgba8*>(srcImage.GetRawData());
            Rgba8*       destData = const_cast<Rgba8*>(static_cast<const Rgba8*>(atlasImage.GetRawData()));

            for (int y = 0; y < cellSize.y; ++y)
            {
                const Rgba8* srcRow  = srcData + y * cellSize.x;
                Rgba8*       destRow = destData + (destY + y) * atlasSize.x + destX;
                std::memcpy(destRow, srcRow, cellSize.x * sizeof(Rgba8));
            }

            AABB2 uvBounds           = CalculateUVCoordinates(row, col, IntVec2(cols, rows), atlasSize);
            m_spriteData[i].uvBounds = uvBounds;
        }

        m_atlasTexture = SpriteAtlasHelper::CreateTextureFromImage(atlasImage, atlasSize);

        if (!m_atlasTexture)
        {
            ERROR_RECOVERABLE("PackSimpleGrid: Failed to create atlas texture");
            return;
        }

        m_atlasDimensions = atlasSize;
    }

    void SpriteAtlas::PackMaxRects()
    {
        if (m_pendingImages.empty())
        {
            ERROR_RECOVERABLE("PackMaxRects: No pending images to pack");
            return;
        }

        // [STEP 1] Calculate total area and initial atlas size
        int     totalArea = 0;
        IntVec2 maxImageSize(0, 0);

        for (const auto& img : m_pendingImages)
        {
            IntVec2 dims   = img->GetDimensions();
            totalArea      += dims.x * dims.y;
            maxImageSize.x = std::max(maxImageSize.x, dims.x);
            maxImageSize.y = std::max(maxImageSize.y, dims.y);
        }

        int estimatedSize = static_cast<int>(std::sqrt(totalArea * 1.2f));
        estimatedSize     = std::max(estimatedSize, std::max(maxImageSize.x, maxImageSize.y));

        int atlasWidth = 1;
        while (atlasWidth < estimatedSize) atlasWidth *= 2;
        int atlasHeight = atlasWidth;

        atlasWidth  = std::min(atlasWidth, m_maxAtlasSize.x);
        atlasHeight = std::min(atlasHeight, m_maxAtlasSize.y);

        // [STEP 2] Initialize free rectangles list
        std::vector<Rectangle> freeRects;
        freeRects.push_back(Rectangle(0, 0, atlasWidth, atlasHeight));

        // [STEP 3] Pack each image using Best-Fit heuristic
        std::vector<Rectangle> placedRects;
        placedRects.reserve(m_pendingImages.size());

        for (size_t i = 0; i < m_pendingImages.size(); ++i)
        {
            IntVec2 imgDims = m_pendingImages[i]->GetDimensions();

            int bestIndex        = -1;
            int bestShortSideFit = INT_MAX;
            int bestLongSideFit  = INT_MAX;

            for (size_t j = 0; j < freeRects.size(); ++j)
            {
                const Rectangle& rect = freeRects[j];

                if (rect.width >= imgDims.x && rect.height >= imgDims.y)
                {
                    int leftoverX    = rect.width - imgDims.x;
                    int leftoverY    = rect.height - imgDims.y;
                    int shortSideFit = std::min(leftoverX, leftoverY);
                    int longSideFit  = std::max(leftoverX, leftoverY);

                    if (shortSideFit < bestShortSideFit ||
                        (shortSideFit == bestShortSideFit && longSideFit < bestLongSideFit))
                    {
                        bestIndex        = static_cast<int>(j);
                        bestShortSideFit = shortSideFit;
                        bestLongSideFit  = longSideFit;
                    }
                }
            }

            if (bestIndex == -1)
            {
                ERROR_RECOVERABLE("PackMaxRects: Failed to pack all images (atlas too small)");
                return;
            }

            Rectangle placed(freeRects[bestIndex].x, freeRects[bestIndex].y, imgDims.x, imgDims.y);
            placedRects.push_back(placed);

            // [STEP 4] Split free rectangles
            std::vector<Rectangle> newFreeRects;

            for (const Rectangle& freeRect : freeRects)
            {
                if (placed.Intersects(freeRect))
                {
                    if (placed.x > freeRect.x)
                    {
                        newFreeRects.push_back(Rectangle(freeRect.x, freeRect.y,
                                                         placed.x - freeRect.x, freeRect.height));
                    }
                    if (placed.x + placed.width < freeRect.x + freeRect.width)
                    {
                        newFreeRects.push_back(Rectangle(placed.x + placed.width, freeRect.y,
                                                         freeRect.x + freeRect.width - (placed.x + placed.width), freeRect.height));
                    }
                    if (placed.y > freeRect.y)
                    {
                        newFreeRects.push_back(Rectangle(freeRect.x, freeRect.y,
                                                         freeRect.width, placed.y - freeRect.y));
                    }
                    if (placed.y + placed.height < freeRect.y + freeRect.height)
                    {
                        newFreeRects.push_back(Rectangle(freeRect.x, placed.y + placed.height,
                                                         freeRect.width, freeRect.y + freeRect.height - (placed.y + placed.height)));
                    }
                }
                else
                {
                    newFreeRects.push_back(freeRect);
                }
            }

            freeRects = std::move(newFreeRects);

            // Remove redundant rectangles
            for (size_t j = 0; j < freeRects.size(); ++j)
            {
                for (size_t k = j + 1; k < freeRects.size();)
                {
                    if (freeRects[j].Contains(freeRects[k]))
                    {
                        freeRects.erase(freeRects.begin() + k);
                    }
                    else if (freeRects[k].Contains(freeRects[j]))
                    {
                        freeRects.erase(freeRects.begin() + j);
                        --j;
                        break;
                    }
                    else
                    {
                        ++k;
                    }
                }
            }
        }

        // [STEP 5] Create atlas texture and copy images
        Image atlasImage(IntVec2(atlasWidth, atlasHeight), Rgba8::BLACK);

        for (size_t i = 0; i < placedRects.size(); ++i)
        {
            const Rectangle& rect     = placedRects[i];
            const Image&     srcImage = *m_pendingImages[i];
            IntVec2          srcDims  = srcImage.GetDimensions();

            for (int y = 0; y < srcDims.y; ++y)
            {
                const Rgba8* srcRow = static_cast<const Rgba8*>(srcImage.GetRawData()) + y * srcDims.x;
                Rgba8*       dstRow = static_cast<Rgba8*>(const_cast<void*>(atlasImage.GetRawData())) + (rect.y + y) * atlasWidth + rect.x;
                std::memcpy(dstRow, srcRow, srcDims.x * sizeof(Rgba8));
            }

            float uvMinX = static_cast<float>(rect.x) / atlasWidth;
            float uvMinY = static_cast<float>(rect.y) / atlasHeight;
            float uvMaxX = static_cast<float>(rect.x + rect.width) / atlasWidth;
            float uvMaxY = static_cast<float>(rect.y + rect.height) / atlasHeight;

            float texelOffsetU = 0.5f / atlasWidth;
            float texelOffsetV = 0.5f / atlasHeight;
            uvMinX             += texelOffsetU;
            uvMinY             += texelOffsetV;
            uvMaxX             -= texelOffsetU;
            uvMaxY             -= texelOffsetV;

            m_spriteData[i].uvBounds = AABB2(uvMinX, uvMinY, uvMaxX, uvMaxY);
        }

        IntVec2 atlasDims = atlasImage.GetDimensions();
        m_atlasTexture    = SpriteAtlasHelper::CreateTextureFromImage(atlasImage, atlasDims);

        if (!m_atlasTexture)
        {
            ERROR_RECOVERABLE("PackMaxRects: Failed to create atlas texture");
            return;
        }

        m_atlasDimensions = IntVec2(atlasWidth, atlasHeight);
        m_isPacked        = true;
        m_pendingImages.clear();
    }
} // namespace enigma::graphic
