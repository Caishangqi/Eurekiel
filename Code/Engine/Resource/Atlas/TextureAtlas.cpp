#include "TextureAtlas.hpp"
#include "../../Core/ErrorWarningAssert.hpp"
#include "../../Core/StringUtils.hpp"
#include "../../Core/FileUtils.hpp"
#include "../../Core/Logger/LoggerAPI.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>

// Prevent Windows min/max macro conflicts
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// Include stb_image_write for PNG export
#pragma warning(push)
#pragma warning(disable: 4996) // Disable 'sprintf' unsafe warning
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Renderer/IRenderer.hpp"
#include "ThirdParty/stb/stb_image_write.h"
#pragma warning(pop)

namespace enigma::resource
{
    TextureAtlas::TextureAtlas(const AtlasConfig& config)
        : m_config(config)
          , m_metadata(ResourceLocation("engine", "atlas/" + config.name), std::filesystem::path("atlas/" + config.name))
          , m_location("engine", "atlas/" + config.name)
          , m_atlasImage(nullptr)
          , m_atlasDimensions(IntVec2::ZERO)
          , m_isBuilt(false)
          , m_gridResolution(config.requiredResolution)
          , m_atlasTexture(nullptr)
    {
        m_metadata.type = ResourceType::TEXTURE;
        m_stats.Reset();
    }

    TextureAtlas::~TextureAtlas()
    {
        Unload();
    }

    ResourceLocation TextureAtlas::GetResourceLocation() const
    {
        return m_location;
    }

    bool TextureAtlas::IsLoaded() const
    {
        return m_isBuilt && m_atlasImage != nullptr;
    }

    void TextureAtlas::Unload()
    {
        ClearAtlas();
    }

    const ResourceMetadata& TextureAtlas::GetMetadata() const
    {
        return m_metadata;
    }

    ResourceType TextureAtlas::GetType() const
    {
        return ResourceType::TEXTURE;
    }

    const void* TextureAtlas::GetRawData() const
    {
        if (!IsLoaded())
            return nullptr;
        return m_atlasImage->GetRawData();
    }

    size_t TextureAtlas::GetRawDataSize() const
    {
        if (!IsLoaded())
            return 0;
        return m_atlasDimensions.x * m_atlasDimensions.y * 4; // RGBA
    }

    bool TextureAtlas::BuildAtlas(const std::vector<std::shared_ptr<ImageResource>>& images)
    {
        // Clear previous atlas data
        ClearAtlas();

        if (images.empty())
        {
            return false;
        }

        // Validate all input images
        if (!ValidateImages(images))
        {
            return false;
        }

        // Pack sprites into atlas
        if (!PackSprites(images))
        {
            return false;
        }

        // Calculate UV coordinates for all sprites
        CalculateAllUVCoordinates();

        // Update statistics
        m_stats.totalSprites   = static_cast<int>(m_sprites.size());
        m_stats.atlasWidth     = m_atlasDimensions.x;
        m_stats.atlasHeight    = m_atlasDimensions.y;
        m_stats.atlasSizeBytes = m_atlasDimensions.x * m_atlasDimensions.y * 4; // RGBA

        // Calculate packing efficiency
        int usedPixels = m_stats.totalSprites * m_config.requiredResolution * m_config.requiredResolution;
        m_stats.CalculatePackingEfficiency(usedPixels);

        m_isBuilt = true;
        return true;
    }

    void TextureAtlas::ClearAtlas()
    {
        m_atlasImage.reset();
        m_sprites.clear();
        m_spriteLocationMap.clear();
        m_packingGrid.clear();
        m_atlasDimensions = IntVec2::ZERO;
        m_stats.Reset();
        m_isBuilt      = false;
        m_atlasTexture = nullptr;
    }

    const AtlasSprite* TextureAtlas::FindSprite(const ResourceLocation& location) const
    {
        auto it = m_spriteLocationMap.find(location);
        if (it != m_spriteLocationMap.end() && it->second < m_sprites.size())
        {
            return &m_sprites[it->second];
        }
        return nullptr;
    }

    const std::vector<AtlasSprite>& TextureAtlas::GetAllSprites() const
    {
        return m_sprites;
    }

    IntVec2 TextureAtlas::GetAtlasDimensions() const
    {
        return m_atlasDimensions;
    }

    const AtlasConfig& TextureAtlas::GetConfig() const
    {
        return m_config;
    }

    const AtlasStats& TextureAtlas::GetStats() const
    {
        return m_stats;
    }

    const Image* TextureAtlas::GetAtlasImage() const
    {
        return m_atlasImage.get();
    }

    Texture* TextureAtlas::GetAtlasTexture() const
    {
        // Create texture on-demand if not already created
        if (!m_atlasTexture && m_atlasImage && IsLoaded())
        {
            // Get renderer from global context
            extern IRenderer* g_theRenderer;
            if (g_theRenderer && m_atlasImage)
            {
                // Create GPU texture from atlas image with MipMap support
                m_atlasTexture = g_theRenderer->CreateTextureFromImageWithMipmaps(*m_atlasImage, 5);
                if (m_atlasTexture)
                {
                    if (m_atlasTexture->HasMipmaps())
                    {
                        core::LogInfo(core::LogResource, "[OK] TextureAtlas created with MipMap support (%d levels): %s (%dx%d)",
                                      m_atlasTexture->GetMipLevels(),
                                      m_location.ToString().c_str(),
                                      m_atlasDimensions.x, m_atlasDimensions.y);
                    }
                    else
                    {
                        core::LogWarn(core::LogResource, "[WARNING] TextureAtlas MipMap generation failed for: %s",
                                      m_location.ToString().c_str());
                    }
                }
                else
                {
                    core::LogError(core::LogResource, "Failed to create GPU texture for atlas: %s",
                                   m_location.ToString().c_str());
                }
            }
        }

        return m_atlasTexture;
    }

    bool TextureAtlas::ExportToPNG(const std::string& filepath) const
    {
        if (!IsLoaded())
        {
            return false;
        }

        // Create directory if it doesn't exist
        std::filesystem::path filePath(filepath);
        std::filesystem::path directory = filePath.parent_path();
        if (!directory.empty())
        {
            std::filesystem::create_directories(directory);
        }

        // Get image data in RGBA format
        const void* rawData = m_atlasImage->GetRawData();
        if (!rawData)
        {
            return false;
        }

        // Convert Rgba8 data to unsigned char array for stb_image_write
        const Rgba8* rgbaData    = static_cast<const Rgba8*>(rawData);
        int          totalPixels = m_atlasDimensions.x * m_atlasDimensions.y;

        std::vector<unsigned char> pngData;
        pngData.reserve(totalPixels * 4);

        for (int i = 0; i < totalPixels; ++i)
        {
            pngData.push_back(rgbaData[i].r);
            pngData.push_back(rgbaData[i].g);
            pngData.push_back(rgbaData[i].b);
            pngData.push_back(rgbaData[i].a);
        }

        // Export using stb_image_write
        stbi_flip_vertically_on_write(1); // Match our coordinate system
        int result = stbi_write_png(
            filepath.c_str(),
            m_atlasDimensions.x,
            m_atlasDimensions.y,
            4, // RGBA
            pngData.data(),
            m_atlasDimensions.x * 4 // stride
        );

        return result != 0;
    }

    void TextureAtlas::PrintAtlasInfo() const
    {
        printf("\n=== Atlas Info: %s ===\n", m_config.name.c_str());
        printf("Dimensions: %dx%d\n", m_atlasDimensions.x, m_atlasDimensions.y);
        printf("Sprites: %d\n", m_stats.totalSprites);
        printf("Packing Efficiency: %.1f%%\n", m_stats.packingEfficiency);
        printf("Required Resolution: %dx%d\n", m_config.requiredResolution, m_config.requiredResolution);
        printf("Rejected Sprites: %d\n", m_stats.rejectedSprites);
        printf("Scaled Sprites: %d\n", m_stats.scaledSprites);
        printf("Size: %.2f KB\n", m_stats.atlasSizeBytes / 1024.0f);
    }

    std::string TextureAtlas::GetDebugString() const
    {
        return Stringf("Atlas[%s] %dx%d with %d sprites (%.1f%% efficiency)",
                       m_config.name.c_str(),
                       m_atlasDimensions.x, m_atlasDimensions.y,
                       m_stats.totalSprites, m_stats.packingEfficiency);
    }

    bool TextureAtlas::ValidateImages(const std::vector<std::shared_ptr<ImageResource>>& images)
    {
        m_stats.rejectedSprites = 0;
        m_stats.scaledSprites   = 0;

        for (const auto& imageRes : images)
        {
            if (!imageRes || !imageRes->IsLoaded())
            {
                continue;
            }

            if (!imageRes->IsValidForAtlas())
            {
                m_stats.rejectedSprites++;
                continue;
            }

            int resolution = imageRes->GetResolution();
            if (resolution != m_config.requiredResolution)
            {
                if (m_config.rejectMismatched)
                {
                    m_stats.rejectedSprites++;
                    continue;
                }
                else if (m_config.autoScale)
                {
                    m_stats.scaledSprites++;
                }
            }
        }

        return true; // Continue even with some rejected/scaled sprites
    }

    bool TextureAtlas::PackSprites(const std::vector<std::shared_ptr<ImageResource>>& images)
    {
        // Count valid sprites
        int validSprites = 0;
        for (const auto& imageRes : images)
        {
            if (!imageRes || !imageRes->IsLoaded() || !imageRes->IsValidForAtlas())
                continue;

            int resolution = imageRes->GetResolution();
            if (resolution != m_config.requiredResolution)
            {
                if (m_config.rejectMismatched)
                    continue;
            }
            validSprites++;
        }

        if (validSprites == 0)
        {
            return false;
        }

        // Calculate optimal atlas size
        m_atlasDimensions = FindBestAtlasSize(validSprites, m_config.requiredResolution);

        // Create atlas image
        m_atlasImage = std::make_unique<Image>(m_atlasDimensions, Rgba8(0, 0, 0, 0)); // Transparent background

        // Initialize packing grid
        InitializePackingGrid(m_atlasDimensions, m_config.requiredResolution);

        // Pack each sprite
        for (const auto& imageRes : images)
        {
            if (!imageRes || !imageRes->IsLoaded() || !imageRes->IsValidForAtlas())
                continue;

            int resolution = imageRes->GetResolution();
            if (resolution != m_config.requiredResolution)
            {
                if (m_config.rejectMismatched)
                    continue;
            }

            // Find position for this sprite
            IntVec2 spriteSize(m_config.requiredResolution, m_config.requiredResolution);
            IntVec2 position;

            if (!TryPackSprite(spriteSize, position))
            {
                // Atlas is full, could implement multiple atlas support here
                break;
            }

            // Create atlas sprite entry
            AtlasSprite sprite(imageRes->GetResourceLocation(), position, spriteSize, resolution);
            m_sprites.push_back(sprite);
            m_spriteLocationMap[imageRes->GetResourceLocation()] = m_sprites.size() - 1;

            // Copy image data to atlas
            if (resolution == m_config.requiredResolution)
            {
                // Direct copy
                CopyImageToAtlas(imageRes->GetImage(), position);
            }
            else if (m_config.autoScale)
            {
                // Scale and copy
                Image scaledImage(spriteSize, Rgba8::WHITE);
                ScaleImageIfNeeded(imageRes->GetImage(), scaledImage, m_config.requiredResolution);
                CopyImageToAtlas(scaledImage, position);
            }
        }

        return !m_sprites.empty();
    }

    void TextureAtlas::ScaleImageIfNeeded(const Image& source, Image& destination, int targetResolution) const
    {
        IntVec2 sourceSize = source.GetDimensions();
        IntVec2 destSize(targetResolution, targetResolution);

        // Simple nearest-neighbor scaling
        for (int y = 0; y < destSize.y; ++y)
        {
            for (int x = 0; x < destSize.x; ++x)
            {
                // Map destination coordinates to source coordinates
                int srcX = (x * sourceSize.x) / destSize.x;
                int srcY = (y * sourceSize.y) / destSize.y;

                // Clamp to source bounds
                srcX = std::min(srcX, sourceSize.x - 1);
                srcY = std::min(srcY, sourceSize.y - 1);

                Rgba8 color = source.GetTexelColor(IntVec2(srcX, srcY));
                destination.SetTexelColor(IntVec2(x, y), color);
            }
        }
    }

    void TextureAtlas::CopyImageToAtlas(const Image& source, const IntVec2& position)
    {
        IntVec2 sourceSize = source.GetDimensions();

        for (int y = 0; y < sourceSize.y; ++y)
        {
            for (int x = 0; x < sourceSize.x; ++x)
            {
                IntVec2 sourcePos(x, y);
                IntVec2 atlasPos = position + sourcePos;

                // Bounds check
                if (atlasPos.x >= 0 && atlasPos.x < m_atlasDimensions.x &&
                    atlasPos.y >= 0 && atlasPos.y < m_atlasDimensions.y)
                {
                    Rgba8 color = source.GetTexelColor(sourcePos);
                    m_atlasImage->SetTexelColor(atlasPos, color);
                }
            }
        }
    }

    void TextureAtlas::CalculateAllUVCoordinates()
    {
        for (auto& sprite : m_sprites)
        {
            sprite.CalculateUVCoordinates(m_atlasDimensions);
        }
    }

    IntVec2 TextureAtlas::FindBestAtlasSize(int totalSprites, int spriteResolution) const
    {
        // Calculate minimum area needed
        int totalArea = totalSprites * spriteResolution * spriteResolution;
        int minSize   = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(totalArea))));

        // Round up to next power of 2 for GPU efficiency
        int size = 1;
        while (size < minSize)
        {
            size *= 2;
        }

        // Ensure we don't exceed maximum atlas size
        size = std::min(size, m_config.maxAtlasSize.x);
        size = std::min(size, m_config.maxAtlasSize.y);

        return IntVec2(size, size);
    }

    bool TextureAtlas::TryPackSprite(IntVec2 spriteSize, IntVec2& outPosition)
    {
        int gridCols = m_atlasDimensions.x / m_gridResolution;
        int gridRows = m_atlasDimensions.y / m_gridResolution;

        // Simple linear search for free space (can be optimized with better algorithms)
        for (int row = 0; row <= gridRows - (spriteSize.y / m_gridResolution); ++row)
        {
            for (int col = 0; col <= gridCols - (spriteSize.x / m_gridResolution); ++col)
            {
                // Check if this position is free
                bool canPlace   = true;
                int  spriteCols = spriteSize.x / m_gridResolution;
                int  spriteRows = spriteSize.y / m_gridResolution;

                for (int sr = 0; sr < spriteRows && canPlace; ++sr)
                {
                    for (int sc = 0; sc < spriteCols && canPlace; ++sc)
                    {
                        if (m_packingGrid[row + sr][col + sc])
                        {
                            canPlace = false;
                        }
                    }
                }

                if (canPlace)
                {
                    // Mark this area as occupied
                    for (int sr = 0; sr < spriteRows; ++sr)
                    {
                        for (int sc = 0; sc < spriteCols; ++sc)
                        {
                            m_packingGrid[row + sr][col + sc] = true;
                        }
                    }

                    outPosition = IntVec2(col * m_gridResolution, row * m_gridResolution);
                    return true;
                }
            }
        }

        return false; // No space found
    }

    void TextureAtlas::InitializePackingGrid(IntVec2 atlasDimensions, int spriteSize)
    {
        int gridCols = atlasDimensions.x / spriteSize;
        int gridRows = atlasDimensions.y / spriteSize;

        m_packingGrid.clear();
        m_packingGrid.resize(gridRows, std::vector<bool>(gridCols, false));
    }
}
