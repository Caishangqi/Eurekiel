#pragma once
//-----------------------------------------------------------------------------------------------
// EnigmetaParser.hpp
//
// Pure static helper for parsing .enigmeta JSON metadata files.
// Extracts texture filter/address configuration and pre-builds SamplerConfig at load time.
//
// .enigmeta JSON format:
//   {
//     "texture": { "blur": true, "clamp": false },
//     "samplerSlot": 1
//   }
//
// All fields optional. Defaults: blur=false, clamp=false, samplerSlot=0.
//-----------------------------------------------------------------------------------------------

#include <filesystem>

#include "Engine/Graphic/Sampler/SamplerConfig.hpp"

namespace enigma::graphic
{
    // Parsed result from .enigmeta file with pre-built SamplerConfig
    struct TextureMetadata
    {
        bool blur  = false; // true = bilinear (Linear), false = nearest (Point)
        bool clamp = false; // true = Clamp address mode, false = Wrap address mode

        SamplerConfig samplerConfig; // Pre-built at parse time from blur/clamp

        int samplerSlot = 0; // Target slot index for SamplerProvider (0-15)
    };

    // Static helper class for .enigmeta JSON parsing
    class EnigmetaParser
    {
    public:
        EnigmetaParser() = delete; // Pure static, no instantiation

        // Parse .enigmeta file and return TextureMetadata with pre-built SamplerConfig.
        // Returns default metadata if file does not exist.
        // Throws EnigmetaParseException if file exists but contains invalid JSON.
        static TextureMetadata Parse(const std::filesystem::path& enigmetaPath);

    private:
        // Build SamplerConfig from blur/clamp flags.
        // blur=false, clamp=false -> PointWrap
        // blur=false, clamp=true  -> Point (Clamp)
        // blur=true,  clamp=false -> LinearWrap
        // blur=true,  clamp=true  -> Linear (Clamp default)
        static SamplerConfig BuildSamplerConfig(bool blur, bool clamp);
    };
} // namespace enigma::graphic
