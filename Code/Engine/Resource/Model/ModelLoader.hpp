#pragma once
#include "../ResourceLoader.hpp"
#include "../ResourceCommon.hpp"
#include "ModelResource.hpp"
#include <set>
#include <string>
#include <memory>

namespace enigma::resource::model
{
    using namespace enigma::resource;

    /**
     * ModelLoader - Resource loader for ModelResource objects
     * Supports JSON model files following Minecraft model format
     * Implements IResourceLoader interface for integration with ResourceSubsystem
     */
    class ModelLoader : public IResourceLoader
    {
    public:
        ModelLoader();
        virtual ~ModelLoader() override = default;

        // IResourceLoader interface implementation
        ResourcePtr           Load(const ResourceMetadata& metadata, const std::vector<uint8_t>& data) override;
        std::set<std::string> GetSupportedExtensions() const override;
        std::string           GetLoaderName() const override;
        int                   GetPriority() const override;

        // Additional validation
        bool CanLoad(const ResourceMetadata& metadata) const override;

    private:
        // Supported model formats
        std::set<std::string> m_supportedExtensions;

        // Helper methods
        bool                           IsModelFormat(const std::string& extension) const;
        std::shared_ptr<ModelResource> LoadModelFromJson(const std::vector<uint8_t>& data, const ResourceLocation& location) const;
    };
}
