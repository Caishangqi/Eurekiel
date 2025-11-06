/**
 * @file ShaderIncludeHelper.cpp
 * @brief Shader Include系统辅助工具类实现
 * @date 2025-11-04
 * @author Caizii
 *
 * 实现说明:
 * ShaderIncludeHelper.cpp 实现了Shader Include系统的便捷工具函数。
 * 这些函数封装了路径处理、Include图构建和Include展开的常用操作，
 * 简化了Include系统的使用复杂度。
 *
 * 实现策略:
 * - 使用现有组件的组合而非重新实现
 * - 提供便捷的工厂方法和封装接口
 * - 重点关注错误处理和边界情况
 * - 保持与现有Include系统的兼容性
 */

#include "Engine/Graphic/Shader/Common/ShaderIncludeHelper.hpp"
#include "Engine/Graphic/Shader/ShaderPack/Include/IncludeProcessor.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace enigma::graphic
{
    // ========================================================================
    // 路径处理函数组实现
    // ========================================================================

    std::filesystem::path ShaderIncludeHelper::DetermineRootPath(const std::filesystem::path& anyPath)
    {
        // 教学要点：路径推断算法 - 从文件路径向上查找合理的根目录

        // 1. 检查路径是否存在
        if (!std::filesystem::exists(anyPath))
        {
            // 路径不存在，返回父目录（如果有的话）
            return anyPath.parent_path();
        }

        std::filesystem::path currentPath = anyPath;

        // 2. 如果是文件路径，使用其父目录
        if (std::filesystem::is_regular_file(currentPath))
        {
            currentPath = currentPath.parent_path();
        }

        // 3. 向上查找常见的项目根目录标识
        while (!currentPath.empty() && currentPath != currentPath.root_path())
        {
            // 检查是否存在常见的项目根目录标识
            bool foundRootMarker = false;

            for (const auto& entry : std::filesystem::directory_iterator(currentPath))
            {
                if (!entry.is_directory()) continue;

                const std::string dirName = entry.path().filename().string();

                // 常见的Shader Pack根目录标识
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

            // 继续向上查找
            currentPath = currentPath.parent_path();
        }

        // 4. 如果没有找到明确的根目录标识，返回原始路径的父目录
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
        // 教学要点：路径安全检查 - 防止路径遍历攻击

        try
        {
            // 1. 将两个路径都转换为绝对和规范形式
            std::filesystem::path absolutePath = std::filesystem::absolute(path);
            std::filesystem::path absoluteRoot = std::filesystem::absolute(root);

            // 2. 规范化路径（解析..和.）
            std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(absolutePath);
            std::filesystem::path canonicalRoot = std::filesystem::weakly_canonical(absoluteRoot);

            // 3. 确保根路径以分隔符结尾（便于前缀比较）
            if (!canonicalRoot.string().empty() && canonicalRoot.string().back() != '/' &&
                canonicalRoot.string().back() != '\\')
            {
                canonicalRoot = canonicalRoot / "";
            }

            // 4. 检查path是否以root开头
            std::string pathStr = canonicalPath.string();
            std::string rootStr = canonicalRoot.string();

            return pathStr.find(rootStr) == 0;
        }
        catch (const std::filesystem::filesystem_error&)
        {
            // 文件系统错误（如权限问题、路径不存在等）
            return false;
        }
    }

    std::string ShaderIncludeHelper::NormalizePath(const std::string& path)
    {
        // 教学要点：路径标准化 - 统一路径分隔符和处理相对路径

        if (path.empty())
        {
            return path;
        }

        std::string result;
        result.reserve(path.length());

        std::vector<std::string> components;
        std::string              current;

        // 1. 解析路径组件并处理相对路径
        for (char c : path)
        {
            if (c == '/' || c == '\\')
            {
                if (!current.empty())
                {
                    // 处理路径组件
                    if (current == ".")
                    {
                        // 忽略当前目录
                    }
                    else if (current == "..")
                    {
                        // 回到上级目录
                        if (!components.empty())
                        {
                            components.pop_back();
                        }
                    }
                    else
                    {
                        // 普通目录名
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

        // 处理最后一个组件
        if (!current.empty())
        {
            if (current == ".")
            {
                // 忽略当前目录
            }
            else if (current == "..")
            {
                // 回到上级目录
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

        // 2. 重新构建标准化路径（使用/作为分隔符）
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
        // 教学要点：相对路径解析 - 基于基础路径解析相对路径

        try
        {
            // 直接使用ShaderPath的Resolve方法
            return basePath.Resolve(relativePath);
        }
        catch (const std::exception&)
        {
            // 如果解析失败，返回基础路径（或抛出异常）
            // 这里选择重新抛出异常，让调用者处理
            throw;
        }
    }

    // ========================================================================
    // Include图构建便捷接口实现
    // ========================================================================

    std::unique_ptr<IncludeGraph> ShaderIncludeHelper::BuildFromFiles(
        const std::filesystem::path&    anyPath,
        const std::vector<std::string>& relativeShaderPaths)
    {
        // 教学要点：便捷接口封装 - 简化从文件系统构建Include图的流程

        try
        {
            // 1. 推断Shader Pack根目录
            std::filesystem::path packRoot = DetermineRootPath(anyPath);

            // 2. 将相对路径转换为ShaderPath对象
            std::vector<ShaderPath> shaderPaths;
            shaderPaths.reserve(relativeShaderPaths.size());

            for (const auto& relativePath : relativeShaderPaths)
            {
                // 确保相对路径以/开头（ShaderPath要求绝对路径）
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
                    // 路径格式错误，跳过这个文件
                    continue;
                }
            }

            // 3. 使用BuildFromShaderPack构建Include图
            return BuildFromShaderPack(packRoot, shaderPaths);
        }
        catch (const std::exception&)
        {
            // 构建失败，返回nullptr
            return nullptr;
        }
    }

    std::unique_ptr<IncludeGraph> ShaderIncludeHelper::BuildFromShaderPack(
        const std::filesystem::path&   packRoot,
        const std::vector<ShaderPath>& shaderPaths)
    {
        // 教学要点：精确控制构建 - 明确指定Shader Pack根目录

        try
        {
            // 1. 创建ShaderPackReader
            auto fileReader = std::make_shared<ShaderPackReader>(packRoot);

            // 2. 构建IncludeGraph
            auto graph = std::make_unique<IncludeGraph>(fileReader, shaderPaths);

            // 3. 返回构建的Include图
            return graph;
        }
        catch (const std::exception&)
        {
            // 构建失败（可能是循环依赖或其他错误），返回nullptr
            return nullptr;
        }
    }

    // ========================================================================
    // Include展开便捷接口实现
    // ========================================================================

    std::string ShaderIncludeHelper::ExpandShaderSource(
        const IncludeGraph& graph,
        const ShaderPath&   shaderPath,
        bool                withLineDirectives)
    {
        // 教学要点：Include展开封装 - 简化IncludeProcessor的使用

        // 1. 验证shaderPath在IncludeGraph中存在
        if (!graph.HasNode(shaderPath))
        {
            throw std::invalid_argument(
                "Shader path not found in IncludeGraph: " + shaderPath.GetPathString()
            );
        }

        // 2. 根据withLineDirectives选择展开方式
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
