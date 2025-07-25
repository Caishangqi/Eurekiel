#pragma once
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
struct ResourceLocation;

template <typename T>
class ResourceLoader
{
public:
    virtual                    ~ResourceLoader() = default;
    virtual bool               CanLoad(const std::string& extension) const = 0;
    virtual std::unique_ptr<T> Load(const ResourceLocation& location, const std::string& filePath) = 0;
    virtual int                GetPriority() const { return 0; }
    virtual std::string        GetLoaderName() const = 0;
};

template <typename T>
class LoaderRegistry
{
private:
    struct LoaderEntry
    {
        std::unique_ptr<ResourceLoader<T>> loader;
        std::set<std::string>              supportedExtensions;
        int                                priority;

        LoaderEntry(std::unique_ptr<ResourceLoader<T>> l, const std::set<std::string>& ext, int p) : loader(std::move(l)), supportedExtensions(ext), priority(p)
        {
        }
    };

    std::vector<LoaderEntry>                             m_loaders;
    std::unordered_map<std::string, std::vector<size_t>> m_extensionToLoaders;

    mutable std::mutex m_mutex;

public:
    void                            RegisterLoader(std::unique_ptr<ResourceLoader<T>> loader, const std::set<std::string>& extensions);
    ResourceLoader<T>*              FindLoader(const std::string& extension) const;
    std::vector<ResourceLoader<T>*> FindAllLoaders(const std::string& extension) const;
    /// Try to load the resource with the best loader
    /// @param location 
    /// @param filePath 
    /// @return 
    std::unique_ptr<T> LoadResource(const ResourceLocation& location, const std::string& filePath);
    std::unique_ptr<T> LoadResourceWithFallback(const ResourceLocation& location, const std::string& filePath);
    /// Get all registered loader information
    /// @return All registered loaders
    std::vector<std::string> GetRegisteredLoaders() const;
};
