#include "ResourceMapper.hpp"
#include "../Core/Logger/LoggerAPI.hpp"
#include "Engine/Registry/Core/IRegistrable.hpp"

using namespace enigma::resource;
using namespace enigma::core;

ResourceMapper::ResourceMapping ResourceMapper::CreateDefaultMapping(const IRegistrable* object) const
{
    if (!object) return ResourceMapping();

    ResourceLocation registryLocation(object->GetNamespace(), object->GetRegistryName());
    ResourceLocation modelLocation(object->GetNamespace(), "block/" + object->GetRegistryName());

    ResourceMapping mapping(registryLocation, modelLocation);
    mapping.blockStateLocation = ResourceLocation(object->GetNamespace(), "blockstates/" + object->GetRegistryName());

    // Add default texture location
    mapping.textures.push_back(ResourceLocation(object->GetNamespace(), "block/" + object->GetRegistryName()));

    return mapping;
}

ResourceMapper::ResourceMapping::ResourceMapping(const ResourceLocation& registry, const ResourceLocation& model) : registryName(registry), modelLocation(model)
{
}

void ResourceMapper::RegisterMappingProvider(const std::string& typeName, ResourceMappingProvider provider)
{
    m_providers[typeName] = provider;
}

void ResourceMapper::MapObject(const IRegistrable* object, const std::string& typeName)
{
    if (!object) return;

    std::string fullName = object->GetNamespace() + ":" + object->GetRegistryName();

    // Check if we have a custom provider for this type
    auto providerIt = m_providers.find(typeName);
    if (providerIt != m_providers.end())
    {
        m_mappings[fullName] = providerIt->second(object);
    }
    else
    {
        // Default mapping
        m_mappings[fullName] = CreateDefaultMapping(object);
    }
}

const ResourceMapper::ResourceMapping* ResourceMapper::GetMapping(const std::string& registryName) const
{
    auto it = m_mappings.find(registryName);
    return it != m_mappings.end() ? &it->second : nullptr;
}

const ResourceMapper::ResourceMapping* ResourceMapper::GetMapping(const std::string& namespaceName, const std::string& name) const
{
    std::string fullName = namespaceName + ":" + name;
    return GetMapping(fullName);
}

ResourceLocation ResourceMapper::GetModelLocation(const std::string& registryName) const
{
    const auto* mapping = GetMapping(registryName);
    return mapping ? mapping->modelLocation : ResourceLocation();
}

ResourceLocation ResourceMapper::GetBlockStateLocation(const std::string& registryName) const
{
    const auto* mapping = GetMapping(registryName);
    return mapping ? mapping->blockStateLocation : ResourceLocation();
}

std::vector<ResourceLocation> ResourceMapper::GetTextureLocations(const std::string& registryName) const
{
    const auto* mapping = GetMapping(registryName);
    return mapping ? mapping->textures : std::vector<ResourceLocation>();
}

void ResourceMapper::AddTextureLocation(const std::string& registryName, const ResourceLocation& textureLocation)
{
    auto it = m_mappings.find(registryName);
    if (it != m_mappings.end())
    {
        it->second.textures.push_back(textureLocation);
    }
}

void ResourceMapper::UpdateModelLocation(const std::string& registryName, const ResourceLocation& modelLocation)
{
    auto it = m_mappings.find(registryName);
    if (it != m_mappings.end())
    {
        it->second.modelLocation = modelLocation;
    }
}

void ResourceMapper::UpdateBlockStateLocation(const std::string& registryName, const ResourceLocation& blockstateLocation)
{
    auto it = m_mappings.find(registryName);
    if (it != m_mappings.end())
    {
        it->second.blockStateLocation = blockstateLocation;
    }
}

bool ResourceMapper::HasMapping(const std::string& registryName) const
{
    return m_mappings.find(registryName) != m_mappings.end();
}

std::vector<std::string> ResourceMapper::GetAllMappedNames() const
{
    std::vector<std::string> names;
    names.reserve(m_mappings.size());

    for (const auto& pair : m_mappings)
    {
        names.push_back(pair.first);
    }

    return names;
}

void ResourceMapper::Clear()
{
    m_mappings.clear();
}

void ResourceMapper::RemoveMapping(const std::string& registryName)
{
    m_mappings.erase(registryName);
}

size_t ResourceMapper::GetMappingCount() const
{
    return m_mappings.size();
}
