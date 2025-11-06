#pragma once

#include "ShaderPath.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace enigma::graphic
{
    /**
     * @class FileNode
     * @brief Include 依赖图中的文件节点（存储文件内容 + #include 依赖）
     *
     * **设计原则**：
     * - 不可变对象（线程安全）
     * - 工厂方法模式（私有构造函数 + 公有静态工厂）
     * - 自动解析 #include 指令
     *
     * **对应 Iris 源码**：
     * - Iris: FileNode.java
     * - 职责：表示 Shader Pack 中的一个文件及其 Include 依赖
     *
     * **使用场景**：
     * - IncludeGraph 构建依赖图时的节点
     * - 存储文件内容（按行）和 Include 映射（行号 → 目标路径）
     * - 支持 BFS/DFS 遍历依赖树
     *
     * **核心数据**：
     * - m_path: 文件的绝对路径（Shader Pack 内部路径）
     * - m_lines: 文件内容（按行存储）
     * - m_includes: Include 映射（行号 → 目标文件的绝对路径）
     *
     * **示例**：
     * ```cpp
     * // 文件内容:
     * // 0: #include "Common.hlsl"
     * // 1: void main() {
     * // 2:     #include "../lib/Lighting.hlsl"
     * // 3: }
     *
     * auto node = FileNode::FromLines(
     *     ShaderPath::FromAbsolutePath("/shaders/gbuffers_terrain.vs.hlsl"),
     *     lines
     * );
     *
     * // m_includes = {
     * //     0 → /shaders/Common.hlsl
     * //     2 → /shaders/lib/Lighting.hlsl
     * // }
     * ```
     */
    class FileNode
    {
    public:
        /**
         * @brief 工厂方法 - 从文件路径和内容创建 FileNode
         * @param path 文件的绝对路径（Shader Pack 内部路径）
         * @param lines 文件内容（按行存储）
         * @return 自动解析 #include 后的 FileNode
         *
         * 教学要点：
         * - 工厂方法模式：集中验证和初始化逻辑
         * - 自动调用 FindIncludes() 解析 #include 指令
         * - 不可变对象：创建后内容不可修改
         *
         * 异常：
         * - std::invalid_argument: 如果 path 不是有效的文件路径（必须有父目录）
         */
        static FileNode FromLines(
            const ShaderPath&               path,
            const std::vector<std::string>& lines
        );

        // ========================================================================
        // 访问器 - 获取节点数据
        // ========================================================================

        /**
         * @brief 获取文件路径
         * @return Shader Pack 内部绝对路径
         */
        const ShaderPath& GetPath() const { return m_path; }

        /**
         * @brief 获取文件内容（按行）
         * @return 文件内容的只读引用
         */
        const std::vector<std::string>& GetLines() const { return m_lines; }

        /**
         * @brief 获取 Include 映射
         * @return 行号 → 目标路径的映射
         *
         * 业务逻辑：
         * - Key: 行号（0-based）
         * - Value: #include 指令引用的文件绝对路径
         */
        const std::unordered_map<int, ShaderPath>& GetIncludes() const { return m_includes; }

        /**
         * @brief 检查指定行是否是 #include 指令
         * @param lineNumber 行号（0-based）
         * @return true 如果该行是 #include 指令
         */
        bool IsIncludeLine(int lineNumber) const
        {
            return m_includes.find(lineNumber) != m_includes.end();
        }

        /**
         * @brief 获取指定行的 Include 目标路径
         * @param lineNumber 行号（0-based）
         * @return 目标路径的 Optional（如果该行不是 #include 返回空）
         */
        std::optional<ShaderPath> GetIncludeTarget(int lineNumber) const
        {
            auto it = m_includes.find(lineNumber);
            if (it != m_includes.end())
            {
                return it->second;
            }
            return std::nullopt;
        }

        // ========================================================================
        // 调试支持
        // ========================================================================

        /**
         * @brief 获取调试信息（文件路径 + Include 数量）
         * @return 格式化的调试字符串
         */
        std::string ToString() const
        {
            return "FileNode {path: " + m_path.GetPathString() +
                ", lines: " + std::to_string(m_lines.size()) +
                ", includes: " + std::to_string(m_includes.size()) + "}";
        }

    private:
        /**
         * @brief 私有构造函数（使用工厂方法）
         * @param path 文件路径
         * @param lines 文件内容
         * @param includes Include 映射
         *
         * 教学要点：
         * - 私有构造函数确保所有实例都经过工厂方法创建
         * - 不可变对象：所有成员都是 const
         */
        FileNode(
            const ShaderPath&                          path,
            const std::vector<std::string>&            lines,
            const std::unordered_map<int, ShaderPath>& includes
        );

        /**
         * @brief 查找文件中的所有 #include 指令
         * @param currentDirectory 当前文件所在目录（用于解析相对路径）
         * @param lines 文件内容
         * @return Include 映射（行号 → 目标路径）
         *
         * 解析规则：
         * 1. 行必须以 `#include` 开头（忽略前导空白）
         * 2. 提取 `#include` 后的路径部分
         * 3. 移除引号（如果存在）
         * 4. 基于当前目录解析相对路径
         *
         * 示例：
         * - `#include "Common.hlsl"` → `/shaders/Common.hlsl`（假设当前目录 `/shaders`）
         * - `#include "../lib/Lighting.hlsl"` → `/lib/Lighting.hlsl`（回到上级目录）
         * - `  #include   "test.hlsl"  ` → 正确处理（忽略前导/尾随空白）
         *
         * 教学要点：
         * - 简单的字符串解析算法
         * - 相对路径解析（调用 AbsolutePackPath::Resolve()）
         * - 容错处理（允许缺少引号，但会正常处理）
         */
        static std::unordered_map<int, ShaderPath> FindIncludes(
            const ShaderPath&               currentDirectory,
            const std::vector<std::string>& lines
        );

    private:
        ShaderPath                          m_path; // 文件路径（不可变）
        std::vector<std::string>            m_lines; // 文件内容（按行，不可变）
        std::unordered_map<int, ShaderPath> m_includes; // Include 映射（不可变）

        /**
         * 教学要点总结：
         * 1. **不可变对象设计**：所有成员都是 const，线程安全
         * 2. **工厂方法模式**：私有构造函数 + 公有静态工厂
         * 3. **自动 Include 解析**：工厂方法自动调用 FindIncludes()
         * 4. **相对路径解析**：基于当前目录解析 #include 路径
         * 5. **std::optional 应用**：GetIncludeTarget() 可能无结果
         * 6. **依赖图节点**：为 IncludeGraph 提供节点抽象
         */
    };
} // namespace enigma::graphic
