#include "ResourceMetadata.hpp"

namespace enigma::resource
{
    ResourceMetadata::ResourceMetadata(const ResourceLocation& loc, const std::filesystem::path& path) : location(loc), filePath(path)
    {
        if (std::filesystem::exists(filePath))
        {
            fileSize     = std::filesystem::file_size(filePath);
            lastModified = std::filesystem::last_write_time(filePath);
            type         = DetectType(filePath);
        }
    }

    ResourceType ResourceMetadata::DetectType(const std::filesystem::path& path)
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });    // Use lambda to avoid type conversion warnings

        if (ext == ".png" || ext == ".jpg")
            return ResourceType::TEXTURE;

        if (ext == ".json" || ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb" || ext == ".dae")
            return ResourceType::MODEL;

        if (ext == ".vert" || ext == ".frag" || ext == ".glsl" || ext == ".hlsl" || ext == ".shader")
            return ResourceType::SHADER;


        if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac" || ext == ".m4a")
            return ResourceType::AUDIO;

        if (ext == ".ttf" || ext == ".otf" || ext == ".fnt")
            return ResourceType::FONT;

        if (ext == ".txt" || ext == ".cfg" || ext == ".ini" || ext == ".xml" || ext == ".yaml" || ext == ".yml")
            return ResourceType::TEXT;

        if (ext == ".bin" || ext == ".dat")
            return ResourceType::BINARY;

        return ResourceType::UNKNOWN;
    }

    std::string ResourceMetadata::GetFileExtension() const
    {
        return filePath.extension().string();
    }
}
