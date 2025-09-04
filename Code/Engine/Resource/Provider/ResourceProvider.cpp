#include "ResourceProvider.hpp"

#include <fstream>

namespace enigma::resource
{
    FileSystemResourceProvider::FileSystemResourceProvider(const std::filesystem::path& basePath, const std::string& name) : m_basePath(basePath), m_name(name)
    {
        if (!std::filesystem::exists(m_basePath))
            throw std::runtime_error("Base path does not exist: " + m_basePath.string());

        // Default search extension
        m_searchExtensions = {
            ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds", // Texture
            ".obj", ".fbx", ".gltf", ".glb", ".dae", // Model
            ".json", ".txt", ".cfg", ".xml", ".yaml", // Configuration
            ".wav", ".ogg", ".mp3", // Audio
            ".vert", ".frag", ".glsl", ".hlsl" // Shader
        };
    }

    bool FileSystemResourceProvider::HasResource(const ResourceLocation& location) const
    {
        auto path = locationToPath(location);
        if (path.has_value())
        {
            return true;
        }

        // If exact match failed, try to find the actual file path with extension
        std::filesystem::path namespacePath;
        auto                  it = m_namespaceMappings.find(location.GetNamespace());
        if (it != m_namespaceMappings.end())
        {
            namespacePath = it->second;
        }
        else
        {
            namespacePath = m_basePath / location.GetNamespace();
        }

        std::filesystem::path resourcePath(location.GetPath());
        auto                  basePath = namespacePath / resourcePath;

        return findResourceFile(basePath).has_value();
    }

    std::optional<ResourceMetadata> FileSystemResourceProvider::GetMetadata(const ResourceLocation& location) const
    {
        auto pathOpt = locationToPath(location);
        if (pathOpt)
        {
            return ResourceMetadata(location, *pathOpt);
        }

        // If exact match failed, try to find the actual file path with extension
        std::filesystem::path namespacePath;
        auto                  it = m_namespaceMappings.find(location.GetNamespace());
        if (it != m_namespaceMappings.end())
        {
            namespacePath = it->second;
        }
        else
        {
            namespacePath = m_basePath / location.GetNamespace();
        }

        std::filesystem::path resourcePath(location.GetPath());
        auto                  basePath = namespacePath / resourcePath;

        auto foundPath = findResourceFile(basePath);
        if (foundPath)
        {
            return ResourceMetadata(location, *foundPath);
        }

        return std::nullopt;
    }

    std::vector<uint8_t> FileSystemResourceProvider::ReadResource(const ResourceLocation& location)
    {
        auto pathOpt = locationToPath(location);
        if (!pathOpt)
        {
            // If exact match failed, try to find the actual file path with extension
            std::filesystem::path namespacePath;
            auto                  it = m_namespaceMappings.find(location.GetNamespace());
            if (it != m_namespaceMappings.end())
            {
                namespacePath = it->second;
            }
            else
            {
                namespacePath = m_basePath / location.GetNamespace();
            }

            std::filesystem::path resourcePath(location.GetPath());
            auto                  basePath = namespacePath / resourcePath;

            pathOpt = findResourceFile(basePath);
        }

        if (!pathOpt)
        {
            throw std::runtime_error("Resource not found: " + location.ToString());
        }

        std::ifstream file(*pathOpt, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open resource: " + location.ToString());
        }

        auto fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> data(fileSize);
        file.read(reinterpret_cast<char*>(data.data()), fileSize);

        return data;
    }

    std::vector<ResourceLocation> FileSystemResourceProvider::ListResources(const std::string& namespace_id, ResourceType type) const
    {
        std::vector<ResourceLocation> results;

        if (namespace_id.empty())
        {
            // Scan all configured namespaces
            if (!m_namespaceMappings.empty())
            {
                // Use configured namespace mappings
                for (const auto& [ns, path] : m_namespaceMappings)
                {
                    if (std::filesystem::exists(path) && std::filesystem::is_directory(path))
                    {
                        scanDirectory(path, ns, results, type);
                    }
                }
            }
            else
            {
                // Fallback: scan base directory using directory names as namespaces
                for (const auto& entry : std::filesystem::directory_iterator(m_basePath))
                {
                    if (entry.is_directory())
                    {
                        scanDirectory(entry.path(), entry.path().filename().string(), results, type);
                    }
                }
            }
        }
        else
        {
            // Scan a specific namespace
            auto namespacePath = m_basePath / namespace_id;
            if (std::filesystem::exists(namespacePath))
            {
                scanDirectory(namespacePath, namespace_id, results, type);
            }
        }

        return results;
    }

    void FileSystemResourceProvider::SetNamespaceMapping(const std::string& namespace_id, const std::filesystem::path& path)
    {
        m_namespaceMappings[namespace_id] = path;
    }

    void FileSystemResourceProvider::SetSearchExtensions(const std::vector<std::string>& extensions)
    {
        m_searchExtensions = extensions;
    }

    std::optional<std::filesystem::path> FileSystemResourceProvider::locationToPath(const ResourceLocation& location) const
    {
        std::filesystem::path namespacePath;

        // Check if there are custom mappings
        auto it = m_namespaceMappings.find(location.GetNamespace());
        if (it != m_namespaceMappings.end())
        {
            namespacePath = it->second;
        }
        else
        {
            namespacePath = m_basePath / location.GetNamespace();
        }

        // Automatically process path separators using filesystem
        std::filesystem::path resourcePath(location.GetPath());
        auto                  basePath = namespacePath / resourcePath;

        // Check if the path already has an extension
        if (basePath.has_extension())
        {
            // If the path already has an extension, check if file exists directly
            if (std::filesystem::exists(basePath) && std::filesystem::is_regular_file(basePath))
            {
                return basePath;
            }
            // If file with specified extension doesn't exist, return nullopt
            return std::nullopt;
        }
        else
        {
            // If no extension, try different extensions
            return findResourceFile(basePath);
        }
    }

    std::optional<std::filesystem::path> FileSystemResourceProvider::findResourceFile(const std::filesystem::path& basePath) const
    {
        // First, check if a file with an extension already exists
        if (std::filesystem::exists(basePath) && std::filesystem::is_regular_file(basePath))
        {
            return basePath;
        }

        // Try all configured extensions
        for (const auto& ext : m_searchExtensions)
        {
            auto pathWithExt = basePath.string() + ext;
            if (std::filesystem::exists(pathWithExt) && std::filesystem::is_regular_file(pathWithExt))
            {
                return pathWithExt;
            }
        }

        return std::nullopt;
    }

    void FileSystemResourceProvider::scanDirectory(const std::filesystem::path& dir, const std::string& namespace_id, std::vector<ResourceLocation>& results, ResourceType filterType) const
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir))
        {
            if (!entry.is_regular_file()) continue;

            // Get the relative path
            auto relativePath = std::filesystem::relative(entry.path(), dir);

            // Path converted to ResourceLocation format (remove extension following Minecraft Neoforge API design)
            std::string locationPath = relativePath.generic_string();

            // Remove file extension - ResourceLocation should not contain extensions
            // Extensions are stored in ResourceMetadata instead
            auto extensionPos = locationPath.find_last_of('.');
            if (extensionPos != std::string::npos)
            {
                locationPath = locationPath.substr(0, extensionPos);
            }

            // If there is type filtering, check the file type
            if (filterType != ResourceType::UNKNOWN)
            {
                auto detectedType = ResourceMetadata::DetectType(entry.path());
                if (detectedType != filterType) continue;
            }

            // Create ResourceLocation
            try
            {
                ResourceLocation loc(namespace_id, locationPath);


                // Avoid duplicate additions (the same resource may have multiple extensions)
                if (std::find(results.begin(), results.end(), loc) == results.end())
                {
                    results.push_back(loc);
                }
            }
            catch (const std::exception&)
            {
                // Ignore invalid resource locations
            }
        }
    }

    std::optional<ResourceLocation> FileSystemResourceProvider::findResourceByPath(const ResourceLocation& location) const
    {
        std::filesystem::path namespacePath;

        // Check if there are custom mappings
        auto it = m_namespaceMappings.find(location.GetNamespace());
        if (it != m_namespaceMappings.end())
        {
            namespacePath = it->second;
        }
        else
        {
            namespacePath = m_basePath / location.GetNamespace();
        }

        std::filesystem::path resourcePath(location.GetPath());
        auto                  basePath = namespacePath / resourcePath;

        // If the requested location already has an extension, try exact match first
        if (basePath.has_extension())
        {
            if (std::filesystem::exists(basePath) && std::filesystem::is_regular_file(basePath))
            {
                return location;
            }
        }

        // Try adding each possible extension
        for (const auto& ext : m_searchExtensions)
        {
            auto pathWithExt = basePath.string() + ext;
            if (std::filesystem::exists(pathWithExt) && std::filesystem::is_regular_file(pathWithExt))
            {
                // Return the original ResourceLocation without extension
                // The actual file path with extension is handled in metadata
                return location;
            }
        }

        return std::nullopt;
    }
}
