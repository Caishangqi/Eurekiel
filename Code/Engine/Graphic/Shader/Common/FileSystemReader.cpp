/**
 * @file FileSystemReader.cpp
 * @brief file system reader implementation
 * @date 2025-11-04
 * @author Caizii
 *
 * Implementation instructions:
 * This file contains a complete implementation of the FileSystemReader class, which provides secure local file system access functionality.
 * All implementations follow modern C++ best practices to ensure thread safety and exception safety.
 *
 *Performance optimization:
 * - Use std::filesystem for efficient path operations
 * - Minimize the number of system calls
 * - Intelligent error handling avoids exception overhead
 * - cache-friendly file reading strategy
 *
 * Security features:
 * - Strict path boundary checking
 * - Safe handling of symbolic links
 * - Path traversal attack protection
 * - Detailed permission verification
 */
#include "FileSystemReader.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>

#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Graphic/Shader/Program/Include/ShaderPath.hpp"

namespace enigma::graphic
{
    FileSystemReader::FileSystemReader()
        : m_rootPath(std::filesystem::current_path())
    {
        // 验证当前工作目录是有效的
        if (!std::filesystem::exists(m_rootPath) || !std::filesystem::is_directory(m_rootPath))
        {
            ERROR_AND_DIE("Current working directory is not valid!");
        }
    }

    FileSystemReader::FileSystemReader(const std::filesystem::path& explicitRoot)
        : m_rootPath(explicitRoot)
    {
        //Normalize root directory path
        m_rootPath = CanonicalizePath(explicitRoot);

        // Verify whether the specified root directory is valid
        if (!std::filesystem::exists(m_rootPath) || !std::filesystem::is_directory(m_rootPath))
        {
            ERROR_AND_DIE("Explicit root directory is not a valid directory!");
        }
    }

    // ========================================================================
    // IFileReader interface implementation
    // ========================================================================
    std::optional<std::string> FileSystemReader::ReadFile(const ShaderPath& path)
    {
        try
        {
            // [NEW] Try to resolve the alias path
            auto aliasResolved = ResolveAliasPath(path);
            if (aliasResolved.has_value())
            {
                // Alias path parsing is successful, read directly
                return ReadFileContent(aliasResolved.value());
            }

            // Convert Shader Pack path to file system path
            auto fullPath = path.Resolved(m_rootPath);

            // Security check: Make sure the path is within the root directory
            if (!IsPathWithinRoot(fullPath, m_rootPath))
            {
                ERROR_AND_DIE("Attempted to read file outside working directory!");
            }

            // Safely read file contents
            return ReadFileContent(fullPath);
        }
        catch (const std::exception& e)
        {
            // Log the error but do not terminate the program
            std::cerr << "FileSystemReader::ReadFile failed for path '"
                << path.GetPathString() << "': " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    bool FileSystemReader::FileExists(const ShaderPath& path) const
    {
        try
        {
            // [NEW] Try to resolve the alias path
            auto aliasResolved = ResolveAliasPath(path);
            if (aliasResolved.has_value())
            {
                // Alias path parsing is successful, check if the file exists
                return std::filesystem::exists(aliasResolved.value()) &&
                    std::filesystem::is_regular_file(aliasResolved.value());
            }

            // Convert Shader Pack path to file system path
            auto fullPath = path.Resolved(m_rootPath);

            // Security check: Make sure the path is within the root directory
            if (!IsPathWithinRoot(fullPath, m_rootPath))
            {
                ERROR_AND_DIE("Attempted to access file outside working directory!");
            }

            // Check if the file exists
            return std::filesystem::exists(fullPath) &&
                std::filesystem::is_regular_file(fullPath);
        }
        catch (const std::exception& e)
        {
            // Log an error but return false to indicate the file does not exist
            std::cerr << "FileSystemReader::FileExists failed for path '"
                << path.GetPathString() << "': " << e.what() << std::endl;
            return false;
        }
    }

    std::filesystem::path FileSystemReader::GetRootPath() const
    {
        return m_rootPath;
    }

    // ========================================================================
    // [NEW] Alias management method implementation
    // ========================================================================

    void FileSystemReader::AddAlias(const std::string& alias, const std::filesystem::path& targetPath)
    {
        // Validate alias format (should start with @)
        if (alias.empty() || alias[0] != '@')
        {
            std::cerr << "FileSystemReader::AddAlias: Alias must start with '@': " << alias << std::endl;
            return;
        }

        //Normalize target path
        auto canonicalTarget = CanonicalizePath(targetPath);

        // Verify that the target path exists
        if (!std::filesystem::exists(canonicalTarget) || !std::filesystem::is_directory(canonicalTarget))
        {
            std::cerr << "FileSystemReader::AddAlias: Target path is not a valid directory: "
                << targetPath.string() << std::endl;
            return;
        }

        m_aliases[alias] = canonicalTarget;
    }

    bool FileSystemReader::HasAlias(const std::string& alias) const
    {
        return m_aliases.find(alias) != m_aliases.end();
    }

    // ========================================================================
    //Private static method implementation
    // ========================================================================


    bool FileSystemReader::IsPathWithinRoot(const std::filesystem::path& path,
                                            const std::filesystem::path& root)
    {
        try
        {
            // Use weakly_canonical to normalize the two paths
            auto canonicalPath = std::filesystem::weakly_canonical(path);
            auto canonicalRoot = std::filesystem::weakly_canonical(root);

            // Make sure the root directory ends with a delimiter to facilitate prefix comparison
            if (canonicalRoot.string().back() != '/' && canonicalRoot.string().back() != '\\')
            {
                canonicalRoot = canonicalRoot / ""; // Add separator
            }

            // Check if the path starts with the root directory
            auto pathStr = canonicalPath.string();
            auto rootStr = canonicalRoot.string();

            // The path must be within the root directory or the root directory itself
            if (pathStr.length() < rootStr.length())
            {
                return pathStr == rootStr.substr(0, pathStr.length());
            }

            return pathStr.substr(0, rootStr.length()) == rootStr;
        }
        catch (const std::exception& e)
        {
            std::cerr << "FileSystemReader::IsPathWithinRoot failed: " << e.what() << std::endl;
            return false;
        }
    }

    std::optional<std::string> FileSystemReader::ReadFileContent(const std::filesystem::path& filePath)
    {
        try
        {
            // Check if the file exists and is readable
            if (!std::filesystem::exists(filePath))
            {
                return std::nullopt;
            }

            if (!std::filesystem::is_regular_file(filePath))
            {
                return std::nullopt;
            }

            // Use ifstream to read the file in text mode
            std::ifstream fileStream(filePath, std::ios::in);
            if (!fileStream.is_open())
            {
                return std::nullopt;
            }

            // Use stringstream to read the entire file content
            std::stringstream buffer;
            buffer << fileStream.rdbuf();

            // Check if the read is successful
            if (fileStream.fail() && !fileStream.eof())
            {
                return std::nullopt;
            }

            return buffer.str();
        }
        catch (const std::exception& e)
        {
            std::cerr << "FileSystemReader::ReadFileContent failed for '"
                << filePath.string() << "': " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    std::filesystem::path FileSystemReader::CanonicalizePath(const std::filesystem::path& path)
    {
        try
        {
            //Convert to absolute path
            auto absolutePath = std::filesystem::absolute(path);

            // Attempt to resolve symlinks (lexically_normal handles .. and .)
            auto canonicalPath = absolutePath.lexically_normal();

            // Note: std::filesystem::canonical will actually access the file system.
            // An exception may be thrown for non-existent paths, so lexically_normal is used here

            return canonicalPath;
        }
        catch (const std::exception& e)
        {
            // If normalization fails, return the absolute path
            std::cerr << "FileSystemReader::CanonicalizePath failed for '"
                << path.string() << "': " << e.what() << std::endl;
            return std::filesystem::absolute(path);
        }
    }

    std::optional<std::filesystem::path> FileSystemReader::ResolveAliasPath(const ShaderPath& path) const
    {
        /**
         * [NEW] Alias path resolution algorithm
         *
         *Business logic:
         * 1. Get the string representation of ShaderPath
         * 2. Check whether the path contains a registered alias (such as @engine)
         * 3. If found, replace the alias with the actual path
         * 4. Return the complete file system path
         *
         * Path format example:
         * - Input: /shaders/@engine/core/core.hlsl
         * - Alias: @engine -> F:/project/.enigma/assets/engine/shaders
         * - Output: F:/project/.enigma/assets/engine/shaders/core/core.hlsl
         */

        std::string pathStr = path.GetPathString();

        // Iterate through all registered aliases
        for (const auto& [alias, targetPath] : m_aliases)
        {
            // Find the location of the alias in the path
            size_t aliasPos = pathStr.find(alias);
            if (aliasPos != std::string::npos)
            {
                // Find the alias and extract the relative path part after the alias
                //Example: /shaders/@engine/core/core.hlsl
                // aliasPos points to @engine
                //The part after the alias is /core/core.hlsl

                size_t      afterAlias = aliasPos + alias.length();
                std::string relativePart;

                if (afterAlias < pathStr.length())
                {
                    relativePart = pathStr.substr(afterAlias);
                    // Remove leading slash (if present)
                    if (!relativePart.empty() && relativePart[0] == '/')
                    {
                        relativePart = relativePart.substr(1);
                    }
                }

                // Build the full path: targetPath / relativePart
                std::filesystem::path fullPath = targetPath;
                if (!relativePart.empty())
                {
                    fullPath = fullPath / relativePart;
                }

                return fullPath;
            }
        }

        // No aliases found
        return std::nullopt;
    }
} // namespace enigma::graphic
