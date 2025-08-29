#include "ImageResource.hpp"
#include "../../Core/ErrorWarningAssert.hpp"

namespace enigma::resource
{
    ImageResource::ImageResource(const ResourceMetadata& metadata, std::unique_ptr<Image> image)
        : m_metadata(metadata)
        , m_image(std::move(image))
        , m_isLoaded(m_image != nullptr)
    {
        GUARANTEE_OR_DIE(m_image != nullptr, "ImageResource: Cannot create resource with null image");
    }

    ImageResource::~ImageResource()
    {
        Unload();
    }

    const ResourceMetadata& ImageResource::GetMetadata() const
    {
        return m_metadata;
    }

    ResourceType ImageResource::GetType() const
    {
        return ResourceType::TEXTURE;
    }

    bool ImageResource::IsLoaded() const
    {
        return m_isLoaded && m_image != nullptr;
    }

    const void* ImageResource::GetRawData() const
    {
        if (!IsLoaded())
            return nullptr;
        return m_image->GetRawData();
    }

    size_t ImageResource::GetRawDataSize() const
    {
        if (!IsLoaded())
            return 0;
        
        IntVec2 dimensions = m_image->GetDimensions();
        return dimensions.x * dimensions.y * 4; // RGBA = 4 bytes per pixel
    }

    const Image& ImageResource::GetImage() const
    {
        GUARANTEE_OR_DIE(IsLoaded(), "ImageResource: Attempting to access unloaded image");
        return *m_image;
    }

    Image& ImageResource::GetImage()
    {
        GUARANTEE_OR_DIE(IsLoaded(), "ImageResource: Attempting to access unloaded image");
        return *m_image;
    }

    IntVec2 ImageResource::GetDimensions() const
    {
        if (!IsLoaded())
            return IntVec2::ZERO;
        return m_image->GetDimensions();
    }

    Rgba8 ImageResource::GetTexelColor(const IntVec2& texelCoords) const
    {
        GUARANTEE_OR_DIE(IsLoaded(), "ImageResource: Attempting to access unloaded image");
        return m_image->GetTexelColor(texelCoords);
    }

    void ImageResource::SetTexelColor(const IntVec2& texelCoords, const Rgba8& newColor)
    {
        GUARANTEE_OR_DIE(IsLoaded(), "ImageResource: Attempting to modify unloaded image");
        m_image->SetTexelColor(texelCoords, newColor);
    }

    bool ImageResource::IsValidForAtlas() const
    {
        if (!IsLoaded())
            return false;
        
        IntVec2 dimensions = GetDimensions();
        
        // Check for valid dimensions (must be positive and power-of-2 friendly)
        return dimensions.x > 0 && dimensions.y > 0;
    }

    int ImageResource::GetResolution() const
    {
        if (!IsLoaded())
            return 0;
        
        IntVec2 dimensions = GetDimensions();
        
        // Return width (assuming square textures following Minecraft pattern)
        return dimensions.x;
    }

    ResourceLocation ImageResource::GetResourceLocation() const
    {
        return m_metadata.location;
    }

    void ImageResource::Unload()
    {
        m_image.reset();
        m_isLoaded = false;
    }
}