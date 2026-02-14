//-----------------------------------------------------------------------------------------------
// EnigmetaParser.cpp
//
// Implementation of .enigmeta JSON metadata parsing.
// Uses Core/Json.hpp (nlohmann/json wrapper) for JSON access.
//-----------------------------------------------------------------------------------------------

#include "EnigmetaParser.hpp"

#include <fstream>

#include "Engine/Core/Json.hpp"
#include "Engine/Graphic/Bundle/BundleException.hpp"

namespace enigma::graphic
{
    //-----------------------------------------------------------------------------------------------
    TextureMetadata EnigmetaParser::Parse(const std::filesystem::path& enigmetaPath)
    {
        TextureMetadata meta;

        // Missing file is not an error - return defaults
        if (!std::filesystem::exists(enigmetaPath))
        {
            meta.samplerConfig = BuildSamplerConfig(meta.blur, meta.clamp);
            return meta;
        }

        // Read file content
        std::ifstream file(enigmetaPath);
        if (!file.is_open())
        {
            meta.samplerConfig = BuildSamplerConfig(meta.blur, meta.clamp);
            return meta;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        file.close();

        // Parse JSON - throw on invalid syntax
        auto jsonOpt = enigma::core::JsonObject::TryParse(content);
        if (!jsonOpt.has_value())
        {
            throw EnigmetaParseException(
                "Invalid JSON in .enigmeta file: " + enigmetaPath.string());
        }

        auto& root = jsonOpt.value();

        // Extract "texture" object for blur/clamp
        if (root.Has("texture"))
        {
            auto texObj = root.GetJsonObject("texture");
            meta.blur   = texObj.GetBool("blur", false);
            meta.clamp  = texObj.GetBool("clamp", false);
        }

        // Extract samplerSlot
        meta.samplerSlot = root.GetInt("samplerSlot", 0);

        // Pre-build SamplerConfig from parsed values
        meta.samplerConfig = BuildSamplerConfig(meta.blur, meta.clamp);

        return meta;
    }

    //-----------------------------------------------------------------------------------------------
    SamplerConfig EnigmetaParser::BuildSamplerConfig(bool blur, bool clamp)
    {
        if (blur && !clamp) return SamplerConfig::LinearWrap();
        if (blur && clamp) return SamplerConfig::Linear(); // Linear default = Clamp
        if (!blur && clamp) return SamplerConfig::Point(); // Point default = Clamp
        return SamplerConfig::PointWrap(); // !blur && !clamp
    }
} // namespace enigma::graphic
