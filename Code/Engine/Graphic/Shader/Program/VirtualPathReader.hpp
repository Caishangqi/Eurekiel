/**
 * @file VirtualPathReader.hpp
 * @brief Virtual Path File Reader - Reads shader files from virtual path system
 * @date 2025-11-04
 * @author Caizii
 *
 * [ARCHITECTURE]
 * VirtualPathReader implements IFileReader interface, providing file reading capability
 * for the Shader Include system using virtual paths.
 *
 * [DESIGN PRINCIPLES]
 * - Single Responsibility: Focus on file reading from virtual paths
 * - Dependency Inversion: Implements IFileReader abstract interface
 * - Path Conversion: Uses ShaderPath::Resolved() for path translation
 * - Exception Safety: Uses std::optional for failure handling
 *
 * [RESPONSIBILITIES]
 * [ADD] File reading (ReadFile)
 * [ADD] File existence check (FileExists)
 * [ADD] Root path management (GetRootPath)
 * [ADD] Virtual path to filesystem path conversion
 * [DELETE] No file caching logic (can be added by upper layer)
 * [DELETE] No include dependency resolution (handled by IncludeGraph)
 */

#pragma once

#include "Engine/Graphic/Shader/Common/IFileReader.hpp"
#include <filesystem>
#include <string>
#include <optional>

namespace enigma::graphic
{
    /**
     * @class VirtualPathReader
     * @brief Virtual path file reader implementation
     *
     * Implements IFileReader interface for reading files from virtual path system.
     * Converts ShaderPath virtual paths to filesystem paths and reads file content.
     *
     * Thread Safety:
     * - Read operations are thread-safe (read-only root path)
     * - Safe to concurrently read different files
     * - Recommend creating separate VirtualPathReader instance per thread
     */
    class VirtualPathReader : public IFileReader
    {
    public:
        /**
         * @brief Construct VirtualPathReader
         * @param rootPath Root directory (filesystem path)
         *
         * Stores the root directory for virtual path resolution.
         * Does not validate root path validity (deferred to first use).
         */
        explicit VirtualPathReader(const std::filesystem::path& rootPath);

        /**
         * @brief Virtual destructor
         */
        ~VirtualPathReader() override = default;

        // ========================================================================
        // IFileReader Interface Implementation
        // ========================================================================

        /**
         * @brief Read file from virtual path
         * @param path Virtual shader path (Unix-style absolute path)
         * @return std::optional<std::string> File content on success, nullopt on failure
         *
         * Implementation:
         * 1. Convert virtual path to filesystem path using ShaderPath::Resolved()
         * 2. Check if file exists
         * 3. Open and read file content
         * 4. Return content or nullopt on failure
         *
         * Failure cases: file not found, permission denied, read error
         */
        std::optional<std::string> ReadFile(const ShaderPath& path) override;

        /**
         * @brief Check if file exists at virtual path
         * @param path Virtual shader path (Unix-style absolute path)
         * @return bool True if file exists, false otherwise
         *
         * Uses ShaderPath::Resolved() for path conversion and
         * std::filesystem::exists() for existence check.
         */
        bool FileExists(const ShaderPath& path) const override;

        /**
         * @brief Get root directory
         * @return std::filesystem::path Root path
         */
        std::filesystem::path GetRootPath() const override;

    private:
        /**
         * @brief Root directory (filesystem path)
         *
         * Used to convert virtual paths to actual file paths.
         * Immutable after construction.
         */
        std::filesystem::path m_rootPath;
    };
} // namespace enigma::graphic
