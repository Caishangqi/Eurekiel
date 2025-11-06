#include "IncludeProcessor.hpp"
#include <sstream>
#include <stdexcept>

#include "Engine/Core/EngineCommon.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // 公共接口实现 - Expand 简单展开
    // ========================================================================

    std::string IncludeProcessor::Expand(
        const IncludeGraph& graph,
        const ShaderPath&   startPath
    )
    {
        /**
         * 教学要点: 简单的 DFS 展开（无 #line 指令）
         *
         * 业务逻辑:
         * 1. 检查起始文件是否存在
         * 2. 初始化已访问集合
         * 3. 调用递归展开函数
         */

        // Step 1: 检查起始文件是否在 IncludeGraph 中
        if (!graph.HasNode(startPath))
        {
            throw std::invalid_argument(
                "Start path not found in IncludeGraph: " + startPath.GetPathString()
            );
        }

        // Step 2: 初始化已访问集合
        std::unordered_set<ShaderPath> visited;

        // Step 3: 调用递归展开（不包含 #line 指令）
        int currentLineNumber = 1; // 占位参数（不使用）
        return ExpandRecursive(graph, startPath, visited, false, currentLineNumber);
    }

    // ========================================================================
    // 公共接口实现 - ExpandWithLineDirectives 带行号映射展开
    // ========================================================================

    std::string IncludeProcessor::ExpandWithLineDirectives(
        const IncludeGraph& graph,
        const ShaderPath&   startPath
    )
    {
        /**
         * 教学要点: 带 #line 指令的展开（用于调试）
         *
         * 业务逻辑:
         * 1. 检查起始文件是否存在
         * 2. 初始化已访问集合
         * 3. 调用递归展开函数（插入 #line 指令）
         */

        // Step 1: 检查起始文件是否在 IncludeGraph 中
        if (!graph.HasNode(startPath))
        {
            throw std::invalid_argument(
                "Start path not found in IncludeGraph: " + startPath.GetPathString()
            );
        }

        // Step 2: 初始化已访问集合
        std::unordered_set<ShaderPath> visited;

        // Step 3: 调用递归展开（包含 #line 指令）
        int currentLineNumber = 1;
        return ExpandRecursive(graph, startPath, visited, true, currentLineNumber);
    }

    // ========================================================================
    // 批量展开实现
    // ========================================================================

    std::unordered_map<ShaderPath, std::string> IncludeProcessor::ExpandMultiple(
        const IncludeGraph&            graph,
        const std::vector<ShaderPath>& programPaths
    )
    {
        /**
         * 教学要点: 批量展开优化
         *
         * 业务逻辑:
         * 1. 对每个程序路径调用 Expand()
         * 2. 存储结果到映射表
         * 3. 错误处理（跳过无效路径）
         *
         * 优化要点:
         * - 每个程序独立展开（不共享 visited 集合）
         * - 因为不同程序可能需要相同的 Include 文件
         */

        std::unordered_map<ShaderPath, std::string> results;

        for (const auto& programPath : programPaths)
        {
            try
            {
                // 展开该程序
                std::string expandedCode = Expand(graph, programPath);
                results[programPath]     = expandedCode;
            }
            catch (const std::exception& e)
            {
                UNUSED(e)
                // 跳过失败的程序（例如：路径不存在）
                // 错误会被记录到 IncludeGraph.GetFailures()
                continue;
            }
        }

        return results;
    }

    // ========================================================================
    // 私有递归实现 - ExpandRecursive
    // ========================================================================

    std::string IncludeProcessor::ExpandRecursive(
        const IncludeGraph&             graph,
        const ShaderPath&               currentPath,
        std::unordered_set<ShaderPath>& visited,
        bool                            includeLineDirectives,
        int&                            currentLineNumber
    )
    {
        /**
         * 教学要点: DFS 递归展开算法
         *
         * 业务逻辑:
         * 1. 检查是否已访问（防止重复包含）
         * 2. 标记为已访问
         * 3. 获取文件节点
         * 4. 遍历每一行：
         *    - 如果是 #include → 递归展开被包含文件
         *    - 如果是普通代码 → 直接添加
         * 5. 可选：插入 #line 指令用于调试
         *
         * 算法复杂度: O(V+E)，V = 文件数，E = Include 依赖数
         */

        // Step 1: 检查是否已访问（Once-include 模式）
        if (visited.find(currentPath) != visited.end())
        {
            return ""; // 已包含，避免重复
        }

        // Step 2: 标记为已访问
        visited.insert(currentPath);

        // Step 3: 获取文件节点
        auto nodeOpt = graph.GetNode(currentPath);
        if (!nodeOpt.has_value())
        {
            // 文件不存在（应该在 IncludeGraph 构建时已检测）
            return "";
        }

        const FileNode&    node = nodeOpt.value();
        std::ostringstream result;

        // Step 4: 可选 - 插入起始 #line 指令
        if (includeLineDirectives)
        {
            result << "#line 1 \"" << currentPath.GetPathString() << "\"\n";
        }

        // Step 5: 遍历文件的每一行
        const auto& lines = node.GetLines();
        for (int lineIndex = 0; lineIndex < static_cast<int>(lines.size()); ++lineIndex)
        {
            const std::string& line = lines[lineIndex];

            // 检查该行是否是 #include 指令
            if (node.IsIncludeLine(lineIndex))
            {
                // Step 5a: 获取 Include 目标路径
                auto includeTargetOpt = node.GetIncludeTarget(lineIndex);
                if (!includeTargetOpt.has_value())
                {
                    // 无效的 Include（应该在解析时已过滤）
                    continue;
                }

                ShaderPath includedPath = includeTargetOpt.value();

                // Step 5b: 递归展开被包含文件
                std::string includedContent = ExpandRecursive(
                    graph,
                    includedPath,
                    visited,
                    includeLineDirectives,
                    currentLineNumber
                );

                result << includedContent;

                // Step 5c: 可选 - 插入返回 #line 指令（回到当前文件）
                if (includeLineDirectives)
                {
                    // #include 指令之后的下一行
                    int nextLine = lineIndex + 2; // +1 因为行号从 1 开始，+1 因为跳过 #include 行
                    result << "#line " << nextLine << " \"" << currentPath.GetPathString() << "\"\n";
                }
            }
            else
            {
                // Step 5d: 普通代码行，直接添加
                result << line << "\n";
            }
        }

        return result.str();
    }
} // namespace enigma::graphic
