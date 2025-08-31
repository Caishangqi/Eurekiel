#pragma once
#include "ResourceMetadata.hpp"
#include <set>
#include <unordered_map>

namespace enigma::resource
{
    /**
     * IResourceLoader - Interface for resource loaders
     * Responsible for parsing raw data into specific resource objects
     */
    class IResourceLoader
    {
    public:
        virtual ~IResourceLoader() = default;

        virtual ResourcePtr           Load(const ResourceMetadata& metadata, const std::vector<uint8_t>& data) = 0; // Load the resource
        virtual std::set<std::string> GetSupportedExtensions() const = 0; // Get supported file extensions
        virtual std::string           GetLoaderName() const = 0; // Get the loader name
        virtual int                   GetPriority() const { return 0; } // Get priority (higher value, higher priority)

        // Check if this resource can be loaded
        virtual bool CanLoad(const ResourceMetadata& metadata) const
        {
            auto ext        = metadata.GetFileExtension();
            auto extensions = GetSupportedExtensions();
            return extensions.find(ext) != extensions.end();
        }
    };

    using ResourceLoaderPtr = std::shared_ptr<IResourceLoader>;

    /**
     * RawResourceLoader - Class for loading raw resources
     * Implements the IResourceLoader interface to provide basic support for handling raw resource data.
     * This loader processes raw data without interpreting its specific structure or content.
     */
    class RawResourceLoader : public IResourceLoader
    {
    public:
        ResourcePtr           Load(const ResourceMetadata& metadata, const std::vector<uint8_t>& data) override;
        std::set<std::string> GetSupportedExtensions() const override { return {"*"}; }
        std::string           GetLoaderName() const override { return "RawResourceLoader"; }
        int                   GetPriority() const override { return -1000; } // Minimum priority
        bool                  CanLoad(const ResourceMetadata& metadata) const override
        {
            UNUSED(metadata)
            return true;
        }
    };


    /**
     * LoaderRegistry - Registry for managing resource loaders
     * Facilitates the registration, unregistration, and retrieval of resource loaders
     * Maintains loaders indexed by file extension and loader names
     */
    class LoaderRegistry
    {
    public:
        void                           RegisterLoader(ResourceLoaderPtr loader); // Register the loader
        void                           UnregisterLoader(const std::string& name); // Remove the loader
        ResourceLoaderPtr              FindLoaderByExtension(const std::string& extension) const; // Find loaders based on file extensions
        ResourceLoaderPtr              FindLoaderForResource(const ResourceMetadata& metadata) const; // Find the appropriate loader based on the resource metadata
        std::vector<ResourceLoaderPtr> GetAllLoaders() const; // Get all registered loaders
        void                           Clear(); // Empty all loaders

    private:
        std::unordered_map<std::string, std::vector<ResourceLoaderPtr>> m_loadersByExtension; // Loader indexed by extension
        std::unordered_map<std::string, ResourceLoaderPtr>              m_loadersByName;
    };
}
