//-----------------------------------------------------------------------------------------------
// BundleTextureLoader.cpp
//
// Implementation of texture loading from ShaderBundle directories.
// Uses RendererSubsystem::CreateTexture2D for GPU resource creation.
//-----------------------------------------------------------------------------------------------

#include "BundleTextureLoader.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Graphic/Bundle/BundleException.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"

namespace enigma::graphic
{
    //-----------------------------------------------------------------------------------------------
    LoadedTexture BundleTextureLoader::LoadTexture(
        const TextureDeclaration&    decl,
        const std::filesystem::path& bundleShadersPath)
    {
        // Resolve absolute path from relative declaration path
        std::filesystem::path absolutePath = bundleShadersPath / decl.path;

        if (!std::filesystem::exists(absolutePath))
        {
            throw TextureLoadException(
                "Custom texture file not found: " + absolutePath.string());
        }

        // Parse co-located .enigmeta file (e.g. cloud-water.png.enigmeta)
        std::filesystem::path enigmetaPath = absolutePath;
        enigmetaPath                       += ".enigmeta";

        TextureMetadata metadata;
        try
        {
            metadata = EnigmetaParser::Parse(enigmetaPath);
        }
        catch (const EnigmetaParseException& e)
        {
            // Invalid .enigmeta JSON - use defaults, log warning
            ERROR_RECOVERABLE(e.what());
            metadata = EnigmetaParser::Parse(std::filesystem::path{}); // defaults
        }

        // Create GPU texture via RendererSubsystem
        std::string debugName = "BundleTex:" + decl.path;
        auto        texture   = g_theRendererSubsystem->CreateTexture2D(
            absolutePath.string(), TextureUsage::ShaderResource, debugName);

        if (!texture)
        {
            throw TextureLoadException(
                "Failed to create GPU texture from: " + absolutePath.string());
        }

        return LoadedTexture{std::move(texture), metadata};
    }

    //-----------------------------------------------------------------------------------------------
    std::unordered_map<std::string, LoadedTexture> BundleTextureLoader::LoadAllTextures(
        const CustomTextureData&     textureData,
        const std::filesystem::path& bundleShadersPath)
    {
        std::unordered_map<std::string, LoadedTexture> result;

        // Collect all unique texture paths from both stage and custom bindings
        auto loadIfNew = [&](const TextureDeclaration& decl)
        {
            if (result.find(decl.path) != result.end())
            {
                return; // Already loaded (dedup)
            }

            try
            {
                result[decl.path] = LoadTexture(decl, bundleShadersPath);
            }
            catch (const TextureLoadException& e)
            {
                ERROR_RECOVERABLE(e.what());
                // Skip this texture, continue loading others
            }
        };

        for (const auto& binding : textureData.stageBindings)
        {
            loadIfNew(binding.texture);
        }

        for (const auto& binding : textureData.customBindings)
        {
            loadIfNew(binding.texture);
        }

        return result;
    }
} // namespace enigma::graphic
