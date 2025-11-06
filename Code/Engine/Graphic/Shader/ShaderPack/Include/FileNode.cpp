#include "FileNode.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace enigma::graphic
{
    // ========================================================================
    // 辅助函数 - 字符串处理
    // ========================================================================

    namespace
    {
        /**
         * @brief Remove leading and trailing whitespace from string
         * @param str Original string
         * @return String with whitespace removed
         *
         * BUGFIX: Use lambda with static_cast to prevent assertion failure
         *         when processing UTF-8 Chinese characters in shader files.
         *         std::isspace() requires char value in range [-1, 255],
         *         but UTF-8 multi-byte characters exceed this range.
         */
        std::string Trim(const std::string& str)
        {
            auto start = std::find_if_not(str.begin(), str.end(),
                                          [](unsigned char c) { return std::isspace(c); });
            auto end = std::find_if_not(str.rbegin(), str.rend(),
                                        [](unsigned char c) { return std::isspace(c); }).base();
            return (start < end) ? std::string(start, end) : std::string();
        }

        /**
         * @brief 检查字符串是否以指定前缀开头
         * @param str 原始字符串
         * @param prefix 前缀
         * @return true 如果以指定前缀开头
         */
        bool StartsWith(const std::string& str, const std::string& prefix)
        {
            return str.size() >= prefix.size() &&
                str.compare(0, prefix.size(), prefix) == 0;
        }
    }

    // ========================================================================
    // 构造函数实现
    // ========================================================================

    FileNode::FileNode(
        const ShaderPath&                          path,
        const std::vector<std::string>&            lines,
        const std::unordered_map<int, ShaderPath>& includes
    )
        : m_path(path)
          , m_lines(lines)
          , m_includes(includes)
    {
        // 教学要点: 私有构造函数，只能通过工厂方法创建
        // 不可变对象：所有成员在构造后不可修改
    }

    // ========================================================================
    // 工厂方法实现
    // ========================================================================

    FileNode FileNode::FromLines(
        const ShaderPath&               path,
        const std::vector<std::string>& lines
    )
    {
        /**
         * 教学要点: 工厂方法模式
         *
         * 业务逻辑:
         * 1. 获取文件的父目录（用于解析相对路径）
         * 2. 调用 FindIncludes() 解析所有 #include 指令
         * 3. 创建 FileNode 实例
         */

        // Step 1: 获取父目录
        auto parentOpt = path.Parent();
        if (!parentOpt.has_value())
        {
            throw std::invalid_argument(
                "Not a valid shader file name (must have parent directory): " + path.GetPathString()
            );
        }

        ShaderPath currentDirectory = parentOpt.value();

        // Step 2: 解析 Include 指令
        auto includes = FindIncludes(currentDirectory, lines);

        // Step 3: 创建实例
        return FileNode(path, lines, includes);
    }

    // ========================================================================
    // Include 解析实现
    // ========================================================================

    std::unordered_map<int, ShaderPath> FileNode::FindIncludes(
        const ShaderPath&               currentDirectory,
        const std::vector<std::string>& lines
    )
    {
        /**
         * 教学要点: #include 指令解析算法
         *
         * 业务逻辑:
         * 1. 遍历每一行
         * 2. 检查是否以 "#include" 开头（忽略前导空白）
         * 3. 提取 "#include" 后的路径部分
         * 4. 移除引号（如果存在）
         * 5. 基于当前目录解析相对路径
         *
         * 支持的格式:
         * - `#include "Common.hlsl"`
         * - `  #include   "../lib/Lighting.hlsl"  `
         * - `#include Common.hlsl` (容错，缺少引号)
         */

        std::unordered_map<int, ShaderPath> foundIncludes;

        for (int i = 0; i < static_cast<int>(lines.size()); ++i)
        {
            // Step 1: Trim 前导和尾随空白
            std::string line = Trim(lines[i]);

            // Step 2: 检查是否是 #include 指令
            if (!StartsWith(line, "#include"))
            {
                continue;
            }

            // Step 3: 移除 "#include " 部分，保留路径
            std::string target = line.substr(8); // "#include" = 8 字符
            target             = Trim(target); // 移除前导/尾随空白

            if (target.empty())
            {
                // 空路径，忽略（可能是错误的 #include 指令）
                continue;
            }

            // Step 4: 移除引号（如果存在）
            // 支持：
            // - `"Common.hlsl"` → `Common.hlsl`
            // - `Common.hlsl` → `Common.hlsl` (容错)
            // - `"test.hlsl` → `test.hlsl` (容错，缺少结束引号)

            if (!target.empty() && target[0] == '"')
            {
                target = target.substr(1); // 移除前导引号
            }

            if (!target.empty() && target[target.size() - 1] == '"')
            {
                target = target.substr(0, target.size() - 1); // 移除尾随引号
            }

            if (target.empty())
            {
                // 引号内为空，忽略
                continue;
            }

            // Step 5: 基于当前目录解析相对路径
            try
            {
                ShaderPath resolved = currentDirectory.Resolve(target);
                foundIncludes.insert({i, resolved}); // 使用 insert() 避免默认构造函数需求
            }
            catch (const std::exception&)
            {
                // 路径解析失败（例如：无效的相对路径）
                // 忽略该 Include，继续处理其他行
                // 注意: IncludeGraph 构建时会检测并报告缺失文件
            }
        }

        return foundIncludes;
    }
} // namespace enigma::graphic
