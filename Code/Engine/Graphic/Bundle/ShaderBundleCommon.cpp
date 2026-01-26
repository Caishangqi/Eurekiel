//-----------------------------------------------------------------------------------------------
// ShaderBundleCommon.cpp
//
// [NEW] Implementation file for ShaderBundle common types and declarations
//
// This file contains:
//   - Log category definition for LogShaderBundle
//   - ShaderBundleMeta::FromBundlePath() factory method implementation
//
// Teaching Points:
//   - DEFINE_LOG_CATEGORY must be in exactly one .cpp file
//   - Static factory methods provide clean object construction with validation
//   - std::optional for nullable return types (C++17)
//
//-----------------------------------------------------------------------------------------------

#include "ShaderBundleCommon.hpp"

#include "Engine/Graphic/Bundle/Helper/JsonHelper.hpp"

namespace enigma::graphic
{
    //-------------------------------------------------------------------------------------------
    // Log Category Definition
    //
    // [NEW] Define the LogShaderBundle log category
    // This creates the actual storage for the category declared in the header
    // Default log level is TRACE (most verbose)
    //-------------------------------------------------------------------------------------------
    DEFINE_LOG_CATEGORY(LogShaderBundle);

    //-------------------------------------------------------------------------------------------
    // ShaderBundleMeta::FromBundlePath
    //
    // [NEW] Static factory method implementation
    //
    // Workflow:
    //   1. Construct bundle.json path: {bundlePath}/shaders/bundle.json
    //   2. Call JsonHelper::ParseBundleJson to parse metadata
    //   3. Override path and isEngineBundle fields (JsonHelper sets path from json location)
    //   4. Return the configured meta or nullopt on failure
    //-------------------------------------------------------------------------------------------
    std::optional<ShaderBundleMeta> ShaderBundleMeta::FromBundlePath(
        const std::filesystem::path& bundlePath,
        bool                         isEngineBundle)
    {
        // Construct path to bundle.json
        std::filesystem::path bundleJsonPath = bundlePath / "shaders" / "bundle.json";

        // Parse bundle.json using existing JsonHelper
        auto metaOpt = JsonHelper::ParseBundleJson(bundleJsonPath);

        if (!metaOpt)
        {
            return std::nullopt;
        }

        // Override fields that need to be set by caller
        ShaderBundleMeta meta = metaOpt.value();
        meta.path             = bundlePath; // Use the provided bundle path directly
        meta.isEngineBundle   = isEngineBundle; // Set engine bundle flag

        return meta;
    }
} // namespace enigma::graphic
