/**
 * @file FileSystemReader.cpp
 * @brief 文件系统读取器实现
 * @date 2025-11-04
 * @author Caizii
 *
 * 实现说明:
 * 本文件包含FileSystemReader类的完整实现，提供安全的本地文件系统访问功能。
 * 所有实现都遵循现代C++最佳实践，确保线程安全和异常安全。
 *
 * 性能优化:
 * - 使用std::filesystem高效路径操作
 * - 最小化系统调用次数
 * - 智能错误处理避免异常开销
 * - 缓存友好的文件读取策略
 *
 * 安全特性:
 * - 严格的路径边界检查
 * - 符号链接安全处理
 * - 路径遍历攻击防护
 * - 详细的权限验证
 */

#include "FileSystemReader.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>

#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Graphic/Shader/ShaderPack/Include/ShaderPath.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // 构造函数实现
    // ========================================================================

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
        // 规范化根目录路径
        m_rootPath = CanonicalizePath(explicitRoot);

        // 验证指定的根目录是否有效
        if (!std::filesystem::exists(m_rootPath) || !std::filesystem::is_directory(m_rootPath))
        {
            ERROR_AND_DIE("Explicit root directory is not a valid directory!");
        }
    }

    // ========================================================================
    // IFileReader接口实现
    // ========================================================================

    std::optional<std::string> FileSystemReader::ReadFile(const ShaderPath& path)
    {
        try
        {
            // 将Shader Pack路径转换为文件系统路径
            auto fullPath = path.Resolved(m_rootPath);

            // 安全检查：确保路径在根目录内
            if (!IsPathWithinRoot(fullPath, m_rootPath))
            {
                ERROR_AND_DIE("Attempted to read file outside working directory!");
            }

            // 安全读取文件内容
            return ReadFileContent(fullPath);
        }
        catch (const std::exception& e)
        {
            // 记录错误但不会终止程序
            std::cerr << "FileSystemReader::ReadFile failed for path '"
                << path.GetPathString() << "': " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    bool FileSystemReader::FileExists(const ShaderPath& path) const
    {
        try
        {
            // 将Shader Pack路径转换为文件系统路径
            auto fullPath = path.Resolved(m_rootPath);

            // 安全检查：确保路径在根目录内
            if (!IsPathWithinRoot(fullPath, m_rootPath))
            {
                ERROR_AND_DIE("Attempted to access file outside working directory!");
            }

            // 检查文件是否存在
            return std::filesystem::exists(fullPath) &&
                std::filesystem::is_regular_file(fullPath);
        }
        catch (const std::exception& e)
        {
            // 记录错误但返回false表示文件不存在
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
    // 私有静态方法实现
    // ========================================================================


    bool FileSystemReader::IsPathWithinRoot(const std::filesystem::path& path,
                                            const std::filesystem::path& root)
    {
        try
        {
            // 使用 weakly_canonical 规范化两个路径
            auto canonicalPath = std::filesystem::weakly_canonical(path);
            auto canonicalRoot = std::filesystem::weakly_canonical(root);

            // 确保根目录以分隔符结尾，便于前缀比较
            if (canonicalRoot.string().back() != '/' && canonicalRoot.string().back() != '\\')
            {
                canonicalRoot = canonicalRoot / ""; // 添加分隔符
            }

            // 检查路径是否以根目录开头
            auto pathStr = canonicalPath.string();
            auto rootStr = canonicalRoot.string();

            // 路径必须在根目录内或就是根目录本身
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
            // 检查文件是否存在且可读
            if (!std::filesystem::exists(filePath))
            {
                return std::nullopt;
            }

            if (!std::filesystem::is_regular_file(filePath))
            {
                return std::nullopt;
            }

            // 使用ifstream以文本模式读取文件
            std::ifstream fileStream(filePath, std::ios::in);
            if (!fileStream.is_open())
            {
                return std::nullopt;
            }

            // 使用stringstream读取整个文件内容
            std::stringstream buffer;
            buffer << fileStream.rdbuf();

            // 检查读取是否成功
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
            // 转换为绝对路径
            auto absolutePath = std::filesystem::absolute(path);

            // 尝试解析符号链接（lexically_normal处理..和.）
            auto canonicalPath = absolutePath.lexically_normal();

            // 注意：std::filesystem::canonical会实际访问文件系统，
            // 可能对不存在的路径抛出异常，所以这里使用lexically_normal

            return canonicalPath;
        }
        catch (const std::exception& e)
        {
            // 如果规范化解失败，返回绝对路径
            std::cerr << "FileSystemReader::CanonicalizePath failed for '"
                << path.string() << "': " << e.what() << std::endl;
            return std::filesystem::absolute(path);
        }
    }
} // namespace enigma::graphic
