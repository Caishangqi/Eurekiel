/**
 * @file IFileReader.hpp
 * @brief File Reader Abstract Interface - Provides file reading abstraction for Shader Include system
 * @date 2025-11-04
 * @author Caizii
 *
 * [ARCHITECTURE]
 * IFileReader provides an abstract interface for file reading operations.
 * This is the core abstraction layer for the Include system refactoring.
 *
 * [DESIGN PRINCIPLES]
 * - Pure Abstract Interface: Defines contract only, no concrete implementation
 * - Forward Declaration: Avoids circular dependency with ShaderPath class
 * - Exception Safety: Uses std::optional for potentially failing operations
 * - Extensibility: Supports various file reading implementations
 *
 * [RESPONSIBILITIES]
 * [ADD] File reading abstraction (ReadFile)
 * [ADD] File existence check (FileExists)
 * [ADD] Root path retrieval (GetRootPath)
 * [DELETE] No concrete file IO implementation (provided by subclasses)
 * [DELETE] No caching logic (can be added by concrete implementations)
 */

#pragma once

#include <optional>
#include <string>
#include <filesystem>

namespace enigma::graphic
{
    // Forward declaration to avoid circular dependency
    class ShaderPath;

    /**
     * @interface IFileReader
     * @brief File reading abstract interface
     *
     * Defines a common contract for file reading operations.
     * Supports multiple file reading strategies through polymorphism.
     *
     * Interface Design:
     * - Minimal interface: Only essential methods
     * - Clear semantics: Each method has a distinct responsibility
     * - Exception safety: Uses optional instead of exceptions for failure handling
     *
     * Implementations:
     * - VirtualPathReader: Virtual path file system reading
     * - [FUTURE] MemoryFileReader: In-memory cache reading
     * - [FUTURE] CompressedFileReader: Compressed file reading
     */
    class IFileReader
    {
    public:
        /**
         * @brief Virtual destructor
         *
         * Ensures proper destruction when deleting derived objects through base pointer.
         */
        virtual ~IFileReader() = default;

        /**
         * @brief Read file content
         * @param path Virtual shader path (Unix-style absolute path)
         * @return std::optional<std::string> File content on success, nullopt on failure
         *
         * Error handling:
         * - File not found -> nullopt
         * - Permission denied -> nullopt
         * - Read error -> nullopt
         */
        virtual std::optional<std::string> ReadFile(const ShaderPath& path) = 0;

        /**
         * @brief Check if file exists
         * @param path Virtual shader path (Unix-style absolute path)
         * @return bool True if file exists, false otherwise
         *
         * Used for include dependency graph construction and pre-validation.
         */
        virtual bool FileExists(const ShaderPath& path) const = 0;

        /**
         * @brief Get file reader root path
         * @return std::filesystem::path Root path (filesystem path)
         *
         * Returns the base path for virtual path resolution.
         * Used with ShaderPath::Resolved() for full path construction.
         */
        virtual std::filesystem::path GetRootPath() const = 0;

    protected:
        /**
         * @brief Protected default constructor
         *
         * Interface classes should not be instantiated directly.
         * Protected constructor ensures objects are created only through derived classes.
         */
        IFileReader() = default;

        /**
         * @brief Protected copy constructor
         * @param other Object to copy
         */
        IFileReader(const IFileReader& other) = default;

        /**
         * @brief Protected assignment operator
         * @param other Object to assign
         * @return *this
         */
        IFileReader& operator=(const IFileReader& other) = default;
    };
} // namespace enigma::graphic
