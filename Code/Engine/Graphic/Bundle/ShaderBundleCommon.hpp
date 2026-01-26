#pragma once
//-----------------------------------------------------------------------------------------------
// ShaderBundleCommon.hpp
//
// [NEW] Common types and declarations for ShaderBundle module
//
// This header provides:
//   - Log category declaration for ShaderBundle module (LogShaderBundle)
//   - Metadata structures (ShaderBundleMeta, ShaderBundleResult, FallbackRule)
//   - Event name constants for lifecycle notifications
//
// Usage:
//   #include "ShaderBundleCommon.hpp"
//   LogInfo(LogShaderBundle, "Loading bundle: %s", name.c_str());
//
// Teaching Points:
//   - Log categories enable filtering and organization of log messages
//   - Forward declarations reduce compilation dependencies
//   - Event constants provide type-safe event naming
//   - std::filesystem::path for cross-platform path handling
//
//-----------------------------------------------------------------------------------------------

#include <string>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>
#include <unordered_map>

#include "Engine/Core/LogCategory/LogCategory.hpp"


namespace enigma::graphic
{
    // Forward declarations to avoid circular dependencies
    class ShaderBundle;

    //-------------------------------------------------------------------------------------------
    // Log Category Declaration
    //
    // [NEW] ShaderBundle module log category for consistent logging
    // Use with LogInfo, LogWarn, LogError macros:
    //   LogInfo(LogShaderBundle, "Message with format: %s", arg);
    //
    // Definition in ShaderBundleCommon.cpp:
    //   DEFINE_LOG_CATEGORY(LogShaderBundle);
    //-------------------------------------------------------------------------------------------
    DECLARE_LOG_CATEGORY_EXTERN(LogShaderBundle);

    //-------------------------------------------------------------------------------------------
    // ShaderBundleMeta
    //
    // [NEW] Metadata for a discovered ShaderBundle
    // Populated from bundle.json parsing or directory discovery
    //
    // Factory Method:
    //   FromBundlePath() - Create meta from bundle directory path by parsing bundle.json
    //
    // Usage:
    //   auto meta = ShaderBundleMeta::FromBundlePath(bundlePath, true);  // Engine bundle
    //   auto meta = ShaderBundleMeta::FromBundlePath(bundlePath, false); // User bundle
    //-------------------------------------------------------------------------------------------
    struct ShaderBundleMeta
    {
        std::string           name; // ShaderBundle display name
        std::string           author; // Author name (optional)
        std::string           description; // Description text (optional)
        std::filesystem::path path; // Full path to bundle directory
        bool                  isEngineBundle = false; // True if this is the engine default bundle

        //-------------------------------------------------------------------------------------------
        // FromBundlePath
        //
        // [NEW] Static factory method to create ShaderBundleMeta from bundle directory path
        //
        // Parameters:
        //   bundlePath     - Path to bundle root directory (e.g., ".enigma/assets/engine")
        //   isEngineBundle - Whether this is the engine default bundle
        //
        // Returns:
        //   std::optional<ShaderBundleMeta> - Parsed metadata, or std::nullopt if:
        //     - bundle.json doesn't exist at {bundlePath}/shaders/bundle.json
        //     - JSON parsing fails
        //     - Required 'name' field is missing
        //
        // Note: This method internally calls JsonHelper::ParseBundleJson and sets
        //       the path and isEngineBundle fields appropriately.
        //-------------------------------------------------------------------------------------------
        static std::optional<ShaderBundleMeta> FromBundlePath(const std::filesystem::path& bundlePath, bool isEngineBundle = false);
    };

    //-------------------------------------------------------------------------------------------
    // ShaderBundleResult
    //
    // [NEW] Result type for ShaderBundle operations
    // Used as return type for LoadShaderBundle, UnloadShaderBundle, etc.
    //-------------------------------------------------------------------------------------------
    struct ShaderBundleResult
    {
        bool                          success = false; // Operation success status
        std::string                   errorMessage; // Error message if success == false
        std::shared_ptr<ShaderBundle> bundle; // Loaded bundle (nullptr if failed)
    };

    //-------------------------------------------------------------------------------------------
    // FallbackRule
    //
    // [NEW] Fallback configuration loaded from fallback_rule.json
    // Defines shader program fallback chains for graceful degradation
    //
    // Example fallback_rule.json:
    // {
    //   "default": "gbuffers_basic",
    //   "fallbacks": {
    //     "gbuffers_clouds": ["gbuffers_textured", "gbuffers_basic"],
    //     "gbuffers_water":  ["gbuffers_textured", "gbuffers_basic"]
    //   }
    // }
    //-------------------------------------------------------------------------------------------
    struct FallbackRule
    {
        std::string                                               defaultProgram; // Default fallback program
        std::unordered_map<std::string, std::vector<std::string>> fallbacks; // Program -> fallback chain
    };

    //-------------------------------------------------------------------------------------------
    // Event Name Constants
    //
    // [NEW] Standard event names for ShaderBundle lifecycle events
    // Subscribe to these events via EventSystem for notifications
    //
    // Usage:
    //   eventSystem->Subscribe(EVENT_SHADER_BUNDLE_LOADED, [](const EventData& data) {
    //       auto* bundleData = static_cast<const ShaderBundleEventData*>(&data);
    //       // Handle bundle loaded
    //   });
    //-------------------------------------------------------------------------------------------
    constexpr const char* EVENT_SHADER_BUNDLE_LOADED              = "OnShaderBundleLoaded";
    constexpr const char* EVENT_SHADER_BUNDLE_UNLOADED            = "OnShaderBundleUnloaded";
    constexpr const char* EVENT_SHADER_BUNDLE_PROPERTIES_MODIFIED = "OnShaderBundlePropertiesModified";
    constexpr const char* EVENT_SHADER_BUNDLE_PROPERTIES_RESET    = "OnShaderBundlePropertiesReset";
    constexpr const char* EVENT_SHADER_BUNDLE_RELOAD              = "OnShaderBundleReload";

    //-------------------------------------------------------------------------------------------
    // ShaderBundleEventData
    //
    // [NEW] Event data structure for ShaderBundle lifecycle events
    // Passed to event subscribers when bundle events are triggered
    //-------------------------------------------------------------------------------------------
    struct ShaderBundleEventData
    {
        std::shared_ptr<ShaderBundle> bundle; // The bundle involved in the event
        std::string                   bundleName; // Name of the bundle
        std::string                   eventType; // Type of event (matches EVENT_* constants)
    };
} // namespace enigma::graphic
