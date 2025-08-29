#pragma once
#include "../ResourceCommon.hpp"
#include "../ResourceMetadata.hpp"
#include "../../Core/Image.hpp"
#include <memory>

namespace enigma::resource
{
    /**
     * ImageResource - Extends Image to become an IResource
     * Provides resource management functionality for images loaded from files
     * Supports PNG, JPG, BMP, TGA formats through existing stb_image integration
     */
    class ImageResource : public IResource
    {
    public:
        // Constructor - creates an ImageResource from a loaded image
        explicit ImageResource(const ResourceMetadata& metadata, std::unique_ptr<Image> image);
        
        // Destructor
        virtual ~ImageResource() override;

        // IResource interface implementation
        const ResourceMetadata& GetMetadata() const override;
        ResourceType GetType() const override;
        bool IsLoaded() const override;
        const void* GetRawData() const override;
        size_t GetRawDataSize() const override;

        // Image-specific interface
        const Image& GetImage() const;
        Image& GetImage();
        
        // Convenience methods
        IntVec2 GetDimensions() const;
        Rgba8 GetTexelColor(const IntVec2& texelCoords) const;
        void SetTexelColor(const IntVec2& texelCoords, const Rgba8& newColor);
        
        // Check if the image has valid dimensions for atlas building
        bool IsValidForAtlas() const;
        
        // Get the image's resolution (assumes square textures, returns width)
        int GetResolution() const;

        // Additional methods for atlas system compatibility
        ResourceLocation GetResourceLocation() const;
        void Unload();

    private:
        ResourceMetadata m_metadata;
        std::unique_ptr<Image> m_image;
        bool m_isLoaded;
    };
}