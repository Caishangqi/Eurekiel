#pragma once
//-----------------------------------------------------------------------------------------------
// ShaderScanHelper.hpp
//
// [NEW] Pure static utility class for discovering and matching shader program files
//
// This helper provides:
//   - ScanShaderPrograms: Scan directory for valid VS/PS shader pairs
//   - FindShaderFiles: Find specific program's VS/PS file paths
//   - MatchProgramsByPattern: Regex-based program name matching
//
// Design Principles (SOLID + KISS):
//   - Single Responsibility: Only handles shader file discovery and matching
//   - Stateless: All methods are static, no instance creation allowed
//   - Error Handling: Returns empty containers (no exceptions thrown)
//   - Dependency: Relies on Core/FileSystemHelper for filesystem operations
//
// Shader File Naming Convention:
//   {programName}.vs.hlsl  - Vertex shader
//   {programName}.ps.hlsl  - Pixel shader
//
// Example:
//   gbuffers_basic.vs.hlsl + gbuffers_basic.ps.hlsl => programName = "gbuffers_basic"
//
// Usage:
//   auto programs = ShaderScanHelper::ScanShaderPrograms(shaderDir);
//   for (const auto& program : programs) {
//       auto files = ShaderScanHelper::FindShaderFiles(shaderDir, program);
//       if (files) {
//           // Use files->first (VS) and files->second (PS)
//       }
//   }
//
//   auto matches = ShaderScanHelper::MatchProgramsByPattern(programs, "gbuffers_.*");
//
// Teaching Points:
//   - Pure static class pattern (all constructors deleted)
//   - std::optional for optional return values (not-found cases)
//   - std::regex for flexible pattern matching
//   - C++17 compatible (no std::string::ends_with which is C++20)
//
//-----------------------------------------------------------------------------------------------

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <utility>

namespace enigma::graphic
{
    //-------------------------------------------------------------------------------------------
    // ShaderScanHelper
    //
    // [NEW] Static utility class for shader file discovery and matching
    // All methods are static, class cannot be instantiated
    //-------------------------------------------------------------------------------------------
    class ShaderScanHelper
    {
    public:
        // Prevent instantiation - pure static utility class
        ShaderScanHelper()                                   = delete;
        ~ShaderScanHelper()                                  = delete;
        ShaderScanHelper(const ShaderScanHelper&)            = delete;
        ShaderScanHelper& operator=(const ShaderScanHelper&) = delete;

        //-------------------------------------------------------------------------------------------
        // ScanShaderPrograms
        //
        // [NEW] Scan directory for valid shader program pairs
        //
        // This method scans a directory for .vs.hlsl files and checks if corresponding
        // .ps.hlsl files exist. Only programs with both VS and PS files are included.
        //
        // Parameters:
        //   directory - Path to shader directory (e.g., shaders/program/ or shaders/bundle/xxx/)
        //
        // Returns:
        //   Vector of program names (without .vs.hlsl suffix)
        //   Returns empty vector if directory doesn't exist or no valid pairs found
        //
        // Example:
        //   Directory contains: gbuffers_basic.vs.hlsl, gbuffers_basic.ps.hlsl, orphan.vs.hlsl
        //   Returns: {"gbuffers_basic"} (orphan.vs.hlsl is excluded - no matching .ps.hlsl)
        //-------------------------------------------------------------------------------------------
        static std::vector<std::string> ScanShaderPrograms(
            const std::filesystem::path& directory);

        //-------------------------------------------------------------------------------------------
        // FindShaderFiles
        //
        // [NEW] Find VS and PS shader files for a specific program
        //
        // This method looks for {programName}.vs.hlsl and {programName}.ps.hlsl
        // in the specified directory.
        //
        // Parameters:
        //   directory   - Path to shader directory
        //   programName - Name of the shader program (without extension)
        //
        // Returns:
        //   std::optional containing pair of (vsPath, psPath) if both files exist
        //   std::nullopt if either file is missing
        //
        // Example:
        //   auto files = FindShaderFiles(dir, "gbuffers_basic");
        //   if (files) {
        //       files->first  => ".../gbuffers_basic.vs.hlsl"
        //       files->second => ".../gbuffers_basic.ps.hlsl"
        //   }
        //-------------------------------------------------------------------------------------------
        static std::optional<std::pair<std::filesystem::path, std::filesystem::path>> FindShaderFiles(
            const std::filesystem::path& directory,
            const std::string&           programName);

        //-------------------------------------------------------------------------------------------
        // MatchProgramsByPattern
        //
        // [NEW] Filter program names using regex pattern
        //
        // This method applies a regex pattern to filter program names.
        // Useful for finding related programs (e.g., all "gbuffers_*" programs).
        //
        // Parameters:
        //   programNames - Vector of program names to filter
        //   pattern      - Regex pattern to match against names
        //
        // Returns:
        //   Vector of program names that match the pattern
        //   Returns empty vector if no matches or invalid regex
        //
        // Example:
        //   Input: {"gbuffers_basic", "gbuffers_textured", "composite", "final"}
        //   Pattern: "gbuffers_.*"
        //   Returns: {"gbuffers_basic", "gbuffers_textured"}
        //
        // Note: Uses std::regex_match (full string match, not partial)
        //-------------------------------------------------------------------------------------------
        static std::vector<std::string> MatchProgramsByPattern(
            const std::vector<std::string>& programNames,
            const std::string&              pattern);

    private:
        //-------------------------------------------------------------------------------------------
        // Helper constants for shader file extensions
        //-------------------------------------------------------------------------------------------
        static constexpr const char* VS_EXTENSION = ".vs.hlsl";
        static constexpr const char* PS_EXTENSION = ".ps.hlsl";

        //-------------------------------------------------------------------------------------------
        // EndsWith
        //
        // [INTERNAL] C++17 compatible string suffix check
        // Replaces std::string::ends_with() which is C++20 only
        //-------------------------------------------------------------------------------------------
        static bool EndsWith(const std::string& str, const std::string& suffix);
    };
} // namespace enigma::graphic
