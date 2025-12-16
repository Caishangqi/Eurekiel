#pragma once
//-----------------------------------------------------------------------------------------------
// JsonHelper.hpp
//
// [NEW] Pure static utility class for parsing ShaderBundle JSON configuration files
//
// This helper provides:
//   - ParseBundleJson: Parse bundle.json to ShaderBundleMeta
//   - ParseFallbackRuleJson: Parse fallback_rule.json to FallbackRule
//
// Design Principles (SOLID + KISS):
//   - Single Responsibility: Only handles JSON parsing for ShaderBundle module
//   - Stateless: All methods are static, no instance creation allowed
//   - Error Handling: Uses std::optional for graceful error handling (no exceptions)
//   - Dependency: Relies on Core/Json.hpp (JsonObject wrapper)
//
// Usage:
//   auto meta = JsonHelper::ParseBundleJson(path / "shaders" / "bundle.json");
//   if (meta) {
//       LogInfo(LogShaderBundle, "Loaded bundle: %s", meta->name.c_str());
//   }
//
// Teaching Points:
//   - std::optional for nullable return types (C++17)
//   - Pure static class pattern (delete default constructor)
//   - Separation of parsing logic from business logic
//
//-----------------------------------------------------------------------------------------------

#include <optional>
#include <string>
#include <filesystem>

// Forward declarations to minimize header dependencies
namespace enigma::graphic
{
    struct ShaderBundleMeta;
    struct FallbackRule;
}

namespace enigma::graphic
{
    //-------------------------------------------------------------------------------------------
    // JsonHelper
    //
    // [NEW] Static utility class for ShaderBundle JSON parsing
    // All methods are static, class cannot be instantiated
    //-------------------------------------------------------------------------------------------
    class JsonHelper
    {
    public:
        // Prevent instantiation - pure static utility class
        JsonHelper()                             = delete;
        ~JsonHelper()                            = delete;
        JsonHelper(const JsonHelper&)            = delete;
        JsonHelper& operator=(const JsonHelper&) = delete;

        //-------------------------------------------------------------------------------------------
        // ParseBundleJson
        //
        // [NEW] Parse bundle.json file to ShaderBundleMeta structure
        //
        // Expected JSON format:
        // {
        //     "name": "Shader Bundle Name",        // Required
        //     "author": "Author Name",             // Optional, defaults to "Unknown"
        //     "shaderDescription": "Description"   // Optional, defaults to ""
        // }
        //
        // Parameters:
        //   jsonPath - Full path to bundle.json file
        //
        // Returns:
        //   std::optional<ShaderBundleMeta> - Parsed metadata, or std::nullopt if:
        //     - File doesn't exist
        //     - JSON parsing fails
        //     - Required 'name' field is missing
        //
        // Teaching Point:
        //   Returns nullopt instead of throwing exceptions for cleaner error handling
        //-------------------------------------------------------------------------------------------
        static std::optional<ShaderBundleMeta> ParseBundleJson(
            const std::filesystem::path& jsonPath);

        //-------------------------------------------------------------------------------------------
        // ParseFallbackRuleJson
        //
        // [NEW] Parse fallback_rule.json file to FallbackRule structure
        //
        // Expected JSON format:
        // {
        //     "default": "gbuffers_textured",      // Required - default fallback program
        //     "fallbacks": {                       // Required - fallback chains
        //         "gbuffers_clouds": ["gbuffers_textured", "gbuffers_basic"],
        //         "gbuffers_water":  ["gbuffers_terrain", "gbuffers_textured"]
        //     }
        // }
        //
        // Parameters:
        //   jsonPath - Full path to fallback_rule.json file
        //
        // Returns:
        //   std::optional<FallbackRule> - Parsed rules, or std::nullopt if:
        //     - File doesn't exist (fallback rules are optional)
        //     - JSON parsing fails
        //     - Required fields are missing
        //
        // Note: Returning nullopt for missing file is acceptable since fallback rules
        //       are optional for ShaderBundles
        //-------------------------------------------------------------------------------------------
        static std::optional<FallbackRule> ParseFallbackRuleJson(
            const std::filesystem::path& jsonPath);
    };
} // namespace enigma::graphic
