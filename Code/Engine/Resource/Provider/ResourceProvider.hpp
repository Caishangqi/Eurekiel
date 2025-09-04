#pragma once
#include "../ResourceMetadata.hpp"
#include <vector>
#include <unordered_map>

namespace enigma::resource
{
    /**
     * IResourceProvider - Interface for a resource provider
     * Responsible for providing resources from specific sources such as file systems, archives, etc.
     */
    class IResourceProvider
    {
    public:
        virtual ~IResourceProvider() = default;

        virtual std::string GetName() const = 0; //
        // Get the provider name
        virtual bool                            HasResource(const ResourceLocation& location) const = 0; // Check if the specified resource is included
        virtual std::optional<ResourceMetadata> GetMetadata(const ResourceLocation& location) const = 0; // Get resource metadata
        virtual std::vector<uint8_t>            ReadResource(const ResourceLocation& location) = 0; // Read resource data
        virtual std::vector<ResourceLocation>   ListResources(const std::string& namespace_id = "", ResourceType type = ResourceType::UNKNOWN) const = 0; // List all resources

        // Get priority (higher value, higher priority)
        virtual int GetPriority() const { return 0; }
    };

    class FileSystemResourceProvider : public IResourceProvider
    {
    public:
        FileSystemResourceProvider(const std::filesystem::path& basePath, const std::string& name);

        std::string                     GetName() const override { return m_name; }
        bool                            HasResource(const ResourceLocation& location) const override;
        std::optional<ResourceMetadata> GetMetadata(const ResourceLocation& location) const override;
        std::vector<uint8_t>            ReadResource(const ResourceLocation& location) override;
        std::vector<ResourceLocation>   ListResources(const std::string& namespace_id = "", ResourceType type = ResourceType::UNKNOWN) const override;


        void SetNamespaceMapping(const std::string& namespace_id, const std::filesystem::path& path); // Set namespace mapping (optional)
        void SetSearchExtensions(const std::vector<std::string>& extensions); // Set the file extension for the search
    private:
        std::filesystem::path                                  m_basePath;
        std::string                                            m_name;
        std::unordered_map<std::string, std::filesystem::path> m_namespaceMappings;
        std::vector<std::string>                               m_searchExtensions;

        // Convert ResourceLocation to file path (try a different extension)
        std::optional<std::filesystem::path> locationToPath(const ResourceLocation& location) const;

        // Find files that actually exist (try different extensions)
        std::optional<std::filesystem::path> findResourceFile(const std::filesystem::path& basePath) const;

        // Find resource by path (supports both with and without extension)
        std::optional<ResourceLocation> findResourceByPath(const ResourceLocation& location) const;

        // Scan directory structure
        void scanDirectory(const std::filesystem::path& dir, const std::string& namespace_id, std::vector<ResourceLocation>& results, ResourceType filterType) const;
    };

    using ResourceProviderPtr = std::unique_ptr<IResourceProvider>; // Resource provider intelligence pointers
}
