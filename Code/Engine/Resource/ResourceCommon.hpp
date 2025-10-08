#pragma once
#include <string>
#include <string_view>
#include <functional>
#include <optional>
#include <memory>
#include <stdexcept>
/// Engine
#include "Engine/Core/EngineCommon.hpp"

namespace enigma::resource
{
    class IResource;

    // Resource Smart Pointer Type Definition
    using ResourcePtr     = std::shared_ptr<IResource>;
    using WeakResourcePtr = std::weak_ptr<IResource>;

    // Resource type enumeration (expanded for Neoforge compatibility)
    enum class ResourceType
    {
        TEXTURE,
        MODEL,
        SHADER,
        SOUND, // Added for audio resources (like Neoforge)
        FONT,
        JSON,
        TEXT,
        BINARY,
        BLOCKSTATE, // Neoforge compatibility
        RECIPE, // Neoforge compatibility
        LANG, // Language files
        UNKNOWN
    };

    enum class ResourceState
    {
        NOT_LOADED, // Resource not loaded
        LOADING, // Loading
        LOADED, // Loaded into memory
        LOAD_ERROR // Failed to load
    };

    /**
     * ResourceLocation - Unique identifier for resources, following Minecraft Neoforge specification
     * 
     * Based on Neoforge ResourceLocation design:
     * - Format: namespace:path (e.g., "minecraft:block/stone", "mymod:item/sword")
     * - Namespace: lowercase letters, digits, underscores, periods, hyphens (a-z0-9_.-)
     * - Path: lowercase letters, digits, underscores, periods, hyphens, forward slashes (a-z0-9_.-/)
     * - Default namespace: "minecraft" (but we use "engine" for our engine)
     * 
     * Reference: https://docs.neoforged.net/docs/misc/resourcelocation/
     */
    class ResourceLocation
    {
    public:
        // Default constructor creates an invalid ResourceLocation
        ResourceLocation() = default;

        // Main constructor with validation
        ResourceLocation(std::string_view namespaceId, std::string_view path);

        // Parse from string format "namespace:path"
        explicit ResourceLocation(std::string_view fullLocation);

        // Static factory methods (Neoforge style)
        static ResourceLocation Of(std::string_view namespaceId, std::string_view path);
        static ResourceLocation Parse(std::string_view location);
        static ResourceLocation FromNamespaceAndPath(std::string_view namespaceId, std::string_view path);

        // Tryparse version that returns optional instead of throwing
        static std::optional<ResourceLocation> TryParse(std::string_view location);

        // Create with default namespace
        static ResourceLocation WithDefaultNamespace(std::string_view path);

        // Getters
        const std::string& GetNamespace() const { return m_namespace; }
        const std::string& GetPath() const { return m_path; }

        // Utility methods
        std::string ToString() const;

        ResourceLocation WithPrefix(std::string_view prefix) const;
        ResourceLocation WithSuffix(std::string_view suffix) const;
        ResourceLocation WithPath(std::string_view newPath) const;
        ResourceLocation WithNamespace(std::string_view newNamespace) const;

        // Validation
        bool        IsValid() const;
        static bool IsValidNamespace(std::string_view namespaceId);
        static bool IsValidPath(std::string_view path);

        // Comparison operators
        bool operator==(const ResourceLocation& other) const;
        bool operator!=(const ResourceLocation& other) const;
        bool operator<(const ResourceLocation& other) const;

        // Comparison with string (Neoforge convenience)
        bool operator==(std::string_view str) const;
        bool operator!=(std::string_view str) const;

        // Hash support
        friend struct std::hash<ResourceLocation>;

    private:
        std::string m_namespace;
        std::string m_path;

        // Default namespace following Neoforge convention (but adapted for our engine)
        static constexpr std::string_view DEFAULT_NAMESPACE = "engine";

        // Validate and normalize the components
        void validateAndNormalize();
    };


    [[nodiscard]]
    std::string GetFileExtension(const std::string& filePath);
}

namespace std
{
    template <>
    struct hash<enigma::resource::ResourceLocation>
    {
        size_t operator()(const enigma::resource::ResourceLocation& loc) const noexcept
        {
            return hash<std::string>()(loc.ToString());
        }
    };
}
