/**
 * @file FileSystemReader.hpp
 * @brief 文件系统读取器 - IFileReader接口的本地文件系统实现
 * @date 2025-11-04
 * @author Caizii
 *
 * 架构说明:
 * FileSystemReader 是 IFileReader 接口的本地文件系统实现，提供安全的文件读取功能。
 * 这是Shader Include系统的核心文件访问组件，使用当前工作目录作为根目录和安全检查功能。
 *
 * 设计原则:
 * - 安全第一: 所有文件访问都限制在根目录内
 * - 工作目录根: 使用当前工作目录作为根目录
 * - 异常安全: 使用RAII和现代C++最佳实践
 * - 扩展性: 支持手动指定根目录的构造函数重载
 *
 * 职责边界:
 * - + 本地文件读取 (ReadFile)
 * - + 文件存在性检查 (FileExists)
 * - + 根路径管理 (GetRootPath)
 * - + 路径安全验证 (IsPathWithinRoot)
 * - ❌ 不包含缓存逻辑 (可由装饰器类添加)
 * - ❌ 不包含网络文件访问 (可由其他实现类提供)
 *
 * 安全特性:
 * - 路径遍历攻击防护
 * - 工作目录边界检查
 * - 符号链接安全处理
 * - 权限验证
 *
 * 使用示例:
 * @code
 * // 使用当前工作目录作为根目录
 * auto reader1 = std::make_unique<FileSystemReader>();
 * 
 * // 手动指定根目录
 * auto reader2 = std::make_unique<FileSystemReader>(explicitRootPath);
 * 
 * // 读取文件
 * auto shaderPath = ShaderPath::FromAbsolutePath("/shaders/common.hlsl");
 * auto content = reader2->ReadFile(shaderPath);
 * if (content) {
 *     std::cout << "文件内容: " << *content << std::endl;
 * }
 * 
 * // 检查文件存在性
 * if (reader2->FileExists(shaderPath)) {
 *     std::cout << "文件存在" << std::endl;
 * }
 * @endcode
 *
 * 教学要点:
 * - RAII资源管理
 * - 路径安全编程
 * - 现代C++文件系统操作
 * - 异常安全设计
 * - 策略模式实现
 */

#pragma once

#include "IFileReader.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace enigma::graphic
{
    class ShaderPath;
}

namespace enigma::graphic
{
    /**
         * @class FileSystemReader
         * @brief local file system reader implementation
         *
         * Core features:
         * - Use the current working directory as the root directory
         * - Strict path security checks
         * - Support manually specifying the root directory
         * - Efficient file IO operations
         * - [NEW] Support path aliases (e.g. @engine -> .enigma/assets/engine/shaders)
         *
         * Security mechanism:
         * - All file access is restricted to the root directory
         * - Protect against path traversal attacks (../etc/passwd)
         * - Symbolic link security verification
         * - Detailed error handling and logging
         *
         *Performance optimization:
         * - Use std::filesystem for efficient path manipulation
         * - File stream buffer optimization
         * - Minimize system calls
         * - Intelligent error handling
         */
    class FileSystemReader : public IFileReader
    {
    public:
        /**
                 * @brief default constructor - uses the current working directory as the root directory
                 *
                 * Workflow:
                 * 1. Get the current working directory std::filesystem::current_path()
                 * 2. Set the current directory as the root directory
                 * 3. Verify the validity of the root directory
                 *
                 * Example:
                 * @code
                 * // Use the current working directory as the root directory
                 * auto reader = std::make_unique<FileSystemReader>();
                 * @endcode
                 */
        FileSystemReader();

        /**
         * @brief constructor overloading - manually specify the root directory
         * @param explicitRoot The explicitly specified root directory path
         *
         * Usage scenarios:
         * - a known and unambiguous project root directory
         * - Specify a special root directory in the test environment
         * - Requires precise control of root directory location
         *
         * Example:
         * @code
         * FileSystemReader reader("F:/project/");
         * @endcode
         *
         * @throws std::runtime_error if the root directory is invalid
         */
        explicit FileSystemReader(const std::filesystem::path& explicitRoot);

        /**
         * @brief destructor
         *
         * Design description:
         * - use default destructor
         * - RAII automatic resource management
         * - Modern C++ recommended practices
         */
        ~FileSystemReader() override = default;

        /**
         * @brief Read file content
         * @param path Shader Pack path (Unix style absolute path)
         * @return std::optional<std::string> Successfully returns the file content, failure returns nullopt
         *
         * Security check:
         * - Verify that the path is within the root directory
         * - Check if the file exists and is readable
         * - Prevent path traversal attacks
         *
         * Reading strategy:
         * - Use std::ifstream for text reading
         * - Automatically handle newline conversion
         * - Optimize buffer size
         * - Detailed error log
         *
         * @throws std::invalid_argument if the path is outside the root directory
         */
        std::optional<std::string> ReadFile(const ShaderPath& path) override;
        /**
                 * @brief Check if the file exists
                 * @param path Shader Pack path (Unix style absolute path)
                 * @return bool Returns true if the file exists, otherwise returns false
                 *
                 * Security features:
                 * - Path security verification
                 * - Safe handling of symbolic links
                 * - permission check
                 *
                 *Performance optimization:
                 * - Quick check using std::filesystem::exists
                 * - Avoid unnecessary file opening operations
                 * - Cache friendly design
                 *
                 * @throws std::invalid_argument if the path is outside the root directory
                 */
        bool FileExists(const ShaderPath& path) const override;

        /**
         * @brief Get the root path
         * @return std::filesystem::path root path
         *
         * Return value description:
         * - Returns the normalized absolute path
         * - the path ends with a separator (facilitates path splicing)
         * - platform dependent path format
         */
        std::filesystem::path GetRootPath() const override;

        /**
         * @brief [NEW] Add path alias
         * @param alias alias name (such as "@engine")
         * @param targetPath The actual path pointed by the alias (absolute path)
         *
         * Usage scenarios:
         * - @engine -> .enigma/assets/engine/shaders (engine built-in shader)
         * - @bundle -> the shaders directory of the current ShaderBundle
         *
         * Example:
         * @code
         * reader->AddAlias("@engine", "F:/project/.enigma/assets/engine/shaders");
         * // Later #include "@engine/core/core.hlsl" will resolve to the correct path
         * @endcode
         */
        void AddAlias(const std::string& alias, const std::filesystem::path& targetPath);

        /**
         * @brief [NEW] Check whether there is an alias specified
         * @param alias alias name
         * @return bool returns true if it exists
         */
        bool HasAlias(const std::string& alias) const;

    private:
        /**
                 * @brief root directory path
                 *
                 * Immutability:
                 * - No modification after construction
                 * - always an absolute path
                 * - Defaults to the current working directory
                 */
        std::filesystem::path m_rootPath;

        /**
         * @brief [NEW] Path alias mapping table
         *
         * Purpose:
         * - Map virtual aliases (such as @engine) to real file system paths
         * - Supports cross-directory #include references
         *
         * Example:
         * - "@engine" -> "F:/project/.enigma/assets/engine/shaders"
         */
        std::unordered_map<std::string, std::filesystem::path> m_aliases;


        /**
         * @brief Check whether the path is in the root directory
         * @param path The path to check
         * @param root root directory path
         * @return bool Returns true in the root directory, otherwise returns false
         *
         * Security check:
         * - Canonical paths (use weakly_canonical handling of .. and .)
         * - Check path prefix
         * - Symbolic link security verification
         * - Prevent path traversal attacks
         *
         * Check strategy:
         * 1. Convert both paths to absolute and canonical forms (use weakly_canonical)
         * 2. Check whether path starts with root
         * 3. Make sure not to cross the root boundary
         * 4. Handling the security risks of symbolic links
         *
         * @throws std::invalid_argument if the path is not in the root directory
         */
        static bool IsPathWithinRoot(const std::filesystem::path& path,
                                     const std::filesystem::path& root);

        /**
         * @brief Safely read file content
         * @param filePath The file path to be read (verified to be in the root directory)
         * @return std::optional<std::string> file content or nullopt
         *
         * Read details:
         * - Open file in text mode
         * - Automatically handle newline conversion
         * - Efficient reading using stringstream
         * - Memory optimization for handling large files
         *
         * Error handling:
         * - file does not exist → nullopt
         * - Insufficient permissions → nullopt
         * - encoding error → nullopt
         * - IO error → nullopt
         *
         *Performance optimization:
         * - preallocated string buffer
         * - Use move semantics to reduce copying
         * - Exception handling minimizes overhead
         */
        static std::optional<std::string> ReadFileContent(const std::filesystem::path& filePath);

        /**
         * @brief normalizes paths and handles symbolic links
         * @param path original path
         * @return the normalized absolute path
         *
         * Processing steps:
         * 1. Convert to absolute path
         * 2. Resolve all symbolic links
         * 3. Normalized path separator
         * 4. Remove redundant . and ..
         *
         *Safety considerations:
         * - Prevent symbolic link attacks
         * - handle circular links
         * - Permission verification
         */
        static std::filesystem::path CanonicalizePath(const std::filesystem::path& path);

        /**
         * @brief [NEW] Parse paths with aliases
         * @param path ShaderPath (may contain aliases such as @engine)
         * @return std::optional<std::filesystem::path> Returns the actual path if the parsing is successful, otherwise returns nullopt
         *
         * Parsing logic:
         * 1. Check whether the path contains a registered alias (such as @engine)
         * 2. If included, replace the alias with the actual path
         * 3. Return the complete file system path
         *
         * Example:
         * - Input: /shaders/@engine/core/core.hlsl
         * - Alias: @engine -> F:/project/.enigma/assets/engine/shaders
         * - Output: F:/project/.enigma/assets/engine/shaders/core/core.hlsl
         */
        std::optional<std::filesystem::path> ResolveAliasPath(const ShaderPath& path) const;
    };
} // namespace enigma::graphic
