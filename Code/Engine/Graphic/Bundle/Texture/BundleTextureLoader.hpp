#pragma once
//-----------------------------------------------------------------------------------------------
// BundleTextureLoader.hpp
//
// Pure static helper for loading textures from a ShaderBundle directory.
// Resolves paths, creates D12Texture via RendererSubsystem, parses .enigmeta metadata.
// Supports path-based deduplication for batch loading.
//-----------------------------------------------------------------------------------------------

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include "Engine/Graphic/Bundle/Texture/CustomTextureData.hpp"
#include "Engine/Graphic/Bundle/Texture/EnigmetaParser.hpp"

namespace enigma::graphic
{
    class D12Texture;

    // Result of loading a single texture: GPU resource + metadata
    struct LoadedTexture
    {
        std::shared_ptr<D12Texture> texture;
        TextureMetadata             metadata;
    };

    // Static helper for loading textures from ShaderBundle directories
    class BundleTextureLoader
    {
    public:
        BundleTextureLoader() = delete; // Pure static, no instantiation

        // Load a single texture from a TextureDeclaration.
        // Resolves absolute path relative to bundleShadersPath.
        // Parses co-located .enigmeta file for sampler metadata.
        // Throws TextureLoadException if file does not exist.
        static LoadedTexture LoadTexture(
            const TextureDeclaration&    decl,
            const std::filesystem::path& bundleShadersPath);

        // Batch load all textures declared in CustomTextureData.
        // Returns path -> LoadedTexture map with deduplication (same path loaded once).
        // Individual load failures are caught and logged (ERROR_RECOVERABLE), skipped.
        static std::unordered_map<std::string, LoadedTexture> LoadAllTextures(
            const CustomTextureData&     textureData,
            const std::filesystem::path& bundleShadersPath);
    };
} // namespace enigma::graphic
