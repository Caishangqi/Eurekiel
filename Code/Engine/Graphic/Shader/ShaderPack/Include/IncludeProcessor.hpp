#pragma once

#include "IncludeGraph.hpp"
#include "ShaderPath.hpp"
#include "FileNode.hpp"
#include <string>
#include <vector>
#include <unordered_set>

namespace enigma::graphic
{
    /**
     * @class IncludeProcessor
     * @brief Include 展开处理器 - 将 #include 指令替换为实际文件内容
     *
     * **设计原则**：
     * - 基于已构建的 IncludeGraph
     * - DFS 递归展开所有 #include
     * - 防止重复包含（Once-include 模式）
     * - 保留 #line 指令用于调试
     *
     * **对应 Iris 源码**：
     * - Iris: ShaderPreprocessor.java (GLSL 预处理)
     * - Enigma: IncludeProcessor.hpp (HLSL 预处理)
     * - 职责：展开所有 #include 指令，生成单文件着色器代码
     *
     * **核心算法**：
     * 1. **DFS 展开**（Expand）：
     *    - 递归访问所有 #include 文件
     *    - 维护已访问集合（防止重复包含）
     *    - 替换 #include 指令为文件内容
     *
     * 2. **行号映射**（ExpandWithLineDirectives）：
     *    - 插入 #line 指令标记原始文件和行号
     *    - 用于编译错误定位（DXC 错误消息会引用原始文件）
     *
     * **使用场景**：
     * ```cpp
     * // 已构建 IncludeGraph
     * IncludeGraph graph(root, startingPaths);
     *
     * // 展开着色器程序
     * ShaderPath programPath = ShaderPath::FromAbsolutePath("/shaders/gbuffers_terrain.vs.hlsl");
     * std::string expandedCode = IncludeProcessor::Expand(graph, programPath);
     *
     * // 或者：保留行号映射（用于调试）
     * std::string expandedCodeWithLines = IncludeProcessor::ExpandWithLineDirectives(graph, programPath);
     * ```
     *
     * **示例输入**：
     * ```hlsl
     * // gbuffers_terrain.vs.hlsl
     * #include "Common.hlsl"
     * #include "lib/Lighting.hlsl"
     *
     * void main() {
     *     // ...
     * }
     * ```
     *
     * **示例输出（Expand）**：
     * ```hlsl
     * // Common.hlsl 的内容
     * // lib/Lighting.hlsl 的内容
     *
     * void main() {
     *     // ...
     * }
     * ```
     *
     * **示例输出（ExpandWithLineDirectives）**：
     * ```hlsl
     * #line 1 "/shaders/Common.hlsl"
     * // Common.hlsl 的内容
     * #line 1 "/shaders/lib/Lighting.hlsl"
     * // lib/Lighting.hlsl 的内容
     * #line 3 "/shaders/gbuffers_terrain.vs.hlsl"
     *
     * void main() {
     *     // ...
     * }
     * ```
     */
    class IncludeProcessor
    {
    public:
        /**
         * @brief 展开着色器程序的所有 #include 指令
         * @param graph 已构建的 Include 依赖图
         * @param startPath 起始文件路径（着色器程序文件）
         * @return 展开后的完整着色器代码（单文件）
         *
         * 教学要点：
         * - DFS 递归展开所有 #include
         * - Once-include 模式（防止重复包含）
         * - 简洁输出（无 #line 指令）
         *
         * 业务逻辑：
         * 1. 从起始文件开始
         * 2. 对每行代码：
         *    - 如果是 #include → 递归展开被包含文件
         *    - 如果是普通代码 → 直接添加
         * 3. 维护已访问集合（避免重复）
         *
         * 异常：
         * - std::invalid_argument: 如果起始文件不在 IncludeGraph 中
         */
        static std::string Expand(
            const IncludeGraph& graph,
            const ShaderPath&   startPath
        );

        /**
         * @brief 展开着色器程序并保留行号映射（插入 #line 指令）
         * @param graph 已构建的 Include 依赖图
         * @param startPath 起始文件路径
         * @return 展开后的代码（包含 #line 指令）
         *
         * 教学要点：
         * - 插入 #line 指令用于调试
         * - DXC 编译错误会引用原始文件和行号
         * - 适用于开发和调试阶段
         *
         * HLSL #line 指令格式：
         * - `#line <line_number> "<file_path>"`
         * - 例如：`#line 42 "/shaders/Common.hlsl"`
         *
         * 使用场景：
         * - 开发阶段：使用此方法（更好的错误定位）
         * - 发布阶段：使用 Expand（更小的文件大小）
         */
        static std::string ExpandWithLineDirectives(
            const IncludeGraph& graph,
            const ShaderPath&   startPath
        );

        // ========================================================================
        // 批量展开接口
        // ========================================================================

        /**
         * @brief 批量展开多个着色器程序
         * @param graph 已构建的 Include 依赖图
         * @param programPaths 着色器程序路径列表
         * @return 路径 → 展开代码的映射
         *
         * 教学要点：
         * - 批量处理优化（共享已访问集合）
         * - 适用于 ShaderPackLoader 批量加载场景
         */
        static std::unordered_map<ShaderPath, std::string> ExpandMultiple(
            const IncludeGraph&            graph,
            const std::vector<ShaderPath>& programPaths
        );

    private:
        /**
         * @brief 递归展开 Include（DFS 辅助函数）
         * @param graph Include 依赖图
         * @param currentPath 当前文件路径
         * @param visited 已访问集合（防止重复包含）
         * @param includeLineDirectives 是否插入 #line 指令
         * @param currentLineNumber 当前行号（用于 #line 指令）
         * @return 展开后的代码
         *
         * 教学要点：
         * - 经典的 DFS 递归算法
         * - visited 集合防止重复包含
         * - 递归深度等于最大 Include 嵌套深度
         *
         * 算法伪代码：
         * ```
         * function ExpandRecursive(path, visited, includeLineDirectives):
         *     if path in visited:
         *         return ""  // 已包含，避免重复
         *
         *     visited.add(path)
         *     node = graph.GetNode(path)
         *     result = ""
         *
         *     if includeLineDirectives:
         *         result += "#line 1 \"" + path + "\"\n"
         *
         *     for each (lineNumber, line) in node.lines:
         *         if line is #include:
         *             includedPath = node.GetIncludeTarget(lineNumber)
         *             result += ExpandRecursive(includedPath, visited, includeLineDirectives)
         *
         *             if includeLineDirectives:
         *                 result += "#line " + (lineNumber+2) + " \"" + path + "\"\n"
         *         else:
         *             result += line + "\n"
         *
         *     return result
         * ```
         */
        static std::string ExpandRecursive(
            const IncludeGraph&             graph,
            const ShaderPath&               currentPath,
            std::unordered_set<ShaderPath>& visited,
            bool                            includeLineDirectives,
            int&                            currentLineNumber
        );

        /**
         * 教学要点总结：
         * 1. **DFS 递归展开**：从起始文件递归访问所有 Include 文件
         * 2. **Once-include 模式**：visited 集合防止重复包含
         * 3. **#line 指令支持**：保留原始文件和行号信息
         * 4. **异常处理**：缺失文件在 IncludeGraph 构建时已检测
         * 5. **批量优化**：ExpandMultiple 共享已访问集合
         * 6. **KISS 原则**：简单的 DFS 算法，无需复杂的预处理器
         */
    };
} // namespace enigma::graphic
