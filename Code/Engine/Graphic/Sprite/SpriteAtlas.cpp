/**
 * @file SpriteAtlas.cpp
 * @brief SpriteAtlas 类实现
 */

#include "Engine/Graphic/Sprite/SpriteAtlas.hpp"
#include "Engine/Graphic/Sprite/Sprite.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include <cstring>

#include "Engine/Graphic/Integration/RendererSubsystem.hpp"

namespace enigma::graphic
{
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
        if (gridLayout.x <= 0 || gridLayout.y <= 0)
        {
            ERROR_RECOVERABLE("BuildFromGrid: Invalid gridLayout");
            return;
        }

        IntVec2 textureDims = image.GetDimensions();
        if (textureDims.x == 0 || textureDims.y == 0)
        {
            ERROR_RECOVERABLE("BuildFromGrid: Invalid image");
            return;
        }

        Clear();

        m_atlasTexture = g_theRendererSubsystem->CreateTexture2D(
            textureDims.x,
            textureDims.y,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            image.GetRawData()
        );

        if (!m_atlasTexture)
        {
            ERROR_RECOVERABLE("BuildFromGrid: Failed to create texture");
            return;
        }

        IntVec2 cellSize(textureDims.x / gridLayout.x, textureDims.y / gridLayout.y);
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

        Image   image(imagePath);
        IntVec2 textureDims = image.GetDimensions();
        if (textureDims.x == 0 || textureDims.y == 0)
        {
            ERROR_RECOVERABLE("BuildFromGrid: Invalid image");
            return;
        }

        Clear();

        m_atlasTexture = g_theRendererSubsystem->CreateTexture2D(
            textureDims.x,
            textureDims.y,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            image.GetRawData()
        );

        if (!m_atlasTexture)
        {
            ERROR_RECOVERABLE("BuildFromGrid: Failed to create texture");
            return;
        }

        IntVec2 cellSize(textureDims.x / gridLayout.x, textureDims.y / gridLayout.y);
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

        if (m_nameToIndex.find(spriteName) != m_nameToIndex.end())
        {
            ERROR_RECOVERABLE(Stringf("AddSprite: Sprite name already exists: %s", spriteName.c_str()));
            return;
        }

        auto    image = std::make_unique<Image>(imagePath);
        IntVec2 dims  = image->GetDimensions();
        if (dims.x == 0 || dims.y == 0)
        {
            ERROR_RECOVERABLE(Stringf("AddSprite: Invalid image: %s", imagePath));
            return;
        }

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
            ERROR_RECOVERABLE("AddSprite: Empty sprite name")
            return;
        }

        IntVec2 dims = image.GetDimensions();
        if (dims.x == 0 || dims.y == 0)
        {
            ERROR_RECOVERABLE("AddSprite: Invalid image");
            return;
        }

        if (m_nameToIndex.find(spriteName) != m_nameToIndex.end())
        {
            ERROR_RECOVERABLE(Stringf("AddSprite: Sprite name already exists: %s", spriteName.c_str()));
            return;
        }

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

        bool success = false;
        switch (mode)
        {
        case PackingMode::SimpleGrid:
            PackSimpleGrid();
            success = true;
            break;
        case PackingMode::MaxRects:
            ERROR_RECOVERABLE("PackAtlas: MaxRects not implemented");
            return;
        case PackingMode::Auto:
            PackSimpleGrid();
            success = true;
            break;
        }

        if (success)
        {
            m_isPacked = true;
            m_pendingImages.clear();
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

        int cols = m_maxAtlasSize.x / cellSize.x;
        int rows = (imageCount + cols - 1) / cols;

        IntVec2 atlasSize(cols * cellSize.x, rows * cellSize.y);

        if (atlasSize.x > m_maxAtlasSize.x || atlasSize.y > m_maxAtlasSize.y)
        {
            ERROR_RECOVERABLE("PackSimpleGrid: Atlas size exceeds maximum");
            return;
        }

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

        m_atlasTexture = g_theRendererSubsystem->CreateTexture2D(
            atlasSize.x,
            atlasSize.y,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            atlasImage.GetRawData()
        );

        if (!m_atlasTexture)
        {
            ERROR_RECOVERABLE("PackSimpleGrid: Failed to create atlas texture");
            return;
        }

        m_atlasDimensions = atlasSize;
    }

    void SpriteAtlas::PackMaxRects()
    {
        // TODO: Phase 2 implementation
    }
} // namespace enigma::graphic
