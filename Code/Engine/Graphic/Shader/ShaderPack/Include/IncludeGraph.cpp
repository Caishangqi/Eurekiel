#include "IncludeGraph.hpp"
#include <fstream>
#include <sstream>
#include <queue>
#include <algorithm>

namespace enigma::graphic
{
    // ========================================================================
    // 构造函数实现 - BFS 构建依赖图
    // ========================================================================

    IncludeGraph::IncludeGraph(
        const std::filesystem::path&         root,
        const std::vector<AbsolutePackPath>& startingPaths
    )
    {
        /**
         * 教学要点: BFS 构建 Include 依赖图
         *
         * 业务逻辑:
         * 1. 初始化队列和已访问集合
         * 2. 从起始路径开始 BFS 遍历
         * 3. 对每个文件：读取内容 → 解析 #include → 加入队列
         * 4. 处理读取失败（记录到 m_failures）
         * 5. 完成后检测循环依赖
         *
         * 算法伪代码:
         * ```
         * queue = [startingPaths]
         * visited = {}
         *
         * while queue is not empty:
         *     path = queue.dequeue()
         *     if path in visited: continue
         *     visited.add(path)
         *
         *     try:
         *         lines = ReadFile(path)
         *         node = FileNode.FromLines(path, lines)
         *         nodes[path] = node
         *
         *         for includedPath in node.GetIncludes():
         *             queue.enqueue(includedPath)
         *     catch:
         *         failures[path] = error
         * ```
         */

        // Step 1: 初始化队列和已访问集合
        std::queue<AbsolutePackPath>         queue;
        std::unordered_set<AbsolutePackPath> visited;

        // 将所有起始路径加入队列
        for (const auto& startPath : startingPaths)
        {
            queue.push(startPath);
        }

        // Step 2: BFS 遍历
        while (!queue.empty())
        {
            AbsolutePackPath currentPath = queue.front();
            queue.pop();

            // 检查是否已访问（避免重复读取）
            if (visited.find(currentPath) != visited.end())
            {
                continue; // 已处理，跳过
            }

            visited.insert(currentPath);

            // Step 3: 读取文件内容
            try
            {
                // 将 Shader Pack 路径转换为文件系统路径
                std::filesystem::path filePath = currentPath.Resolved(root);

                // 读取文件内容（按行）
                std::vector<std::string> lines = ReadFileLines(filePath);

                // 创建 FileNode（自动解析 #include）
                FileNode node = FileNode::FromLines(currentPath, lines);

                // 存储节点（使用 insert() 避免默认构造函数需求）
                m_nodes.insert({currentPath, node});

                // Step 4: 将所有 Include 的文件加入队列
                for (const auto& [lineNumber, includedPath] : node.GetIncludes())
                {
                    // 如果该文件未访问，加入队列
                    if (visited.find(includedPath) == visited.end())
                    {
                        queue.push(includedPath);
                    }
                }
            }
            catch (const std::exception& e)
            {
                // 文件读取失败或解析失败，记录错误
                m_failures.insert({currentPath, e.what()}); // 使用 insert() 保持代码一致性

                // 不中断构建，继续处理其他文件
            }
        }

        // Step 5: 检测循环依赖
        DetectCycle();
    }

    // ========================================================================
    // 文件读取辅助函数
    // ========================================================================

    std::vector<std::string> IncludeGraph::ReadFileLines(const std::filesystem::path& filePath)
    {
        /**
         * 教学要点: 文件读取（按行）
         *
         * 业务逻辑:
         * 1. 检查文件是否存在
         * 2. 打开文件流
         * 3. 按行读取到 vector<string>
         * 4. 异常处理
         */

        // Step 1: 检查文件是否存在
        if (!std::filesystem::exists(filePath))
        {
            throw std::runtime_error("File does not exist: " + filePath.string());
        }

        // Step 2: 打开文件流
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open file: " + filePath.string());
        }

        // Step 3: 按行读取
        std::vector<std::string> lines;
        std::string              line;

        while (std::getline(file, line))
        {
            lines.push_back(line);
        }

        return lines;
    }

    // ========================================================================
    // 循环依赖检测实现
    // ========================================================================

    void IncludeGraph::DetectCycle() const
    {
        /**
         * 教学要点: DFS 循环检测算法
         *
         * 业务逻辑:
         * 1. 对每个节点执行 DFS
         * 2. 维护全局已检测集合（避免重复检测）
         * 3. 维护当前路径（path）和路径节点集合（visited）
         * 4. 如果检测到循环，抛出异常
         *
         * 算法伪代码:
         * ```
         * globalChecked = {}
         *
         * for each node in nodes:
         *     if node in globalChecked: continue
         *
         *     path = []
         *     visited = {}
         *     if ExploreForCycles(node, path, visited):
         *         throw error with cycle path
         *
         *     globalChecked.add(all nodes in path)
         * ```
         */

        // 全局已检测集合（避免重复检测）
        std::unordered_set<AbsolutePackPath> globalChecked;

        for (const auto& [nodePath, node] : m_nodes)
        {
            // 如果该节点已检测（作为其他路径的一部分），跳过
            if (globalChecked.find(nodePath) != globalChecked.end())
            {
                continue;
            }

            // 当前路径（用于报告循环路径）
            std::vector<AbsolutePackPath> path;

            // 当前路径中的节点集合（用于快速检测循环）
            std::unordered_set<AbsolutePackPath> visited;

            // 执行 DFS 探索
            if (ExploreForCycles(nodePath, path, visited))
            {
                // 检测到循环，构建错误消息
                std::ostringstream errorMsg;
                errorMsg << "Circular dependency detected:\n";

                for (size_t i = 0; i < path.size(); ++i)
                {
                    errorMsg << "  " << path[i].GetPathString();
                    if (i < path.size() - 1)
                    {
                        errorMsg << " ->\n";
                    }
                }

                errorMsg << " -> " << path[0].GetPathString(); // 循环回到起点

                throw std::runtime_error(errorMsg.str());
            }

            // 将该路径中的所有节点标记为已检测
            for (const auto& pathNode : path)
            {
                globalChecked.insert(pathNode);
            }
        }
    }

    bool IncludeGraph::ExploreForCycles(
        const AbsolutePackPath&               frontier,
        std::vector<AbsolutePackPath>&        path,
        std::unordered_set<AbsolutePackPath>& visited
    ) const
    {
        /**
         * 教学要点: DFS 递归探索循环
         *
         * 业务逻辑:
         * 1. 检查节点是否在当前路径中（visited 集合）
         *    - 如果在 → 检测到循环
         * 2. 将节点加入路径和 visited 集合
         * 3. 对所有 Include 的文件递归探索
         * 4. 回溯：移除路径和 visited 中的节点
         *
         * 算法伪代码:
         * ```
         * function ExploreForCycles(node, path, visited):
         *     if node in visited:
         *         return true  // 检测到循环
         *
         *     path.add(node)
         *     visited.add(node)
         *
         *     for each included in node.includes:
         *         if included not in nodes:
         *             continue  // 文件不存在，跳过
         *
         *         if ExploreForCycles(included, path, visited):
         *             return true
         *
         *     path.remove(node)  // 回溯
         *     visited.remove(node)
         *     return false
         * ```
         */

        // Step 1: 检查是否在当前路径中（循环检测）
        if (visited.find(frontier) != visited.end())
        {
            // 检测到循环！
            return true;
        }

        // Step 2: 将节点加入路径和 visited 集合
        path.push_back(frontier);
        visited.insert(frontier);

        // Step 3: 递归探索所有 Include 的文件
        auto nodeIt = m_nodes.find(frontier);
        if (nodeIt != m_nodes.end())
        {
            const FileNode& currentNode = nodeIt->second;

            for (const auto& [lineNumber, includedPath] : currentNode.GetIncludes())
            {
                // 如果 Include 的文件不存在（加载失败），跳过
                if (m_nodes.find(includedPath) == m_nodes.end())
                {
                    continue;
                }

                // 递归探索
                if (ExploreForCycles(includedPath, path, visited))
                {
                    return true; // 检测到循环
                }
            }
        }

        // Step 4: 回溯 - 移除路径和 visited 中的节点
        path.pop_back();
        visited.erase(frontier);

        return false; // 未检测到循环
    }
} // namespace enigma::graphic
