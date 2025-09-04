#pragma once
#include "../ResourceLoader.hpp"
#include "../ResourceCommon.hpp"
#include "ImageResource.hpp"
#include <set>
#include <string>
#include <memory>

namespace enigma::resource
{
    /**
     * ImageLoader - Resource loader for ImageResource objects
     * Supports multiple image formats using existing stb_image integration
     * Implements IResourceLoader interface for integration with ResourceSubsystem
     */
    class ImageLoader : public IResourceLoader
    {
    public:
        ImageLoader();
        virtual ~ImageLoader() override = default;

        // IResourceLoader interface implementation
        ResourcePtr           Load(const ResourceMetadata& metadata, const std::vector<uint8_t>& data) override;
        std::set<std::string> GetSupportedExtensions() const override;
        std::string           GetLoaderName() const override;
        int                   GetPriority() const override;

        // Additional validation
        bool CanLoad(const ResourceMetadata& metadata) const override;

    private:
        // Supported image formats
        std::set<std::string> m_supportedExtensions;

        // Helper methods
        bool                   IsImageFormat(const std::string& extension) const;
        std::unique_ptr<Image> LoadImageFromData(const std::vector<uint8_t>& data, const std::string& debugName) const;
    };
}
