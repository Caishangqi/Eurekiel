#include "AbsolutePackPath.hpp"
#include <sstream>
#include <vector>
#include <stdexcept>

namespace enigma::graphic
{
    // ========================================================================
    // 私有构造函数 - 只能通过工厂方法创建
    // ========================================================================

    AbsolutePackPath::AbsolutePackPath(const std::string& normalizedPath)
        : m_path(normalizedPath)
    {
        // 教学要点: 私有构造函数确保所有路径都经过标准化验证
        // 不可变对象模式: m_path 在构造后永不改变
    }

    // ========================================================================
    // 工厂方法 - 创建并验证绝对路径
    // ========================================================================

    AbsolutePackPath AbsolutePackPath::FromAbsolutePath(const std::string& absolutePath)
    {
        // 教学要点: 工厂方法模式 - 集中验证和标准化逻辑
        if (absolutePath.empty() || absolutePath[0] != '/')
        {
            throw std::invalid_argument("Path must start with '/': " + absolutePath);
        }

        // 标准化路径（处理 . 和 ..）
        std::string normalized = NormalizeAbsolutePath(absolutePath);

        // 使用私有构造函数创建实例
        return AbsolutePackPath(normalized);
    }

    // ========================================================================
    // 路径标准化算法 - 核心逻辑
    // ========================================================================

    std::string AbsolutePackPath::NormalizeAbsolutePath(const std::string& path)
    {
        /**
         * 教学要点: 路径标准化算法
         *
         * 步骤:
         * 1. 按 `/` 分割路径为段
         * 2. 过滤空段和 `.` 段
         * 3. 处理 `..` 段（回退到上级目录）
         * 4. 重建标准化路径
         *
         * 示例:
         * - `/shaders/./lib/../common.hlsl` → `/shaders/common.hlsl`
         * - `/shaders//lib/common.hlsl` → `/shaders/lib/common.hlsl`
         * - `/` → `/`
         */

        if (path.empty() || path[0] != '/')
        {
            throw std::invalid_argument("Not an absolute path: " + path);
        }

        // ========================================================================
        // Step 1: 分割路径为段
        // ========================================================================

        std::vector<std::string> segments;
        std::string              currentSegment;

        for (char c : path)
        {
            if (c == '/')
            {
                if (!currentSegment.empty())
                {
                    segments.push_back(currentSegment);
                    currentSegment.clear();
                }
                // 忽略空段（连续的 `/`）
            }
            else
            {
                currentSegment += c;
            }
        }

        // 处理最后一个段（如果路径不以 `/` 结尾）
        if (!currentSegment.empty())
        {
            segments.push_back(currentSegment);
        }

        // ========================================================================
        // Step 2: 标准化段（处理 . 和 ..）
        // ========================================================================

        std::vector<std::string> normalizedSegments;

        for (const auto& segment : segments)
        {
            if (segment == "." || segment.empty())
            {
                // 忽略当前目录符号和空段
                continue;
            }
            else if (segment == "..")
            {
                // 回退到上级目录
                if (!normalizedSegments.empty())
                {
                    normalizedSegments.pop_back();
                }
                // 如果已经在根目录，忽略 ..
            }
            else
            {
                // 正常段，添加到结果
                normalizedSegments.push_back(segment);
            }
        }

        // ========================================================================
        // Step 3: 重建标准化路径
        // ========================================================================

        if (normalizedSegments.empty())
        {
            // 特殊情况: 根路径
            return "/";
        }

        std::ostringstream oss;
        for (const auto& segment : normalizedSegments)
        {
            oss << "/" << segment;
        }

        return oss.str();
    }

    // ========================================================================
    // 获取父路径
    // ========================================================================

    std::optional<AbsolutePackPath> AbsolutePackPath::Parent() const
    {
        /**
         * 教学要点: std::optional 应用
         *
         * 业务逻辑:
         * - `/shaders/lib/common.hlsl` → `/shaders/lib`
         * - `/shaders` → `/`
         * - `/` → std::nullopt（根路径无父路径）
         */

        if (m_path == "/")
        {
            // 根路径无父路径
            return std::nullopt;
        }

        // 查找最后一个 `/` 的位置
        size_t lastSlash = m_path.find_last_of('/');

        if (lastSlash == 0)
        {
            // 父路径是根目录
            return AbsolutePackPath("/");
        }

        // 截取到最后一个 `/` 之前的部分
        std::string parentPath = m_path.substr(0, lastSlash);
        return AbsolutePackPath(parentPath);
    }

    // ========================================================================
    // 解析相对路径
    // ========================================================================

    AbsolutePackPath AbsolutePackPath::Resolve(const std::string& relativePath) const
    {
        /**
         * 教学要点: 相对路径解析算法
         *
         * 业务逻辑:
         * 1. 如果 relativePath 以 `/` 开头，直接返回新的绝对路径
         * 2. 否则，基于当前路径解析相对路径
         *
         * 示例:
         * - 当前路径: `/shaders/gbuffers_terrain.hlsl`
         * - 解析 `../lib/common.hlsl`
         * - 步骤:
         *   a) 获取父路径: `/shaders`
         *   b) 拼接: `/shaders/lib/common.hlsl`
         *   c) 标准化: `/shaders/lib/common.hlsl`
         */

        if (relativePath.empty())
        {
            throw std::invalid_argument("Relative path cannot be empty");
        }

        // 如果是绝对路径，直接创建新路径
        if (relativePath[0] == '/')
        {
            return FromAbsolutePath(relativePath);
        }

        // 相对路径解析: 基于父路径
        auto        parentOpt = Parent();
        std::string basePath  = parentOpt.has_value() ? parentOpt->GetPathString() : "/";

        // 拼接路径
        std::string combinedPath = basePath + "/" + relativePath;

        // 标准化并返回
        return FromAbsolutePath(combinedPath);
    }

    // ========================================================================
    // 转换为文件系统路径
    // ========================================================================

    std::filesystem::path AbsolutePackPath::Resolved(const std::filesystem::path& root) const
    {
        /**
         * 教学要点: Shader Pack 路径到文件系统路径的转换
         *
         * 业务逻辑:
         * - Shader Pack 路径: `/shaders/gbuffers_terrain.hlsl`
         * - 根目录: `F:/shaderpacks/ComplementaryReimagined/`
         * - 结果: `F:/shaderpacks/ComplementaryReimagined/shaders/gbuffers_terrain.hlsl`
         *
         * 实现细节:
         * - 移除 Shader Pack 路径的前导 `/`
         * - 使用 std::filesystem::path 的 `/` 运算符拼接
         */

        if (m_path == "/")
        {
            // 根路径直接返回 root
            return root;
        }

        // 移除前导 `/` 并转换为文件系统路径
        std::string relativePart = m_path.substr(1); // 跳过前导 `/`
        return root / relativePart;
    }
} // namespace enigma::graphic
