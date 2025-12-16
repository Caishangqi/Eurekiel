/**
 * @file ShaderIncludeHelper.hpp
 * @brief Shader Include System Helper - Unified utility functions for Include system
 * @date 2025-11-04
 * @author Caizii
 *
 * Architecture:
 * ShaderIncludeHelper is a pure static utility class providing convenient tool function interfaces\n * for the Shader Include system. It integrates path handling, Include graph building, and Include\n * expansion features, simplifying the usage of the Include system.
 *
 * Design Principles:
 * - Pure Static Class: All functions are static, instantiation prohibited\n * - Unified Interface: Provides consistent tool function access for Include system\n * - Convenience Wrapper: Simplifies implementation complexity of common operations\n * - Functional Grouping: Functions organized by domain (path handling, graph building, expansion)
 *
 * Responsibilities:
 * - [ADD] Path handling utilities (DetermineRootPath, IsPathWithinRoot, NormalizePath, ResolveRelativePath)\n * - [ADD] Include graph building convenience interfaces (BuildFromFiles, BuildFromVirtualPaths)\n * - [ADD] Include expansion convenience interfaces (ExpandShaderSource)\n * - [NO] Not responsible for file IO implementation (delegated to IFileReader)\n * - [NO] Not responsible for Include graph algorithm implementation (delegated to IncludeGraph)\n * - [NO] Not responsible for Include expansion algorithm implementation (delegated to IncludeProcessor)
 *
 * Usage Examples:
 * @code
 * // ========== Path Handling Examples ==========
 * 
 * // 1. Infer root directory from any path
 * std::filesystem::path root = ShaderIncludeHelper::DetermineRootPath("F:/MyProject/ShaderBundle/shaders/main.hlsl");
 * // root = "F:/MyProject/ShaderBundle"
 * 
 * // 2. Check path safety
 * bool safe = ShaderIncludeHelper::IsPathWithinRoot("F:/MyProject/ShaderBundle/shaders/lib/common.hlsl", "F:/MyProject/ShaderBundle");
 * // safe = true
 * 
 * // 3. Normalize path string
 * std::string normalized = ShaderIncludeHelper::NormalizePath("shaders\\..\\lib\\common.hlsl");
 * // normalized = "shaders/lib/common.hlsl"
 * 
 * // 4. Resolve relative path
 * ShaderPath basePath = ShaderPath::FromAbsolutePath("/shaders/gbuffers_terrain.hlsl");
 * ShaderPath includePath = ShaderIncludeHelper::ResolveRelativePath(basePath, "../lib/common.hlsl");
 * // includePath = "/shaders/lib/common.hlsl"
 * 
 * // ========== Include Graph Building Examples ==========
 * 
 * // 5. Build Include graph from filesystem
 * std::vector<std::string> shaderFiles = {
 *     "shaders/gbuffers_terrain.vs.hlsl",
 *     "shaders/gbuffers_terrain.ps.hlsl"
 * };
 * auto graph1 = ShaderIncludeHelper::BuildFromFiles("F:/MyProject/ShaderBundle", shaderFiles);
 * 
 * // 6. Build Include graph from virtual paths
 * std::vector<ShaderPath> shaderPaths = {
 *     ShaderPath::FromAbsolutePath("/shaders/gbuffers_terrain.vs.hlsl"),
 *     ShaderPath::FromAbsolutePath("/shaders/gbuffers_terrain.ps.hlsl")
 * };
 * auto graph2 = ShaderIncludeHelper::BuildFromVirtualPaths("F:/MyProject/ShaderBundle", shaderPaths);
 * 
 * // ========== Include Expansion Examples ==========
 * 
 * // 7. Expand shader source code
 * if (graph2 && !graph2->GetFailures().empty()) {
 *     ShaderPath mainPath = ShaderPath::FromAbsolutePath("/shaders/gbuffers_terrain.vs.hlsl");
 *     std::string expanded = ShaderIncludeHelper::ExpandShaderSource(*graph2, mainPath);
 *     
 *     // Or include line directives (for debugging)
 *     std::string expandedWithLines = ShaderIncludeHelper::ExpandShaderSource(*graph2, mainPath, true);
 * }
 * 
 * // ========== Complete Usage Flow Example ==========
 * 
 * // Complete flow: from shader bundle directory to code expansion
 * std::string packRoot = "F:/MyProject/ShaderBundle";
 * std::vector<std::string> programFiles = {
 *     "shaders/composite.fsh.hlsl",
 *     "shaders/gbuffers_basic.vs.hlsl",
 *     "shaders/gbuffers_basic.ps.hlsl"
 * };
 * 
 * // Build Include graph
 * auto includeGraph = ShaderIncludeHelper::BuildFromFiles(packRoot, programFiles);
 * if (!includeGraph) {
 *     // Handle build failure
 *     return;
 * }
 * 
 * // Check for build failures
 * if (!includeGraph->GetFailures().empty()) {
 *     for (const auto& [path, error] : includeGraph->GetFailures()) {
 *         std::cerr << "Include error: " << path.GetPathString() << " - " << error << std::endl;
 *     }
 * }
 * 
 * // Expand all programs
 * for (const auto& file : programFiles) {
 *     ShaderPath shaderPath = ShaderPath::FromAbsolutePath("/" + file);
 *     if (includeGraph->HasNode(shaderPath)) {
 *         std::string expandedCode = ShaderIncludeHelper::ExpandShaderSource(*includeGraph, shaderPath);
 *         // Use expanded code for compilation...
 *     }
 * }
 * @endcode
 *
 * Key Concepts:
 * - Utility Class Pattern
 * - Static Method Design
 * - Convenience Function Wrapper
 * - Path Handling Best Practices
 * - Include System Architecture
 */

#pragma once

#include "Engine/Graphic/Shader/Program/Include/IncludeGraph.hpp"
#include "Engine/Graphic/Shader/Program/Include/IncludeProcessor.hpp"
#include "Engine/Graphic/Shader/Program/Include/ShaderPath.hpp"
#include "Engine/Graphic/Shader/Common/FileSystemReader.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <memory>

namespace enigma::graphic
{
    /**
     * @class ShaderIncludeHelper
     * @brief Shader Include System Helper - Pure Static Utility Class
     *
     * Design Philosophy:
     * - Pure Static Class: All methods are static, instantiation prohibited
     * - Functional Grouping: Organized by domain (path handling, graph building, expansion)
     * - Convenience Wrapper: Simplifies common Include system operations
     * - Unified Interface: Provides consistent tool function access for Include system
     *
     * Functional Groups:
     * 1. **Path Handling Functions**:
     *    - DetermineRootPath: Auto-detect root directory from any path
     *    - IsPathWithinRoot: Check if path is within root directory
     *    - NormalizePath: Normalize path string
     *    - ResolveRelativePath: Resolve relative path
     *
     * 2. **Include Graph Building Convenience Interface**:
     *    - BuildFromFiles: Build Include graph from filesystem
     *    - BuildFromVirtualPaths: Build Include graph from virtual paths
     *
     * 3. **Include Expansion Convenience Interface**:
     *    - ExpandShaderSource: Expand shader source code (supports optional line directives)
     *
     * Key Concepts:
     * - Utility Class Pattern
     * - Static Method Design
     * - Dependency Injection Principle
     * - Facade Pattern
     * - RAII Resource Management
     */
    class ShaderIncludeHelper
    {
    public:
        // Disable instantiation (pure static utility class)
        ShaderIncludeHelper()                                      = delete;
        ~ShaderIncludeHelper()                                     = delete;
        ShaderIncludeHelper(const ShaderIncludeHelper&)            = delete;
        ShaderIncludeHelper& operator=(const ShaderIncludeHelper&) = delete;

        // ========================================================================
        // Path Handling Functions
        // ========================================================================

        /**
         * @brief Auto-detect root directory from any path
         * @param anyPath Any path (file path or directory path)
         * @return std::filesystem::path Detected root directory
         *
         * Key Concepts:
         * - Path Inference Algorithm: Search upward from file path for valid root
         * - Common Directory Structure Recognition: shaders/, src/, assets/, etc.
         * - Error Handling: Return parent directory when unable to identify
         *
         * Detection Strategy:
         * 1. If path points to file -> Use file's parent directory
         * 2. Search for common project root identifiers:
         *    - shaders/ directory exists -> Use its parent as root
         *    - src/ directory exists -> Use its parent as root
         *    - assets/ directory exists -> Use its parent as root
         * 3. If cannot identify -> Return path's parent directory
         *
         * Example:
         * @code
         * // Infer from file path
         * auto root1 = ShaderIncludeHelper::DetermineRootPath("F:/MyProject/ShaderBundle/shaders/main.hlsl");
         * // root1 = "F:/MyProject/ShaderBundle"
         *
         * // Infer from directory path
         * auto root2 = ShaderIncludeHelper::DetermineRootPath("F:/MyProject/ShaderBundle/shaders");
         * // root2 = "F:/MyProject/ShaderBundle"
         *
         * // Infer from deep path
         * auto root3 = ShaderIncludeHelper::DetermineRootPath("F:/project/src/shaders/lib/common.hlsl");
         * // root3 = "F:/project/src"
         * @endcode
         *
         * Notes:
         * - [WARNING] This is a heuristic algorithm, may not be accurate
         * - [WARNING] For precise control, recommend specifying root directly
         * - [WARNING] Path must exist for correct inference
         */
        static std::filesystem::path DetermineRootPath(const std::filesystem::path& anyPath);

        /**
         * @brief Check if path is within root directory
         * @param path Path to check
         * @param root Root directory path
         * @return bool Returns true if within root, false otherwise
         *
         * Key Concepts:
         * - Path Safety Check: Prevent path traversal attacks
         * - Canonical Path Handling: Use std::filesystem normalization
         * - Symlink Safety: Handle symlink security risks
         *
         * Check Strategy:
         * 1. Convert both paths to absolute and canonical form
         * 2. Check if path starts with root
         * 3. Ensure does not cross root boundary
         * 4. Handle symlink security risks
         *
         * Example:
         * @code
         * std::filesystem::path root = "F:/MyProject/ShaderBundle";
         * 
         * // Safe path
         * bool safe1 = ShaderIncludeHelper::IsPathWithinRoot("F:/MyProject/ShaderBundle/shaders/lib/common.hlsl", root);
         * // safe1 = true
         *
         * // Dangerous path (path traversal attack)
         * bool safe2 = ShaderIncludeHelper::IsPathWithinRoot("F:/MyProject/ShaderBundle/../../../etc/passwd", root);
         * // safe2 = false
         * @endcode
         *
         * Security Features:
         * - Prevent path traversal attacks (../etc/passwd)
         * - Handle symlink security risks
         * - Cross-platform path handling
         */
        static bool IsPathWithinRoot(
            const std::filesystem::path& path,
            const std::filesystem::path& root
        );

        /**
         * @brief Normalize path string
         * @param path Original path string
         * @return std::string Normalized path string
         *
         * Key Concepts:
         * - Path Normalization: Unify path separators and handle relative paths
         * - Cross-platform Compatibility: Convert Windows backslash to Unix forward slash
         * - Relative Path Handling: Resolve . and .. components
         *
         * Normalization Rules:
         * 1. Unify using / as path separator
         * 2. Resolve . (current directory) components
         * 3. Resolve .. (parent directory) components
         * 4. Remove redundant consecutive separators
         * 5. Remove trailing separator (unless root path)
         *
         * Example:
         * @code
         * // Windows path normalization
         * std::string norm1 = ShaderIncludeHelper::NormalizePath("shaders\\lib\\common.hlsl");
         * // norm1 = "shaders/lib/common.hlsl"
         *
         * // Relative path handling
         * std::string norm2 = ShaderIncludeHelper::NormalizePath("shaders/./lib/../common.hlsl");
         * // norm2 = "shaders/common.hlsl"
         *
         * // Redundant separator handling
         * std::string norm3 = ShaderIncludeHelper::NormalizePath("shaders//lib///common.hlsl");
         * // norm3 = "shaders/lib/common.hlsl"
         * @endcode
         *
         * Notes:
         * - [WARNING] This is string-level normalization, does not access filesystem
         * - [WARNING] Does not validate if path exists
         * - [WARNING] Does not resolve symlinks
         */
        static std::string NormalizePath(const std::string& path);

        /**
         * @brief Resolve relative path
         * @param basePath Base path (absolute path within virtual path system)
         * @param relativePath Relative path string
         * @return ShaderPath Resolved absolute path
         *
         * Key Concepts:
         * - Relative Path Resolution: Resolve relative path based on base path
         * - ShaderPath Encapsulation: Returns ShaderPath object instead of string
         * - Include Directive Handling: Primarily for handling paths in #include directives
         *
         * Resolution Rules:
         * - If relativePath starts with / -> Use directly as absolute path
         * - Otherwise -> Resolve based on basePath
         * - Uses ShaderPath::Resolve() for actual resolution
         *
         * Example:
         * @code
         * ShaderPath basePath = ShaderPath::FromAbsolutePath("/shaders/gbuffers_terrain.hlsl");
         *
         * // Relative path resolution
         * ShaderPath path1 = ShaderIncludeHelper::ResolveRelativePath(basePath, "../lib/common.hlsl");
         * // path1 = "/shaders/lib/common.hlsl"
         *
         * // Absolute path (starts with /)
         * ShaderPath path2 = ShaderIncludeHelper::ResolveRelativePath(basePath, "/engine/Common.hlsl");
         * // path2 = "/engine/Common.hlsl"
         *
         * // Same level directory
         * ShaderPath path3 = ShaderIncludeHelper::ResolveRelativePath(basePath, "lighting.glsl");
         * // path3 = "/shaders/lighting.glsl"
         * @endcode
         *
         * Use Cases:
         * - Include directive resolution (#include "../lib/common.hlsl")
         * - File path calculation
         * - Resource location
         */
        static ShaderPath ResolveRelativePath(
            const ShaderPath&  basePath,
            const std::string& relativePath
        );

        // ========================================================================
        // Include Graph Building Convenience Interface
        // ========================================================================

        /**
         * @brief Build Include graph from filesystem
         * @param anyPath Any path (used to infer root directory)
         * @param relativeShaderPaths List of relative shader paths
         * @return std::unique_ptr<IncludeGraph> Built Include graph (returns nullptr on failure)
         *
         * Key Concepts:
         * - Convenience Interface: Simplify Build Include graph from filesystem flow
         * - Auto Inference: Automatically infer shader bundle root directory
         * - Error Handling: Returns nullptr on build failure
         *
         * Build Process:
         * 1. Use DetermineRootPath() to infer shader bundle root directory
         * 2. Convert relative paths to ShaderPath objects
         * 3. Create FileSystemReader and build IncludeGraph
         * 4. Return built Include graph (returns nullptr on failure)
         *
         * Example:
         * @code
         * std::vector<std::string> shaderFiles = {
         *     "shaders/gbuffers_terrain.vs.hlsl",
         *     "shaders/gbuffers_terrain.ps.hlsl",
         *     "shaders/lib/common.hlsl"
         * };
         *
         * auto graph = ShaderIncludeHelper::BuildFromFiles("F:/MyProject/ShaderBundle", shaderFiles);
         * if (graph) {
         *     std::cout << "Include graph built successfully" << std::endl;
         *     std::cout << "Nodes: " << graph->GetNodes().size() << std::endl;
         * } else {
         *     std::cout << "Failed to build include graph" << std::endl;
         * }
         * @endcode
         *
         * Notes:
         * - [WARNING] Returns nullptr on build failure, caller must check
         * - [WARNING] May throw std::runtime_error (circular dependency)
         * - [WARNING] Uses relative paths based on inferred root directory
         */
        static std::unique_ptr<IncludeGraph> BuildFromFiles(
            const std::filesystem::path&    anyPath,
            const std::vector<std::string>& relativeShaderPaths
        );

        /**
         * @brief Build Include graph from virtual paths
         * @param packRoot Root directory (filesystem path)
         * @param shaderPaths List of shader paths (ShaderPath objects)
         * @return std::unique_ptr<IncludeGraph> Built Include graph (returns nullptr on failure)
         *
         * Key Concepts:
         * - Precise Control: Explicitly specify shader bundle root directory
         * - ShaderPath Interface: Use ShaderPath objects instead of strings
         * - VirtualPathReader: Use specialized virtual path file reader
         *
         * Build Process:
         * 1. Create VirtualPathReader (based on packRoot)
         * 2. Build IncludeGraph using VirtualPathReader
         * 3. Return built Include graph (returns nullptr on failure)
         *
         * Example:
         * @code
         * std::filesystem::path packRoot = "F:/MyProject/ShaderBundle";
         * std::vector<ShaderPath> shaderPaths = {
         *     ShaderPath::FromAbsolutePath("/shaders/gbuffers_terrain.vs.hlsl"),
         *     ShaderPath::FromAbsolutePath("/shaders/gbuffers_terrain.ps.hlsl"),
         *     ShaderPath::FromAbsolutePath("/shaders/lib/common.hlsl")
         * };
         *
         * auto graph = ShaderIncludeHelper::BuildFromVirtualPaths(packRoot, shaderPaths);
         * if (graph) {
         *     // Check for build failures
         *     if (!graph->GetFailures().empty()) {
         *         for (const auto& [path, error] : graph->GetFailures()) {
         *             std::cerr << "Include error: " << path.GetPathString() << " - " << error << std::endl;
         *         }
         *     }
         * }
         * @endcode
         *
         * Advantages:
         * - Precise root directory control
         * - Optimized performance using VirtualPathReader
         * - Support shader bundle special file access logic
         *
         * Notes:
         * - [WARNING] Returns nullptr on build failure, caller must check
         * - [WARNING] May throw std::runtime_error (circular dependency)
         * - [WARNING] packRoot must point to valid shader bundle directory
         */
        static std::unique_ptr<IncludeGraph> BuildFromVirtualPaths(
            const std::filesystem::path&   packRoot,
            const std::vector<ShaderPath>& shaderPaths
        );

        // ========================================================================
        // Include Expansion Convenience Interface
        // ========================================================================

        /**
         * @brief Expand all #include directives in shader source code
         * @param graph Built Include dependency graph
         * @param shaderPath Shader path to expand
         * @param withLineDirectives Whether to include line directives (default false)
         * @return std::string Expanded complete shader code
         *
         * Key Concepts:
         * - Include Expansion: Replace all #include directives with actual file content
         * - Line Directives: Optionally insert #line directives for debugging
         * - Convenience Interface: Simplify IncludeProcessor usage
         *
         * Expansion Process:
         * 1. Verify shaderPath exists in IncludeGraph
         * 2. Choose expansion method based on withLineDirectives:
         *    - false -> Use IncludeProcessor::Expand()
         *    - true -> Use IncludeProcessor::ExpandWithLineDirectives()
         * 3. Return expanded code
         *
         * HLSL #line directive format:
         * - `#line <line_number> "<file_path>"`
         * - Used for DXC compilation error location to original file
         *
         * Example:
         * @code
         * auto graph = ShaderIncludeHelper::BuildFromFiles(packRoot, shaderFiles);
         * if (graph) {
         *     ShaderPath mainPath = ShaderPath::FromAbsolutePath("/shaders/gbuffers_terrain.vs.hlsl");
         *     
         *     // Simple expansion (release build)
         *     std::string expanded = ShaderIncludeHelper::ExpandShaderSource(*graph, mainPath);
         *     
         *     // Include line directives (development build)
         *     std::string expandedWithLines = ShaderIncludeHelper::ExpandShaderSource(*graph, mainPath, true);
         * }
         * @endcode
         *
         * Input Example:
         * ```hlsl
         * // gbuffers_terrain.vs.hlsl
         * #include "Common.hlsl"
         * #include "lib/Lighting.hlsl"
         *
         * void main() {
         *     // shader code here
         * }
         * ```
         *
         * Output Example (withLineDirectives=true):
         * ```hlsl
         * #line 1 "/shaders/Common.hlsl"
         * // Common.hlsl content...
         * #line 1 "/shaders/lib/Lighting.hlsl"
         * // Lighting.hlsl content...
         * #line 3 "/shaders/gbuffers_terrain.vs.hlsl"
         *
         * void main() {
         *     // shader code here
         * }
         * ```
         *
         * Exceptions:
         * - std::invalid_argument: If shaderPath not in IncludeGraph
         *
         * Recommendations:
         * - Development: Use withLineDirectives=true (better error location)
         * - Release: Use withLineDirectives=false (smaller file size)
         *
         * Notes:
         * - [WARNING] Requires valid IncludeGraph (caller ensures graph is not null)
         * - [WARNING] shaderPath must exist in IncludeGraph
         * - [WARNING] Large shaders may result in large expanded code
         */
        static std::string ExpandShaderSource(
            const IncludeGraph& graph,
            const ShaderPath&   shaderPath,
            bool                withLineDirectives = false
        );

    private:
        /**
         * Key Concepts Summary:
         * 1. **Utility Class Pattern**: Pure static class, all methods are static, instantiation prohibited
         * 2. **Functional Grouping**: Organized by domain (path handling, graph building, expansion)
         * 3. **Convenience Interface Wrapper**: Simplify implementation complexity of common operations
         * 4. **Path Safety Handling**: Prevent path traversal attacks, ensure file access safety
         * 5. **Dependency Injection Principle**: Accept dependencies instead of creating them, improve testability
         * 6. **Error Handling Strategy**: Use return values instead of exceptions for expected error cases
         * 7. **Cross-platform Compatibility**: Use std::filesystem to handle platform differences
         * 8. **RAII Resource Management**: Use smart pointers to manage dynamic resources
         */
    };
} // namespace enigma::graphic
