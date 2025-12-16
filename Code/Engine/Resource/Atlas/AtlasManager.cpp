#include "AtlasManager.hpp"
#include "../ResourceSubsystem.hpp"
#include "../../Core/ErrorWarningAssert.hpp"
#include "../../Core/StringUtils.hpp"
#include "../../Core/FileSystemHelper.hpp"
#include <algorithm>
#include <filesystem>
#include <regex>

#include "Engine/Core/Logger/LoggerAPI.hpp"
using namespace enigma::core;
DEFINE_LOG_CATEGORY(LogAtlas)

namespace enigma::resource
{
    // Default namespaces for atlas collection
    const std::vector<std::string> AtlasManager::DEFAULT_NAMESPACES = {
        "engine", "minecraft", "simpleminer"
    };

    AtlasManager::AtlasManager(ResourceSubsystem* resourceSubsystem)
        : m_resourceSubsystem(resourceSubsystem)
          , m_lookupCacheValid(false)
    {
        GUARANTEE_OR_DIE(m_resourceSubsystem != nullptr, "AtlasManager: ResourceSubsystem cannot be null");
    }

    AtlasManager::~AtlasManager()
    {
        ClearAllAtlases();
    }

    void AtlasManager::AddAtlasConfig(const AtlasConfig& config)
    {
        if (!config.IsValid())
        {
            ERROR_AND_DIE(Stringf("AtlasManager: Invalid atlas configuration for '%s'", config.name.c_str()));
        }

        m_atlasConfigs[config.name] = config;
        m_lookupCacheValid          = false;
    }

    bool AtlasManager::BuildAtlas(const std::string& atlasName)
    {
        auto configIt = m_atlasConfigs.find(atlasName);
        if (configIt == m_atlasConfigs.end())
        {
            return false;
        }

        const AtlasConfig& config = configIt->second;

        // Collect textures for this atlas
        std::vector<std::shared_ptr<ImageResource>> images = CollectTexturesForAtlas(config);

        if (images.empty())
        {
            LogWarn(LogAtlas, "No textures found for atlas '%s'", atlasName.c_str());
            return false;
        }

        LogInfo(LogAtlas, "Building atlas '%s' with %zu textures", atlasName.c_str(), images.size());

        // Create or get existing atlas
        auto atlasIt = m_atlases.find(atlasName);
        if (atlasIt == m_atlases.end())
        {
            m_atlases[atlasName] = std::make_unique<TextureAtlas>(config);
        }
        else
        {
            m_atlases[atlasName]->ClearAtlas();
        }

        // Build the atlas
        bool success = m_atlases[atlasName]->BuildAtlas(images);

        if (success)
        {
            m_lookupCacheValid = false;
            LogInfo(LogAtlas, "Atlas '%s' built successfully", atlasName.c_str());

            // Export to PNG if configured
            if (config.exportPNG)
            {
                std::string exportPath = config.exportPath + "atlas_" + atlasName + ".png";
                ExportAtlasToPNG(atlasName, exportPath);
            }
        }
        else
        {
            LogInfo(LogAtlas, "Atlas '%s' failed to build\n", atlasName.c_str());
            m_atlases.erase(atlasName);
        }

        return success;
    }

    bool AtlasManager::BuildAllAtlases()
    {
        bool allSuccess = true;

        for (const auto& configPair : m_atlasConfigs)
        {
            if (!BuildAtlas(configPair.first))
            {
                allSuccess = false;
            }
        }

        if (allSuccess)
        {
            RebuildSpriteLookupCache();
        }

        return allSuccess;
    }

    void AtlasManager::ClearAtlas(const std::string& atlasName)
    {
        auto it = m_atlases.find(atlasName);
        if (it != m_atlases.end())
        {
            it->second->ClearAtlas();
            m_atlases.erase(it);
            m_lookupCacheValid = false;
        }
    }

    void AtlasManager::ClearAllAtlases()
    {
        m_atlases.clear();
        m_spriteToAtlasMap.clear();
        m_lookupCacheValid = false;
    }

    TextureAtlas* AtlasManager::GetAtlas(const std::string& atlasName)
    {
        auto it = m_atlases.find(atlasName);
        return (it != m_atlases.end()) ? it->second.get() : nullptr;
    }

    const TextureAtlas* AtlasManager::GetAtlas(const std::string& atlasName) const
    {
        auto it = m_atlases.find(atlasName);
        return (it != m_atlases.end()) ? it->second.get() : nullptr;
    }

    std::vector<std::string> AtlasManager::GetAtlasNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_atlases.size());

        for (const auto& atlasPair : m_atlases)
        {
            names.push_back(atlasPair.first);
        }

        return names;
    }

    const AtlasSprite* AtlasManager::FindSprite(const ResourceLocation& location) const
    {
        if (!m_lookupCacheValid)
        {
            const_cast<AtlasManager*>(this)->RebuildSpriteLookupCache();
        }

        auto it = m_spriteToAtlasMap.find(location);
        if (it != m_spriteToAtlasMap.end())
        {
            const TextureAtlas* atlas = GetAtlas(it->second);
            if (atlas)
            {
                return atlas->FindSprite(location);
            }
        }

        return nullptr;
    }

    const AtlasSprite* AtlasManager::FindSprite(const std::string& atlasName, const ResourceLocation& location) const
    {
        const TextureAtlas* atlas = GetAtlas(atlasName);
        return atlas ? atlas->FindSprite(location) : nullptr;
    }

    std::vector<std::shared_ptr<ImageResource>> AtlasManager::CollectTexturesForAtlas(const AtlasConfig& config)
    {
        std::vector<std::shared_ptr<ImageResource>> images;

        for (const AtlasSourceEntry& source : config.sources)
        {
            switch (source.type)
            {
            case AtlasSourceType::DIRECTORY:
                CollectTexturesFromDirectory(source, images);
                break;
            case AtlasSourceType::SINGLE:
                CollectTexturesFromSingle(source, images);
                break;
            case AtlasSourceType::FILTER:
                CollectTexturesFromFilter(source, images);
                break;
            }
        }

        return images;
    }

    std::vector<ResourceLocation> AtlasManager::FindTexturesByPattern(const std::string& pattern, const std::vector<std::string>& namespaces)
    {
        std::vector<ResourceLocation> matches;

        // Get all texture resources from the resource subsystem
        std::vector<ResourceLocation> allTextures = m_resourceSubsystem->ListResources("", ResourceType::TEXTURE);

        for (const ResourceLocation& location : allTextures)
        {
            // Check namespace filter
            if (!namespaces.empty() && !MatchesNamespaceFilter(location, namespaces))
            {
                continue;
            }

            // Check pattern match
            if (MatchesPattern(location.GetPath(), pattern))
            {
                matches.push_back(location);
            }
        }

        return matches;
    }

    bool AtlasManager::ExportAtlasToPNG(const std::string& atlasName, const std::string& filepath)
    {
        const TextureAtlas* atlas = GetAtlas(atlasName);
        if (!atlas)
        {
            return false;
        }

        return atlas->ExportToPNG(filepath);
    }

    bool AtlasManager::ExportAllAtlasesToPNG(const std::string& directory)
    {
        bool allSuccess = true;

        for (const auto& atlasPair : m_atlases)
        {
            std::string filename = "atlas_" + atlasPair.first + ".png";
            std::string fullPath = directory + filename;

            if (!ExportAtlasToPNG(atlasPair.first, fullPath))
            {
                allSuccess = false;
            }
        }

        return allSuccess;
    }

    void AtlasManager::PrintAllAtlasInfo() const
    {
        printf("\n=== Atlas Manager Info ===\n");
        printf("Total Atlases: %zu\n", m_atlases.size());
        printf("Total Sprites: %zu\n", GetTotalSpriteCount());
        printf("Total Memory: %.2f KB\n", GetTotalAtlasMemoryUsage() / 1024.0f);

        for (const auto& atlasPair : m_atlases)
        {
            atlasPair.second->PrintAtlasInfo();
        }
    }

    size_t AtlasManager::GetTotalAtlasCount() const
    {
        return m_atlases.size();
    }

    size_t AtlasManager::GetTotalSpriteCount() const
    {
        size_t totalSprites = 0;
        for (const auto& atlasPair : m_atlases)
        {
            totalSprites += atlasPair.second->GetStats().totalSprites;
        }
        return totalSprites;
    }

    size_t AtlasManager::GetTotalAtlasMemoryUsage() const
    {
        size_t totalMemory = 0;
        for (const auto& atlasPair : m_atlases)
        {
            totalMemory += atlasPair.second->GetStats().atlasSizeBytes;
        }
        return totalMemory;
    }

    void AtlasManager::SetDefaultAtlasConfigs()
    {
        // Discover available namespaces and use them for atlas creation
        std::vector<std::string> discoveredNamespaces = DiscoverAvailableNamespaces();

        AddAtlasConfig(AtlasManagerFactory::CreateBlocksAtlasConfig(discoveredNamespaces, 16));
        AddAtlasConfig(AtlasManagerFactory::CreateItemsAtlasConfig(discoveredNamespaces, 16));
        AddAtlasConfig(AtlasManagerFactory::CreateParticlesAtlasConfig(16)); // Particles still use engine only
    }

    AtlasConfig AtlasManager::CreateMinecraftStyleConfig(const std::string& atlasName, const std::string& textureDirectory)
    {
        return AtlasManagerFactory::CreateCustomAtlasConfig(atlasName, textureDirectory, DEFAULT_NAMESPACES, 16);
    }

    bool AtlasManager::CollectTexturesFromDirectory(const AtlasSourceEntry& source, std::vector<std::shared_ptr<ImageResource>>& outImages)
    {
        // Get all texture resources matching the directory pattern
        std::vector<ResourceLocation> textureLocations = FindTexturesByPattern(source.source + "*", source.namespaces);

        for (const ResourceLocation& location : textureLocations)
        {
            ResourcePtr resource = m_resourceSubsystem->GetResource(location);
            if (resource)
            {
                // Try to cast to ImageResource
                auto imageResource = std::dynamic_pointer_cast<ImageResource>(resource);
                if (imageResource && imageResource->IsLoaded())
                {
                    outImages.push_back(imageResource);
                }
            }
        }

        return !outImages.empty();
    }

    bool AtlasManager::CollectTexturesFromSingle(const AtlasSourceEntry& source, std::vector<std::shared_ptr<ImageResource>>& outImages)
    {
        // Parse ResourceLocation from source string
        ResourceLocation location;
        if (source.source.find(':') != std::string::npos)
        {
            location = ResourceLocation(source.source);
        }
        else
        {
            // Assume default namespace
            location = ResourceLocation("engine", source.source);
        }

        ResourcePtr resource = m_resourceSubsystem->GetResource(location);
        if (resource)
        {
            auto imageResource = std::dynamic_pointer_cast<ImageResource>(resource);
            if (imageResource && imageResource->IsLoaded())
            {
                outImages.push_back(imageResource);
                return true;
            }
        }

        return false;
    }

    bool AtlasManager::CollectTexturesFromFilter(const AtlasSourceEntry& source, std::vector<std::shared_ptr<ImageResource>>& outImages)
    {
        std::vector<ResourceLocation> allTextures = m_resourceSubsystem->ListResources("", ResourceType::TEXTURE);

        for (const ResourceLocation& location : allTextures)
        {
            bool shouldInclude = false;

            // Check include patterns
            if (source.includePatterns.empty())
            {
                shouldInclude = true;
            }
            else
            {
                for (const std::string& pattern : source.includePatterns)
                {
                    if (MatchesPattern(location.GetPath(), pattern))
                    {
                        shouldInclude = true;
                        break;
                    }
                }
            }

            // Check exclude patterns
            if (shouldInclude)
            {
                for (const std::string& pattern : source.excludePatterns)
                {
                    if (MatchesPattern(location.GetPath(), pattern))
                    {
                        shouldInclude = false;
                        break;
                    }
                }
            }

            // Check namespace filter
            if (shouldInclude && !MatchesNamespaceFilter(location, source.namespaces))
            {
                shouldInclude = false;
            }

            if (shouldInclude)
            {
                ResourcePtr resource = m_resourceSubsystem->GetResource(location);
                if (resource)
                {
                    auto imageResource = std::dynamic_pointer_cast<ImageResource>(resource);
                    if (imageResource && imageResource->IsLoaded())
                    {
                        outImages.push_back(imageResource);
                    }
                }
            }
        }

        return !outImages.empty();
    }

    bool AtlasManager::MatchesNamespaceFilter(const ResourceLocation& location, const std::vector<std::string>& namespaces) const
    {
        if (namespaces.empty())
        {
            return true;
        }

        return std::find(namespaces.begin(), namespaces.end(), location.GetNamespace()) != namespaces.end();
    }

    bool AtlasManager::MatchesPattern(const std::string& str, const std::string& pattern) const
    {
        // Simple wildcard matching (* for any characters)
        if (pattern == "*")
        {
            return true;
        }

        if (pattern.find('*') == std::string::npos)
        {
            // No wildcards, exact match
            return str == pattern;
        }

        // Convert wildcard pattern to regex
        std::string regexPattern = pattern;
        std::replace(regexPattern.begin(), regexPattern.end(), '*', '.');
        regexPattern = std::regex_replace(regexPattern, std::regex("\\."), ".*");

        try
        {
            std::regex regex(regexPattern);
            return std::regex_match(str, regex);
        }
        catch (const std::regex_error&)
        {
            // Fallback to simple contains check
            return str.find(pattern.substr(0, pattern.find('*'))) != std::string::npos;
        }
    }

    std::string AtlasManager::FindAtlasForSprite(const ResourceLocation& location) const
    {
        for (const auto& atlasPair : m_atlases)
        {
            if (atlasPair.second->FindSprite(location) != nullptr)
            {
                return atlasPair.first;
            }
        }
        return "";
    }

    void AtlasManager::RebuildSpriteLookupCache()
    {
        m_spriteToAtlasMap.clear();

        for (const auto& atlasPair : m_atlases)
        {
            const std::string&  atlasName = atlasPair.first;
            const TextureAtlas* atlas     = atlasPair.second.get();

            for (const AtlasSprite& sprite : atlas->GetAllSprites())
            {
                m_spriteToAtlasMap[sprite.location] = atlasName;
            }
        }

        m_lookupCacheValid = true;
    }

    std::vector<std::string> AtlasManager::DiscoverAvailableNamespaces() const
    {
        std::vector<std::string> namespaces;

        // Get base assets path from resource subsystem
        std::filesystem::path basePath = m_resourceSubsystem->GetConfig().baseAssetPath;

        try
        {
            if (std::filesystem::exists(basePath) && std::filesystem::is_directory(basePath))
            {
                for (const auto& entry : std::filesystem::directory_iterator(basePath))
                {
                    if (entry.is_directory())
                    {
                        std::string namespaceName = entry.path().filename().string();
                        // Filter out hidden/system directories
                        if (!namespaceName.empty() && namespaceName[0] != '.')
                        {
                            namespaces.push_back(namespaceName);
                        }
                    }
                }
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            core::LogError("AtlasManager: Failed to discover available namespaces: %s", e.what());
            // Log error but don't fail - fallback to defaults
            // TODO: Add proper logging here
        }

        // If no namespaces found, use defaults
        if (namespaces.empty())
        {
            namespaces = DEFAULT_NAMESPACES;
        }

        return namespaces;
    }

    // AtlasManagerFactory implementation
    AtlasConfig AtlasManagerFactory::CreateBlocksAtlasConfig(int resolution)
    {
        AtlasConfig config("blocks");
        config.requiredResolution = resolution;
        // Keep hardcoded for backward compatibility - use overload for auto-discovery
        config.AddDirectorySource("textures/block/", {"minecraft", "testmod", "simpleminer"});
        config.exportPNG = true;
        return config;
    }

    AtlasConfig AtlasManagerFactory::CreateBlocksAtlasConfig(const std::vector<std::string>& namespaces, int resolution)
    {
        AtlasConfig config("blocks");
        config.requiredResolution = resolution;
        config.AddDirectorySource("textures/block/", namespaces);
        config.exportPNG = true;
        return config;
    }

    AtlasConfig AtlasManagerFactory::CreateItemsAtlasConfig(int resolution)
    {
        AtlasConfig config("items");
        config.requiredResolution = resolution;
        // Keep hardcoded for backward compatibility - use overload for auto-discovery
        config.AddDirectorySource("textures/item/", {"minecraft", "testmod", "simpleminer"});
        config.exportPNG = true;
        return config;
    }

    AtlasConfig AtlasManagerFactory::CreateItemsAtlasConfig(const std::vector<std::string>& namespaces, int resolution)
    {
        AtlasConfig config("items");
        config.requiredResolution = resolution;
        config.AddDirectorySource("textures/item/", namespaces);
        config.exportPNG = true;
        return config;
    }

    AtlasConfig AtlasManagerFactory::CreateParticlesAtlasConfig(int resolution)
    {
        AtlasConfig config("particles");
        config.requiredResolution = resolution;
        config.AddDirectorySource("textures/particles/", {"engine"});
        config.exportPNG = true;
        return config;
    }

    AtlasConfig AtlasManagerFactory::CreateUIAtlasConfig(int resolution)
    {
        AtlasConfig config("ui");
        config.requiredResolution = resolution;
        config.AddDirectorySource("textures/ui/", {"engine"});
        config.exportPNG = true;
        return config;
    }

    AtlasConfig AtlasManagerFactory::CreateCustomAtlasConfig(const std::string&              name,
                                                             const std::string&              textureDirectory,
                                                             const std::vector<std::string>& namespaces,
                                                             int                             resolution)
    {
        AtlasConfig config(name);
        config.requiredResolution = resolution;
        config.AddDirectorySource(textureDirectory, namespaces);
        config.exportPNG = true;
        return config;
    }

    void AtlasManagerFactory::SetupDefaultAtlases(AtlasManager& manager)
    {
        // Use auto-discovered namespaces for dynamic atlas setup
        std::vector<std::string> discoveredNamespaces = manager.DiscoverAvailableNamespaces();

        manager.AddAtlasConfig(CreateBlocksAtlasConfig(discoveredNamespaces, 16));
        manager.AddAtlasConfig(CreateItemsAtlasConfig(discoveredNamespaces, 16));
        manager.AddAtlasConfig(CreateParticlesAtlasConfig(16));
    }
}
