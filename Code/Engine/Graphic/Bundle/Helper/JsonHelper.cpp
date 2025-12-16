//-----------------------------------------------------------------------------------------------
// JsonHelper.cpp
//
// [NEW] Implementation of JSON parsing utilities for ShaderBundle module
//
// Implementation Notes:
//   - Uses JsonObject wrapper from Core/Json.hpp for safe JSON access
//   - Returns std::nullopt on any error (file missing, parse failure, validation failure)
//   - No exceptions thrown from this helper (clean error handling pattern)
//
//-----------------------------------------------------------------------------------------------

#include "JsonHelper.hpp"

#include <fstream>

#include "Engine/Graphic/Bundle/ShaderBundleCommon.hpp"
#include "Engine/Core/Json.hpp"
#include "Engine/Core/FileSystemHelper.hpp"
#include "Engine/Core/LogCategory/LogCategory.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

namespace enigma::graphic
{
    //-------------------------------------------------------------------------------------------
    // ParseBundleJson
    //-------------------------------------------------------------------------------------------
    std::optional<ShaderBundleMeta> JsonHelper::ParseBundleJson(
        const std::filesystem::path& jsonPath)
    {
        // [1] Check file existence first
        if (!FileSystemHelper::FileExists(jsonPath))
        {
            return std::nullopt;
        }

        // [2] Open file and parse JSON directly using nlohmann::json
        std::ifstream file(jsonPath);
        if (!file.is_open())
        {
            LogWarn(LogShaderBundle, "JsonHelper:: Failed to open bundle.json: %s",
                    jsonPath.string().c_str());
            return std::nullopt;
        }

        // [3] Parse JSON directly from file stream (more efficient than string)
        core::Json rawJson;
        try
        {
            rawJson = core::Json::parse(file);
        }
        catch (const core::Json::parse_error& e)
        {
            LogWarn(LogShaderBundle, "JsonHelper:: Failed to parse bundle.json: %s - %s",
                    jsonPath.string().c_str(), e.what());
            return std::nullopt;
        }

        core::JsonObject json(rawJson);

        // [4] Validate required 'name' field
        if (!json.Has("name"))
        {
            LogWarn(LogShaderBundle, "JsonHelper:: bundle.json missing required 'name' field: %s",
                    jsonPath.string().c_str());
            return std::nullopt;
        }

        // [5] Extract fields and build ShaderBundleMeta
        ShaderBundleMeta meta;
        meta.name        = json.GetString("name");
        meta.author      = json.GetString("author", "Unknown");
        meta.description = json.GetString("shaderDescription", "");

        // Path: bundle.json is in .../shaders/bundle.json
        // We want the parent of 'shaders' directory (the bundle root)
        // jsonPath.parent_path() = .../shaders/
        // jsonPath.parent_path().parent_path() = .../
        meta.path = jsonPath.parent_path().parent_path();

        // isEngineBundle is set externally by the caller, not from JSON
        meta.isEngineBundle = false;

        return meta;
    }

    //-------------------------------------------------------------------------------------------
    // ParseFallbackRuleJson
    //-------------------------------------------------------------------------------------------
    std::optional<FallbackRule> JsonHelper::ParseFallbackRuleJson(
        const std::filesystem::path& jsonPath)
    {
        // [1] Check file existence - fallback rules are optional, so missing file is OK
        if (!FileSystemHelper::FileExists(jsonPath))
        {
            // Note: Not logging here since missing fallback_rule.json is acceptable
            return std::nullopt;
        }

        // [2] Open file and parse JSON directly using nlohmann::json
        std::ifstream file(jsonPath);
        if (!file.is_open())
        {
            LogWarn(LogShaderBundle, "JsonHelper:: Failed to open fallback_rule.json: %s",
                    jsonPath.string().c_str());
            return std::nullopt;
        }

        // [3] Parse JSON directly from file stream
        core::Json rawJson;
        try
        {
            rawJson = core::Json::parse(file);
        }
        catch (const core::Json::parse_error& e)
        {
            LogWarn(LogShaderBundle, "JsonHelper:: Failed to parse fallback_rule.json: %s - %s",
                    jsonPath.string().c_str(), e.what());
            return std::nullopt;
        }

        core::JsonObject json(rawJson);

        // [4] Validate required 'default' field
        if (!json.Has("default"))
        {
            LogWarn(LogShaderBundle, "JsonHelper:: fallback_rule.json missing 'default' field: %s",
                    jsonPath.string().c_str());
            return std::nullopt;
        }

        // [5] Extract 'default' field
        FallbackRule rule;
        rule.defaultProgram = json.GetString("default");

        // [6] Extract 'fallbacks' map if present
        if (json.Has("fallbacks"))
        {
            // Get the underlying nlohmann::json for direct map iteration
            // JsonObject wrapper doesn't expose map iteration directly

            if (rawJson.contains("fallbacks") && rawJson["fallbacks"].is_object())
            {
                const core::Json& fallbacksJson = rawJson["fallbacks"];

                for (auto it = fallbacksJson.begin(); it != fallbacksJson.end(); ++it)
                {
                    const std::string& programName   = it.key();
                    const core::Json&  fallbackArray = it.value();

                    if (fallbackArray.is_array())
                    {
                        std::vector<std::string> fallbackChain;
                        for (const auto& item : fallbackArray)
                        {
                            if (item.is_string())
                            {
                                fallbackChain.push_back(item.get<std::string>());
                            }
                        }
                        rule.fallbacks[programName] = std::move(fallbackChain);
                    }
                }
            }
        }

        return rule;
    }
} // namespace enigma::graphic
