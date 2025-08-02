#include "ResourceSubsystem.hpp"
using namespace enigma::resource;

bool ResourceConfig::IsValid()
{
    return !baseAssetPath.empty() &&
        maxCacheSize > 0 &&
        maxCachedResources > 0 &&
        loadThreadCount > 0 &&
        hotReloadCheckInterval > 0 &&
        performanceLimits.maxLoadAttemptsPerFrame > 0 &&
        performanceLimits.maxBytesPerFrame > 0 &&
        performanceLimits.maxLoadTimePerFrame > 0;
}

void ResourceConfig::AddNamespace(const std::string& name, const std::filesystem::path& path)
{
    namespaces.push_back({name, path, true, false, {}});
}

void ResourceConfig::EnableNamespacePreload(const std::string& name, const std::vector<std::string>& patterns)
{
    for (auto& ns : namespaces)
    {
        if (ns.name == name)
        {
            ns.preloadAll      = patterns.empty();
            ns.preloadPatterns = patterns;
            break;
        }
    }
}

ResourceType ResourceConfig::GetTypeForExtension(const std::string& ext) const
{
    for (const auto& mapping : extensionMappings)
    {
        if (mapping.extension == ext)
        {
            return mapping.type;
        }
    }
    return ResourceType::UNKNOWN;
}
