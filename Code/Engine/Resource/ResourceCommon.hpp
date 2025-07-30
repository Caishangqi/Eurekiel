#pragma once
#include <string>
#include <string_view>
#include <functional>
#include <optional>
#include <memory>
/// Engine
#include "Engine/Core/EngineCommon.hpp"

namespace enigma::resource
{
    class IResource;

    // Resource Smart Pointer Type Definition
    using ResourcePtr     = std::shared_ptr<IResource>;
    using WeakResourcePtr = std::weak_ptr<IResource>;

    // Resource type enumeration
    enum class ResourceType
    {
        TEXTURE,
        MODEL,
        SHADER,
        AUDIO,
        FONT,
        JSON,
        TEXT,
        BINARY,
        UNKNOWN
    };

    enum class ResourceState
    {
        NOT_LOADED, // Resource not loaded
        LOADING, // Loading
        LOADED, // Loaded into memory
        ERROR // Failed to load
    };

    /**
     * ResourceLocation - Unique identifier of the resource \n
     * ResourceLocation design with reference to NeoForge \n
     * specification: namespace:path (for example: engine:textures/gui/background) \n
     * Note: does not include file extensions \n
     */
    class ResourceLocation
    {
    public:
        ResourceLocation() = default;
        ResourceLocation(std::string_view namespace_id, std::string_view path);
        ResourceLocation(std::string_view fullLocation); // Supports the "namespace:path" format

        // Parse the string in the format "namespace:path".
        static std::optional<ResourceLocation> Parse(std::string_view location);
        // Use the default namespace to create
        static ResourceLocation WithDefaultNamespace(std::string_view path);

        // Getter
        const std::string& GetNamespace() const { return m_namespace; }
        const std::string& GetPath() const { return m_path; }

        // Practical approach
        std::string      ToString() const;
        ResourceLocation WithPrefix(std::string_view prefix) const;
        ResourceLocation WithSuffix(std::string_view suffix) const;
        ResourceLocation WithPath(std::string_view newPath) const;

        // Comparison operator
        bool operator==(const ResourceLocation& other) const;
        bool operator!=(const ResourceLocation& other) const;
        bool operator<(const ResourceLocation& other) const;

        // Validation Methods
        static bool IsValidNamespace(std::string_view namespace_id);
        static bool IsValidPath(std::string_view path);

        // Hash support
        friend struct std::hash<ResourceLocation>;

    private:
        std::string m_namespace;
        std::string m_path;

        static constexpr std::string_view DEFAULT_NAMESPACE = "engine";
    };


    [[nodiscard]]
    std::string GetFileExtension(const std::string& filePath);
}

template <>
struct std::hash<enigma::resource::ResourceLocation>
{
    size_t operator()(const enigma::resource::ResourceLocation& loc) const noexcept
    {
        return hash<std::string>()(loc.ToString());
    }
};
