#pragma once

#include "AbsolutePackPath.hpp"
#include "FileNode.hpp"
#include "FileIncludeException.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <filesystem>
#include <functional>

namespace enigma::graphic
{
    /**
     * @class IncludeGraph
     * @brief 有向依赖图 - 管理所有着色器文件及其 #include 依赖关系
     *
     * **设计原则**：
     * - 单次文件读取（每个文件只读取一次）
     * - BFS 构建依赖图（广度优先搜索）
     * - DFS 循环检测（深度优先搜索 + 路径回溯）
     * - 延迟处理 Include（性能优化）
     *
     * **对应 Iris 源码**：
     * - Iris: IncludeGraph.java
     * - 职责：构建并验证 Shader Pack 的完整依赖图
     *
     * **核心算法**：
     * 1. **BFS 构建**（构造函数）：
     *    - 从起始路径列表（着色器程序文件）开始
     *    - 使用队列进行 BFS 遍历
     *    - 读取每个文件并解析 #include 指令
     *    - 跟踪已访问节点（避免重复读取）
     *
     * 2. **循环检测**（DetectCycle）：
     *    - 使用 DFS 检测循环依赖
     *    - 路径回溯算法（visited set + path stack）
     *    - 检测到循环时报告完整路径
     *
     * **性能优势**：
     * - ✅ 每个文件只读取一次（无论被 #include 多少次）
     * - ✅ 高效的 BFS/DFS 算法（O(V+E) 复杂度）
     * - ✅ 延迟处理 Include（转换器可以更高效地操作）
     *
     * **使用场景**：
     * ```cpp
     * // 构建依赖图
     * std::vector<AbsolutePackPath> startingPaths = {
     *     AbsolutePackPath::FromAbsolutePath("/shaders/gbuffers_terrain.vs.hlsl"),
     *     AbsolutePackPath::FromAbsolutePath("/shaders/gbuffers_terrain.ps.hlsl")
     * };
     *
     * IncludeGraph graph(
     *     std::filesystem::path("F:/shaderpacks/MyPack"),
     *     startingPaths
     * );
     *
     * // 检查构建失败
     * if (!graph.GetFailures().empty()) {
     *     for (const auto& [path, error] : graph.GetFailures()) {
     *         std::cout << "Failed: " << path.GetPathString() << " - " << error << std::endl;
     *     }
     * }
     *
     * // 访问节点
     * const auto& nodes = graph.GetNodes();
     * for (const auto& [path, node] : nodes) {
     *     std::cout << "Loaded: " << path.GetPathString() << std::endl;
     * }
     * ```
     */
    class IncludeGraph
    {
    public:
        /**
         * @brief 构建 Include 依赖图（BFS + 循环检测）
         * @param root Shader Pack 根目录（文件系统路径）
         * @param startingPaths 起始路径列表（着色器程序文件）
         *
         * 教学要点：
         * - BFS 算法：使用队列遍历依赖树
         * - 自动读取文件：从文件系统加载 .hlsl 文件
         * - 循环检测：构建完成后自动调用 DetectCycle()
         *
         * 业务逻辑：
         * 1. 初始化队列和已访问集合
         * 2. 从起始路径开始 BFS 遍历
         * 3. 对每个文件：读取内容 → 解析 #include → 加入队列
         * 4. 处理读取失败（记录到 m_failures）
         * 5. 完成后检测循环依赖
         *
         * 异常：
         * - std::runtime_error: 如果检测到循环依赖
         */
        IncludeGraph(
            const std::filesystem::path&         root,
            const std::vector<AbsolutePackPath>& startingPaths
        );

        // ========================================================================
        // 访问器 - 获取图数据
        // ========================================================================

        /**
         * @brief 获取所有成功加载的文件节点
         * @return 路径 → 节点映射
         */
        const std::unordered_map<AbsolutePackPath, FileNode>& GetNodes() const { return m_nodes; }

        /**
         * @brief 获取所有加载失败的文件
         * @return 路径 → 错误消息映射
         */
        const std::unordered_map<AbsolutePackPath, std::string>& GetFailures() const { return m_failures; }

        /**
         * @brief 检查指定文件是否已加载
         * @param path 文件路径（Shader Pack 内部路径）
         * @return true 如果文件已成功加载
         */
        bool HasNode(const AbsolutePackPath& path) const
        {
            return m_nodes.find(path) != m_nodes.end();
        }

        /**
         * @brief 获取指定文件的节点
         * @param path 文件路径
         * @return 节点的 Optional（如果文件未加载返回空）
         */
        std::optional<FileNode> GetNode(const AbsolutePackPath& path) const
        {
            auto it = m_nodes.find(path);
            if (it != m_nodes.end())
            {
                return it->second;
            }
            return std::nullopt;
        }

        // ========================================================================
        // 调试支持
        // ========================================================================

        /**
         * @brief 获取图的统计信息
         * @return 格式化的统计字符串
         */
        std::string GetStatistics() const
        {
            return "IncludeGraph {nodes: " + std::to_string(m_nodes.size()) +
                ", failures: " + std::to_string(m_failures.size()) + "}";
        }

    private:
        /**
         * @brief 读取文件内容（按行）
         * @param filePath 文件系统路径
         * @return 文件内容（按行存储）
         * @throws std::runtime_error: 如果文件读取失败
         */
        static std::vector<std::string> ReadFileLines(const std::filesystem::path& filePath);

        /**
         * @brief 检测依赖图中的循环依赖
         *
         * 教学要点：
         * - DFS 循环检测算法
         * - 路径回溯（visited set + path vector）
         * - 检测到循环时报告完整路径
         *
         * 业务逻辑：
         * 1. 对每个节点执行 DFS
         * 2. 维护访问路径（path）和已访问集合（visited）
         * 3. 如果访问到已在路径中的节点 → 循环
         * 4. 回溯时移除路径节点（允许访问其他分支）
         *
         * 异常：
         * - std::runtime_error: 如果检测到循环依赖
         */
        void DetectCycle() const;

        /**
         * @brief DFS 探索循环（递归辅助函数）
         * @param frontier 当前探索的节点
         * @param path 当前访问路径（用于回溯）
         * @param visited 当前路径中的已访问节点
         * @return true 如果检测到循环
         *
         * 教学要点：
         * - 经典的 DFS 循环检测算法
         * - visited 集合跟踪当前路径（非全局）
         * - 回溯时移除 visited 节点（允许其他分支访问）
         *
         * 算法伪代码：
         * ```
         * function ExploreForCycles(node, path, visited):
         *     if node in visited:
         *         return true  // 检测到循环
         *
         *     path.add(node)
         *     visited.add(node)
         *
         *     for each included in node.includes:
         *         if ExploreForCycles(included, path, visited):
         *             return true
         *
         *     path.remove(node)  // 回溯
         *     visited.remove(node)
         *     return false
         * ```
         */
        bool ExploreForCycles(
            const AbsolutePackPath&               frontier,
            std::vector<AbsolutePackPath>&        path,
            std::unordered_set<AbsolutePackPath>& visited
        ) const;

    private:
        std::unordered_map<AbsolutePackPath, FileNode>    m_nodes; // 成功加载的节点
        std::unordered_map<AbsolutePackPath, std::string> m_failures; // 加载失败的文件

        /**
         * 教学要点总结：
         * 1. **BFS 构建依赖图**：队列 + 已访问集合，单次文件读取
         * 2. **DFS 循环检测**：路径回溯 + visited 集合，O(V+E) 复杂度
         * 3. **错误处理**：失败文件记录到 m_failures，不中断构建
         * 4. **性能优化**：每个文件只读取一次，延迟处理 Include
         * 5. **图论应用**：有向图表示依赖关系，经典算法应用
         */
    };
} // namespace enigma::graphic
