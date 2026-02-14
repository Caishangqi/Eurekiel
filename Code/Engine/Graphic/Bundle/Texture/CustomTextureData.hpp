#pragma once
//-----------------------------------------------------------------------------------------------
// CustomTextureData.hpp
//
// Pure data structures for custom texture declarations parsed from shaders.properties.
// Stores texture.<stage>.<textureSlot> and customTexture.<name> directive results.
//
// No external dependencies beyond STL. Header-only.
//-----------------------------------------------------------------------------------------------

#include <string>
#include <vector>

namespace enigma::graphic
{
    // Single texture declaration parsed from a properties directive value
    struct TextureDeclaration
    {
        std::string path; // Relative path from shaders/ directory
        std::string absolutePath; // Resolved absolute filesystem path

        bool isRaw = false; // Whether this is a raw texture definition
        // Raw texture parameters (only valid when isRaw == true)
        std::string rawType;
        std::string rawInternalFormat;
        int         rawSizeX = 0;
        std::string rawPixelFormat;
        std::string rawPixelType;
    };

    // Stage-scoped texture binding: texture.<stage>.<textureSlot>=<path>
    struct StageTextureBinding
    {
        std::string        stage; // Render stage name (e.g. "composite", "deferred")
        int                textureSlot = -1; // customImage slot index (0-15)
        TextureDeclaration texture;
    };

    // Named custom texture binding: customTexture.<name>=<path>
    struct CustomTextureBinding
    {
        std::string        name; // Custom sampler name
        int                textureSlot = -1; // customImage slot index (0-15)
        TextureDeclaration texture;
    };

    // Aggregate of all texture declarations from shaders.properties
    struct CustomTextureData
    {
        std::vector<StageTextureBinding>  stageBindings;
        std::vector<CustomTextureBinding> customBindings;

        bool HasStageBindings() const { return !stageBindings.empty(); }
        bool HasCustomBindings() const { return !customBindings.empty(); }
        bool IsEmpty() const { return stageBindings.empty() && customBindings.empty(); }

        // Return pointers to all stage bindings matching the given stage name
        std::vector<const StageTextureBinding*> GetBindingsForStage(const std::string& stageName) const
        {
            std::vector<const StageTextureBinding*> result;
            for (const auto& binding : stageBindings)
            {
                if (binding.stage == stageName)
                {
                    result.push_back(&binding);
                }
            }
            return result;
        }
    };
} // namespace enigma::graphic
