/**
 * @file Sprite.cpp
 * @brief Sprite class implementation
 */

#include "Sprite.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

namespace enigma::graphic
{
    // ==================== Constructor ====================

    Sprite::Sprite(
        std::shared_ptr<D12Texture> texture,
        const AABB2&                uvBounds,
        const std::string&          name,
        const Vec2&                 pivot,
        const IntVec2&              dimensions
    )
        : m_texture(std::move(texture))
          , m_uvBounds(uvBounds)
          , m_pivot(pivot)
          , m_dimensions(dimensions)
          , m_name(name)
    {
        // If dimensions not specified and texture is valid, use texture dimensions
        if (m_dimensions.x == 0 && m_dimensions.y == 0 && m_texture)
        {
            m_dimensions.x = static_cast<int>(m_texture->GetWidth());
            m_dimensions.y = static_cast<int>(m_texture->GetHeight());
        }
    }

    // ==================== Static factory method ====================

    Sprite Sprite::CreateFromImage(
        const char* imagePath,
        const char* name,
        const Vec2& pivot
    )
    {
        // Step 1: Load image data
        Image image(imagePath);

        // Validate if image loaded successfully
        IntVec2 imageDimensions = image.GetDimensions();
        if (imageDimensions.x == 0 || imageDimensions.y == 0)
        {
            ERROR_RECOVERABLE(Stringf("Sprite::CreateFromImage: Failed to load image from path: %s", imagePath));
            return Sprite(); // Return invalid Sprite
        }

        // Step 2: Prepare texture creation parameters
        TextureCreateInfo info;
        info.type        = TextureType::Texture2D;
        info.width       = static_cast<uint32_t>(imageDimensions.x);
        info.height      = static_cast<uint32_t>(imageDimensions.y);
        info.depth       = 1;
        info.mipLevels   = 1; // Sprites typically don't need mipmaps
        info.arraySize   = 1;
        info.format      = DXGI_FORMAT_R8G8B8A8_UNORM; // Standard RGBA8 format
        info.usage       = TextureUsage::ShaderResource; // Use as shader resource
        info.initialData = image.GetRawData();
        info.dataSize    = static_cast<size_t>(imageDimensions.x * imageDimensions.y * sizeof(Rgba8));
        info.rowPitch    = static_cast<uint32_t>(imageDimensions.x * sizeof(Rgba8)); // Row pitch (bytes)
        info.slicePitch  = static_cast<uint32_t>(info.dataSize); // Slice pitch (entire data size for 2D texture)
        info.debugName   = name;

        // Step 3: Create D12Texture
        auto texture = std::make_shared<D12Texture>(info);

        // Validate if texture created successfully
        if (!texture->IsValid())
        {
            ERROR_RECOVERABLE(Stringf("Sprite::CreateFromImage: Failed to create D12Texture for: %s", name));
            return Sprite(); // Return invalid Sprite
        }

        // Step 4: Construct Sprite object
        // UV bounds are full texture (0,0,1,1)
        AABB2 fullUVBounds(0.0f, 0.0f, 1.0f, 1.0f);

        return Sprite(
            std::move(texture),
            fullUVBounds,
            name,
            pivot,
            imageDimensions
        );
    }
} // namespace enigma::graphic
