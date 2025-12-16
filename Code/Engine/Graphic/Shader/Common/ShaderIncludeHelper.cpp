/**
 * @file ShaderIncludeHelper.cpp
 * @brief Shader Include System Helper Implementation
 * @date 2025-11-04
 * @author Caizii
 *
 * [IMPLEMENTATION]
 * Implements convenience functions for path handling, include graph building,
 * and include expansion operations.
 *
 * [STRATEGY]
 * - Uses composition of existing components
 * - Provides factory methods and wrapper interfaces
 * - Focus on error handling and edge cases
 */

#include "Engine/Graphic/Shader/Common/ShaderIncludeHelper.hpp"
#include "Engine/Graphic/Shader/Program/Include/IncludeProcessor.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

#include "Engine/Graphic/Shader/Program/VirtualPathReader.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // Path Processing Functions
    // ========================================================================

    std::filesystem::path ShaderIncludeHelper::DetermineRootPath(const std::filesystem::path& anyPath)
    {
        // [NOTE] Path inference algorithm - finds reasonable root directory from file path

        // Step 1: Check if path exists
        if (!std::filesystem::exists(anyPath))
        {
            return anyPath.parent_path();
        }

        std::filesystem::path currentPath = anyPath;

        // Step 2: If file path, use parent directory
        if (std::filesystem::is_regular_file(currentPath))
        {
            currentPath = currentPath.parent_path();
        }

        // Step 3: Search upward for common project root markers
        while (!currentPath.empty() && currentPath != currentPath.root_path())
        {
            bool foundRootMarker = false;

            for (const auto& entry : std::filesystem::directory_iterator(currentPath))
            {
                if (!entry.is_directory()) continue;

                const std::string dirName = entry.path().filename().string();

                // Common root directory markers
                if (dirName == "shaders" || dirName == "src" || dirName == "assets" ||
                    dirName == "resources" || dirName == "include")
                {
                    foundRootMarker = true;
                    break;
                }
            }

            if (foundRootMarker)
            {
                return currentPath;
            }

            currentPath = currentPath.parent_path();
        }

        // Step 4: Fall back to parent directory if no root marker found
        if (std::filesystem::is_regular_file(anyPath))
        {
            return anyPath.parent_path();
        }
        else
        {
            return anyPath.parent_path().empty() ? anyPath : anyPath.parent_path();
        }
    }

    bool ShaderIncludeHelper::IsPathWithinRoot(
        const std::filesystem::path& path,
        const std::filesystem::path& root)
    {
        // [NOTE] Path security check - prevents path traversal attacks

        try
        {
            // Step 1: Convert both paths to absolute and canonical form
            std::filesystem::path absolutePath = std::filesystem::absolute(path);
            std::filesystem::path absoluteRoot = std::filesystem::absolute(root);

            // Step 2: Canonicalize paths (resolve .. and .)
            std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(absolutePath);
            std::filesystem::path canonicalRoot = std::filesystem::weakly_canonical(absoluteRoot);

            // Step 3: Ensure root path ends with separator for prefix comparison
            if (!canonicalRoot.string().empty() && canonicalRoot.string().back() != '/' &&
                canonicalRoot.string().back() != '\\')
            {
                canonicalRoot = canonicalRoot / "";
            }

            // Step 4: Check if path starts with root
            std::string pathStr = canonicalPath.string();
            std::string rootStr = canonicalRoot.string();

            return pathStr.find(rootStr) == 0;
        }
        catch (const std::filesystem::filesystem_error&)
        {
            return false;
        }
    }

    std::string ShaderIncludeHelper::NormalizePath(const std::string& path)
    {
        // [NOTE] Path normalization - unifies path separators and handles relative paths

        if (path.empty())
        {
            return path;
        }

        std::string result;
        result.reserve(path.length());

        std::vector<std::string> components;
        std::string              current;

        // Step 1: Parse path components and handle relative paths
        for (char c : path)
        {
            if (c == '/' || c == '\\')
            {
                if (!current.empty())
                {
                    if (current == ".")
                    {
                        // Ignore current directory
                    }
                    else if (current == "..")
                    {
                        // Go to parent directory
                        if (!components.empty())
                        {
                            components.pop_back();
                        }
                    }
                    else
                    {
                        components.push_back(current);
                    }
                    current.clear();
                }
            }
            else
            {
                current += c;
            }
        }

        // Handle last component
        if (!current.empty())
        {
            if (current == ".")
            {
                // Ignore current directory
            }
            else if (current == "..")
            {
                if (!components.empty())
                {
                    components.pop_back();
                }
            }
            else
            {
                components.push_back(current);
            }
        }

        // Step 2: Rebuild normalized path (using / as separator)
        for (size_t i = 0; i < components.size(); ++i)
        {
            if (i > 0)
            {
                result += "/";
            }
            result += components[i];
        }

        return result;
    }

    ShaderPath ShaderIncludeHelper::ResolveRelativePath(
        const ShaderPath&  basePath,
        const std::string& relativePath)
    {
        // [NOTE] Relative path resolution - resolves relative path based on base path

        try
        {
            return basePath.Resolve(relativePath);
        }
        catch (const std::exception&)
        {
            throw;
        }
    }

    // ========================================================================
    // Include Graph Building
    // ========================================================================

    std::unique_ptr<IncludeGraph> ShaderIncludeHelper::BuildFromFiles(
        const std::filesystem::path&    anyPath,
        const std::vector<std::string>& relativeShaderPaths)
    {
        // [NOTE] Convenience wrapper for building include graph from file system

        try
        {
            // Step 1: Infer root directory
            std::filesystem::path rootPath = DetermineRootPath(anyPath);

            // Step 2: Convert relative paths to ShaderPath objects
            std::vector<ShaderPath> shaderPaths;
            shaderPaths.reserve(relativeShaderPaths.size());

            for (const auto& relativePath : relativeShaderPaths)
            {
                // Ensure relative path starts with / (ShaderPath requires absolute path)
                std::string absolutePath = relativePath;
                if (!absolutePath.empty() && absolutePath[0] != '/')
                {
                    absolutePath = "/" + absolutePath;
                }

                try
                {
                    ShaderPath shaderPath = ShaderPath::FromAbsolutePath(absolutePath);
                    shaderPaths.push_back(std::move(shaderPath));
                }
                catch (const std::exception&)
                {
                    // Skip invalid path format
                    continue;
                }
            }

            // Step 3: Build include graph using BuildFromVirtualPaths
            return BuildFromVirtualPaths(rootPath, shaderPaths);
        }
        catch (const std::exception&)
        {
            return nullptr;
        }
    }

    std::unique_ptr<IncludeGraph> ShaderIncludeHelper::BuildFromVirtualPaths(
        const std::filesystem::path&   rootPath,
        const std::vector<ShaderPath>& shaderPaths)
    {
        // [NOTE] Precise control - explicit root directory specification

        try
        {
            // Step 1: Create VirtualPathReader
            auto fileReader = std::make_shared<VirtualPathReader>(rootPath);

            // Step 2: Build IncludeGraph
            auto graph = std::make_unique<IncludeGraph>(fileReader, shaderPaths);

            // Step 3: Return built include graph
            return graph;
        }
        catch (const std::exception&)
        {
            // Build failed (possibly circular dependency or other error)
            return nullptr;
        }
    }

    // ========================================================================
    // Include Expansion
    // ========================================================================

    std::string ShaderIncludeHelper::ExpandShaderSource(
        const IncludeGraph& graph,
        const ShaderPath&   shaderPath,
        bool                withLineDirectives)
    {
        // [NOTE] Include expansion wrapper - simplifies IncludeProcessor usage

        // Step 1: Verify shaderPath exists in IncludeGraph
        if (!graph.HasNode(shaderPath))
        {
            throw std::invalid_argument(
                "Shader path not found in IncludeGraph: " + shaderPath.GetPathString()
            );
        }

        // Step 2: Expand based on withLineDirectives flag
        if (withLineDirectives)
        {
            return IncludeProcessor::ExpandWithLineDirectives(graph, shaderPath);
        }
        else
        {
            return IncludeProcessor::Expand(graph, shaderPath);
        }
    }
} // namespace enigma::graphic
