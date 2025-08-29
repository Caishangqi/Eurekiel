#pragma once
#include "../ResourceCommon.hpp"
#include "AtlasConfig.hpp"
#include "TextureAtlas.hpp"
#include "ImageResource.hpp"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>

namespace enigma::resource
{
    // Forward declaration
    class ResourceSubsystem;

    /**
     * AtlasManager - Global atlas management following Minecraft patterns
     * Handles multi-atlas support (blocks, items, particles, etc.)
     * Cross-namespace texture collection and ResourceLocation-based sprite lookup
     */
    class AtlasManager
    {
    public:
        // Constructor with resource subsystem reference
        explicit AtlasManager(ResourceSubsystem* resourceSubsystem);
        
        // Destructor
        ~AtlasManager();

        // Atlas configuration and building
        void AddAtlasConfig(const AtlasConfig& config);
        bool BuildAtlas(const std::string& atlasName);
        bool BuildAllAtlases();
        void ClearAtlas(const std::string& atlasName);
        void ClearAllAtlases();

        // Atlas access
        TextureAtlas* GetAtlas(const std::string& atlasName);
        const TextureAtlas* GetAtlas(const std::string& atlasName) const;
        std::vector<std::string> GetAtlasNames() const;

        // Sprite lookup (ResourceLocation-based)
        const AtlasSprite* FindSprite(const ResourceLocation& location) const;
        const AtlasSprite* FindSprite(const std::string& atlasName, const ResourceLocation& location) const;
        
        // Cross-namespace texture collection
        std::vector<std::shared_ptr<ImageResource>> CollectTexturesForAtlas(const AtlasConfig& config);
        std::vector<ResourceLocation> FindTexturesByPattern(const std::string& pattern, const std::vector<std::string>& namespaces = {});

        // Atlas export and debugging
        bool ExportAtlasToPNG(const std::string& atlasName, const std::string& filepath);
        bool ExportAllAtlasesToPNG(const std::string& directory = "debug/");
        void PrintAllAtlasInfo() const;
        
        // Statistics and management
        size_t GetTotalAtlasCount() const;
        size_t GetTotalSpriteCount() const;
        size_t GetTotalAtlasMemoryUsage() const;

        // Configuration management
        void SetDefaultAtlasConfigs();
        AtlasConfig CreateMinecraftStyleConfig(const std::string& atlasName, const std::string& textureDirectory);

    private:
        // Internal helper methods
        bool CollectTexturesFromDirectory(const AtlasSourceEntry& source, std::vector<std::shared_ptr<ImageResource>>& outImages);
        bool CollectTexturesFromSingle(const AtlasSourceEntry& source, std::vector<std::shared_ptr<ImageResource>>& outImages);
        bool CollectTexturesFromFilter(const AtlasSourceEntry& source, std::vector<std::shared_ptr<ImageResource>>& outImages);
        
        bool MatchesNamespaceFilter(const ResourceLocation& location, const std::vector<std::string>& namespaces) const;
        bool MatchesPattern(const std::string& str, const std::string& pattern) const;
        ResourceLocation ExtractResourceLocationFromPath(const std::string& fullPath, const std::string& baseDirectory) const;
        
        // Atlas lookup helpers
        std::string FindAtlasForSprite(const ResourceLocation& location) const;
        void RebuildSpriteLookupCache();

    private:
        ResourceSubsystem* m_resourceSubsystem;                                     // Reference to resource subsystem

        // Atlas storage
        std::unordered_map<std::string, AtlasConfig> m_atlasConfigs;               // Atlas configurations
        std::unordered_map<std::string, std::unique_ptr<TextureAtlas>> m_atlases;   // Built atlases
        
        // Sprite lookup optimization
        std::unordered_map<ResourceLocation, std::string> m_spriteToAtlasMap;       // Quick lookup: sprite -> atlas name
        bool m_lookupCacheValid;                                                    // Whether lookup cache is valid
        
        // Default configurations
        static const std::vector<std::string> DEFAULT_NAMESPACES;
    };

    /**
     * AtlasManagerFactory - Factory for creating common atlas configurations
     * Provides standard Minecraft-style configurations
     */
    class AtlasManagerFactory
    {
    public:
        // Create standard Minecraft-style atlas configurations
        static AtlasConfig CreateBlocksAtlasConfig(int resolution = 16);
        static AtlasConfig CreateItemsAtlasConfig(int resolution = 16);
        static AtlasConfig CreateParticlesAtlasConfig(int resolution = 16);
        static AtlasConfig CreateUIAtlasConfig(int resolution = 16);
        
        // Create custom atlas configuration
        static AtlasConfig CreateCustomAtlasConfig(const std::string& name, 
                                                 const std::string& textureDirectory,
                                                 const std::vector<std::string>& namespaces = {},
                                                 int resolution = 16);
        
        // Setup default atlas manager with common configurations
        static void SetupDefaultAtlases(AtlasManager& manager);
    };
}