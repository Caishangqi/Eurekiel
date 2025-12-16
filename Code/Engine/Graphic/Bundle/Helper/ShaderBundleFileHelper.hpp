#pragma once
//-----------------------------------------------------------------------------------------------
// ShaderBundleFileHelper.hpp
//
// [NEW] Pure static utility class for validating ShaderBundle directory structures
//
// This helper provides:
//   - IsValidShaderBundleDirectory: Quick validation check for bundle directory
//   - HasRequiredStructure: Full structure validation with shaders/, bundle.json, program|bundle/
//
// Design Principles (SOLID + KISS):
//   - Single Responsibility: Only handles directory structure validation
//   - Stateless: All methods are static, no instance creation allowed
//   - Error Handling: Returns bool (no exceptions thrown)
//   - Dependency: Relies on Core/FileSystemHelper for filesystem operations
//
// Expected ShaderBundle Directory Structure:
//   {BundleRoot}/
//   ├── shaders/
//   │   ├── bundle.json           (REQUIRED - bundle metadata)
//   │   ├── fallback_rule.json    (OPTIONAL - fallback rules)
//   │   ├── bundle/               (OPTIONAL* - user-defined shader bundles)
//   │   │   └── {bundle_name}/
//   │   │       ├── gbuffers_basic.vs.hlsl
//   │   │       └── gbuffers_basic.ps.hlsl
//   │   └── program/              (OPTIONAL* - standalone shader programs)
//   │       ├── gbuffers_textured.vs.hlsl
//   │       └── gbuffers_textured.ps.hlsl
//   * At least one of bundle/ or program/ must exist
//
// Usage:
//   if (ShaderBundleFileHelper::IsValidShaderBundleDirectory(path)) {
//       // Load the bundle
//   }
//
//   if (ShaderBundleFileHelper::HasRequiredStructure(bundleRoot)) {
//       // Full structure validation passed
//   }
//
// Teaching Points:
//   - Pure static class pattern (all constructors deleted)
//   - Boolean return for validation (simple and clear)
//   - Reuse FileSystemHelper instead of duplicating filesystem logic
//
//-----------------------------------------------------------------------------------------------

#include <filesystem>

namespace enigma::graphic
{
    //-------------------------------------------------------------------------------------------
    // ShaderBundleFileHelper
    //
    // [NEW] Static utility class for ShaderBundle directory validation
    // All methods are static, class cannot be instantiated
    //-------------------------------------------------------------------------------------------
    class ShaderBundleFileHelper
    {
    public:
        // Prevent instantiation - pure static utility class
        ShaderBundleFileHelper()                                         = delete;
        ~ShaderBundleFileHelper()                                        = delete;
        ShaderBundleFileHelper(const ShaderBundleFileHelper&)            = delete;
        ShaderBundleFileHelper& operator=(const ShaderBundleFileHelper&) = delete;

        //-------------------------------------------------------------------------------------------
        // IsValidShaderBundleDirectory
        //
        // [NEW] Quick validation check for ShaderBundle directory
        //
        // This method performs a minimal check to determine if a directory could be
        // a valid ShaderBundle by checking for the presence of shaders/bundle.json
        //
        // Parameters:
        //   directory - Path to potential ShaderBundle root directory
        //
        // Returns:
        //   true  - Directory exists and contains shaders/bundle.json
        //   false - Directory doesn't exist OR shaders/bundle.json is missing
        //
        // Note: This is a quick check. Use HasRequiredStructure() for full validation.
        //-------------------------------------------------------------------------------------------
        static bool IsValidShaderBundleDirectory(const std::filesystem::path& directory);

        //-------------------------------------------------------------------------------------------
        // HasRequiredStructure
        //
        // [NEW] Full structure validation for ShaderBundle directory
        //
        // This method performs comprehensive validation:
        //   1. Checks shaders/ subdirectory exists
        //   2. Checks bundle.json exists in shaders/
        //   3. Checks at least one of bundle/ or program/ directory exists
        //
        // Parameters:
        //   bundleRoot - Path to ShaderBundle root directory
        //
        // Returns:
        //   true  - All required structure elements are present
        //   false - Any required element is missing
        //
        // Structure Requirements:
        //   - shaders/           (REQUIRED)
        //   - shaders/bundle.json (REQUIRED)
        //   - shaders/bundle/    (OPTIONAL, but at least one of bundle/ or program/ needed)
        //   - shaders/program/   (OPTIONAL, but at least one of bundle/ or program/ needed)
        //-------------------------------------------------------------------------------------------
        static bool HasRequiredStructure(const std::filesystem::path& bundleRoot);
    };
} // namespace enigma::graphic
