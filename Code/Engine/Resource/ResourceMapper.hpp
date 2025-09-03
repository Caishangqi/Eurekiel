#pragma once
#include "ResourceCommon.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>


namespace enigma::core
{
    class IRegistrable;
}

namespace enigma::resource
{
    /**
     * @brief Maps registered objects to their associated resources
     * 
     * Similar to Minecraft Forge's ResourceMapper, this class maintains
     * associations between registry objects (blocks, items, etc.) and their
     * corresponding resource files (models, textures, blockstates, etc.).
     */
    class ResourceMapper
    {
    public:
        /**
         * @brief Resource mapping entry
         */
        struct ResourceMapping
        {
            ResourceLocation              registryName; // Registry name of the object
            ResourceLocation              modelLocation; // Model resource location
            ResourceLocation              blockStateLocation; // Blockstate resource location
            std::vector<ResourceLocation> textures; // Associated texture locations

            ResourceMapping() = default;

            ResourceMapping(const ResourceLocation& registry, const ResourceLocation& model);
        };

        /**
         * @brief Resource mapping provider function type
         * Used to customize resource location generation for different object types
         */
        using ResourceMappingProvider = std::function<ResourceMapping(const enigma::core::IRegistrable*)>;

    private:
        // Registry name -> Resource mapping
        std::unordered_map<std::string, ResourceMapping> m_mappings;

        // Type-specific mapping providers
        std::unordered_map<std::string, ResourceMappingProvider> m_providers;

    public:
        ResourceMapper()  = default;
        ~ResourceMapper() = default;

        /**
         * @brief Register a resource mapping provider for a specific type
         */
        void RegisterMappingProvider(const std::string& typeName, ResourceMappingProvider provider);

        /**
         * @brief Map a registry object to its resources
         */
        void MapObject(const enigma::core::IRegistrable* object, const std::string& typeName);

        /**
         * @brief Get resource mapping for a registry object
         */
        const ResourceMapping* GetMapping(const std::string& registryName) const;

        /**
         * @brief Get resource mapping for a registry object with namespace
         */
        const ResourceMapping* GetMapping(const std::string& namespaceName, const std::string& name) const;

        /**
         * @brief Get model location for a registry object
         */
        ResourceLocation GetModelLocation(const std::string& registryName) const;

        /**
         * @brief Get blockstate location for a registry object
         */
        ResourceLocation GetBlockStateLocation(const std::string& registryName) const;

        /**
         * @brief Get all texture locations for a registry object
         */
        std::vector<ResourceLocation> GetTextureLocations(const std::string& registryName) const;

        /**
         * @brief Add texture location to an existing mapping
         */
        void AddTextureLocation(const std::string& registryName, const ResourceLocation& textureLocation);

        /**
         * @brief Update model location for an existing mapping
         */
        void UpdateModelLocation(const std::string& registryName, const ResourceLocation& modelLocation);

        /**
         * @brief Update blockstate location for an existing mapping
         */
        void UpdateBlockStateLocation(const std::string& registryName, const ResourceLocation& blockstateLocation);

        /**
         * @brief Check if mapping exists for a registry object
         */
        bool HasMapping(const std::string& registryName) const;

        /**
         * @brief Get all registry names that have mappings
         */
        std::vector<std::string> GetAllMappedNames() const;

        /**
         * @brief Clear all mappings
         */
        void Clear();

        /**
         * @brief Remove mapping for a specific object
         */
        void RemoveMapping(const std::string& registryName);

        /**
         * @brief Get total number of mappings
         */
        size_t GetMappingCount() const;

    private:
        /**
         * @brief Create default resource mapping for an object
         */
        ResourceMapping CreateDefaultMapping(const enigma::core::IRegistrable* object) const;
    };
}
