#include "Loader.hpp"

#include <algorithm>

#include "Engine/Resource/ResourceCommon.hpp"

using namespace resource;

template <typename T>
void LoaderRegistry<T>::RegisterLoader(std::unique_ptr<ResourceLoader<T>> loader, const std::set<std::string>& extensions)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    int                          priority    = loader->GetPriority();
    size_t                       loaderIndex = m_loaders.size();

    // Add to main list
    m_loaders.emplace_back(std::move(loader), extensions, priority);

    // Create a mapping for each extension
    for (const auto& ext : extensions)
    {
        m_extensionToLoaders[ext].push_back(loaderIndex);

        // Sort by priority (highest priority first)
        auto& loaderIndices = m_extensionToLoaders[ext];
        std::sort(loaderIndices.begin(), loaderIndices.end(),
                  [this](size_t a, size_t b)
                  {
                      return m_loaders[a].priority > m_loaders[b].priority;
                  });
    }
}

template <typename T>
ResourceLoader<T>* LoaderRegistry<T>::FindLoader(const std::string& extension) const
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto                         it = m_extensionToLoaders.find(extension);
    if (it != m_extensionToLoaders.end() && !it->second.empty())
    {
        // Return the loader with the highest priority
        size_t bestLoaderIndex = it->second[0];
        return m_loaders[bestLoaderIndex].loader.get();
    }
    return nullptr;
}

template <typename T>
std::vector<ResourceLoader<T>*> LoaderRegistry<T>::FindAllLoaders(const std::string& extension) const
{
    std::unique_lock<std::mutex>    lock(m_mutex);
    std::vector<ResourceLoader<T>*> result;
    auto                            it = m_extensionToLoaders.find(extension);
    if (it != m_extensionToLoaders.end())
    {
        for (size_t index : it->second)
        {
            result.push_back(m_loaders[index].loader.get());
        }
    }
    return result;
}

template <typename T>
std::unique_ptr<T> LoaderRegistry<T>::LoadResource(const ResourceLocation& location, const std::string& filePath)
{
    std::string extension = GetFileExtension(filePath);
    auto*       loader    = FindLoader(extension);
    if (loader)
    {
        try
        {
            return loader->Load(location, filePath);
        }
        catch (const std::exception& e)
        {
            // Loading failed
            LogError("Failed to load resource with " + loader->GetLoaderName() + ": " + e.what());
        }
    }
    return nullptr;
}

template <typename T>
std::unique_ptr<T> LoaderRegistry<T>::LoadResourceWithFallback(const ResourceLocation& location, const std::string& filePath)
{
    std::string extension = GetFileExtension(filePath);
    auto        loaders   = FindAllLoaders(extension);

    for (auto* loader : loaders)
    {
        try
        {
            auto result = loader->Load(location, filePath);
            if (result)
            {
                return result;
            }
        }
        catch (const std::exception& e)
        {
            LogWarning("Loader " + loader->GetLoaderName() +
                " failed, trying next: " + e.what());
            continue; // Try the next loader
        }
    }

    return nullptr;
}

template <typename T>
std::vector<std::string> LoaderRegistry<T>::GetRegisteredLoaders() const
{
    std::unique_lock<std::mutex> lock(m_mutex);
    std::vector<std::string>     result;
    for (const auto& entry : m_loaders)
    {
        std::string info = entry.loader->GetLoaderName() + " (Priority: " +
            std::to_string(entry.priority) + ", Extensions: ";
        for (const auto& ext : entry.supportedExtensions)
        {
            info += ext + " ";
        }
        info += ")";
        result.push_back(info);
    }
    return result;
}

