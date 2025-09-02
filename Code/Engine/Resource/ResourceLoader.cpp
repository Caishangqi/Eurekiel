#include "ResourceLoader.hpp"

#include <algorithm>

#include "Engine/Resource/ResourceCommon.hpp"
#include "Engine/Resource/ResourceMetadata.hpp"

using namespace enigma::resource;

/**
 * Registers a resource loader to the LoaderRegistry. The loader is added to a registry
 * by its name and its supported file extensions. Resource loaders supporting the same
 * extension are sorted by their priority in descending order.
 *
 * @param loader The resource loader to register. If the loader is null, the function exits without doing anything.
 */
void LoaderRegistry::RegisterLoader(ResourceLoaderPtr loader)
{
    if (!loader) return;

    m_loadersByName[loader->GetLoaderName()] = loader;

    for (const auto& ext : loader->GetSupportedExtensions())
    {
        m_loadersByExtension[ext].push_back(loader);
        // Sort by priority
        std::sort(m_loadersByExtension[ext].begin(), m_loadersByExtension[ext].end(), [](const auto& a, const auto& b)
        {
            return a->GetPriority() > b->GetPriority();
        });
    }
}

/**
 * Unregisters a resource loader from the LoaderRegistry. The loader is removed from the
 * registry by its name and its association with file extensions is also cleared.
 *
 * @param name The name of the resource loader to unregister. If no loader is found with the given name, the function performs no action.
 */
void LoaderRegistry::UnregisterLoader(const std::string& name)
{
    auto it = m_loadersByName.find(name);
    if (it == m_loadersByName.end()) return;

    auto loader = it->second;
    m_loadersByName.erase(it);

    // Remove from extension map
    for (auto& [ext, loaders] : m_loadersByExtension)
    {
        loaders.erase(
            std::remove(loaders.begin(), loaders.end(), loader),
            loaders.end()
        );
    }
}

/**
 * Finds the most suitable resource loader for a given file extension. The loaders
 * are evaluated based on their priority, with the highest priority loader being
 * returned. If no loader is found for the provided extension, a wildcard loader
 * (if available) is used as a fallback.
 *
 * @param extension The file extension (e.g., "png", "txt") for which a loader is to be found.
 * @return A pointer to the appropriate resource loader, or nullptr if no suitable loader is found.
 */
ResourceLoaderPtr LoaderRegistry::FindLoaderByExtension(const std::string& extension) const
{
    auto it = m_loadersByExtension.find(extension);
    if (it != m_loadersByExtension.end() && !it->second.empty())
    {
        return it->second.front(); // Returns the highest priority loader
    }

    // Check the wildcard loader
    auto wildcardIt = m_loadersByExtension.find("*");
    if (wildcardIt != m_loadersByExtension.end() && !wildcardIt->second.empty())
    {
        return wildcardIt->second.front();
    }

    return nullptr;
}

/**
 * Finds and returns a resource loader capable of loading the specified resource metadata.
 * First attempts to locate a loader based on the resource's file extension. If no suitable
 * loader is found, searches through all registered loaders for one that can handle the metadata.
 *
 * @param metadata The metadata of the resource to load, which contains information
 *                 such as the file extension and other details about the resource.
 * @return A pointer to the resource loader capable of handling the given metadata.
 *         Returns nullptr if no suitable loader is found.
 */
ResourceLoaderPtr LoaderRegistry::FindLoaderForResource(const ResourceMetadata& metadata) const
{
    // First, try to find loaders by extension and test each one
    const std::string& extension = metadata.GetFileExtension();
    auto               it        = m_loadersByExtension.find(extension);
    if (it != m_loadersByExtension.end())
    {
        // Try each loader that supports this extension, starting with highest priority
        for (const auto& loader : it->second)
        {
            if (loader->CanLoad(metadata))
            {
                return loader;
            }
        }
    }

    // Fallback: iterate through all loaders and find the first one that can be loaded
    for (const auto& [name, loaderPtr] : m_loadersByName)
    {
        if (loaderPtr->CanLoad(metadata))
        {
            return loaderPtr;
        }
    }

    return nullptr;
}

/**
 * Retrieves all registered resource loaders from the LoaderRegistry.
 * Each registered loader is returned as a unique pointer in a vector.
 *
 * @return A vector containing all resource loaders currently registered in the LoaderRegistry.
 */
std::vector<ResourceLoaderPtr> LoaderRegistry::GetAllLoaders() const
{
    std::vector<ResourceLoaderPtr> result;
    for (const auto& [name, loader] : m_loadersByName)
    {
        result.push_back(loader);
    }
    return result;
}

void LoaderRegistry::Clear()
{
    m_loadersByExtension.clear();
    m_loadersByName.clear();
}

// RawResourceLoader implementation
ResourcePtr RawResourceLoader::Load(const ResourceMetadata& metadata, const std::vector<uint8_t>& data)
{
    // Create and return a RawResource instance
    return std::make_shared<RawResource>(metadata, data);
}
