#include "IncludeGraph.hpp"
#include "Engine/Graphic/Shader/Common/FileSystemReader.hpp"
#include <sstream>
#include <queue>
#include <algorithm>

namespace enigma::graphic
{
    // ========================================================================
    // 构造函数实现 - BFS 构建依赖图（依赖注入版本）
    // ========================================================================

    IncludeGraph::IncludeGraph(
        std::shared_ptr<IFileReader>   fileReader,
        const std::vector<ShaderPath>& startingPaths
    )
        : m_fileReader(std::move(fileReader))
    {
        /**
         * 教学要点: BFS 构建 Include 依赖图 + 依赖注入
         *
         * 业务逻辑:
         * 1. 初始化队列和已访问集合
         * 2. 从起始路径开始 BFS 遍历
         * 3. 对每个文件：使用IFileReader读取 → 解析 #include → 加入队列
         * 4. 处理读取失败（记录到 m_failures）
         * 5. 完成后检测循环依赖
         *
         * 依赖倒置原则:
         * - 依赖IFileReader抽象接口，不依赖具体文件系统操作
         * - 通过依赖注入获取文件读取器
         * - 解耦文件读取逻辑，提高可测试性
         */

        // Step 1: 初始化队列和已访问集合
        std::queue<ShaderPath>         queue;
        std::unordered_set<ShaderPath> visited;

        // 将所有起始路径加入队列
        for (const auto& startPath : startingPaths)
        {
            queue.push(startPath);
        }

        // Step 2: BFS 遍历
        while (!queue.empty())
        {
            ShaderPath currentPath = queue.front();
            queue.pop();

            // 检查是否已访问（避免重复读取）
            if (visited.find(currentPath) != visited.end())
            {
                continue; // 已处理，跳过
            }

            visited.insert(currentPath);

            // Step 3: 使用IFileReader读取文件内容
            try
            {
                // 使用IFileReader接口读取文件
                auto contentOpt = m_fileReader->ReadFile(currentPath);

                if (!contentOpt)
                {
                    // 文件读取失败
                    m_failures.insert({currentPath, "Failed to read file"});
                    continue; // 继续处理其他文件
                }

                // 将文件内容按行分割
                std::vector<std::string> lines;
                std::istringstream       stream(*contentOpt);
                std::string              line;

                while (std::getline(stream, line))
                {
                    lines.push_back(line);
                }

                // 创建 FileNode（自动解析 #include）
                FileNode node = FileNode::FromLines(currentPath, lines);

                // 存储节点
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
                // 解析失败，记录错误
                m_failures.insert({currentPath, e.what()});
                // 不中断构建，继续处理其他文件
            }
        }

        // Step 5: 检测循环依赖
        DetectCycle();
    }

    // ========================================================================
    // 向后兼容构造函数 - 委托到依赖注入版本
    // ========================================================================

    IncludeGraph::IncludeGraph(
        const std::filesystem::path&   root,
        const std::vector<ShaderPath>& startingPaths
    )
        : IncludeGraph(
            std::make_shared<FileSystemReader>(root),
            startingPaths
        )
    {
        /**
         * 教学要点: 委托构造函数 + 向后兼容
         *
         * 业务逻辑:
         * - 创建FileSystemReader并委托到新构造函数
         * - 保持向后兼容，旧代码无需修改
         *
         * 设计原则:
         * - 委托构造：避免代码重复
         * - 向后兼容：保留旧接口
         * - 过渡策略：逐步迁移到新接口
         */
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
         */

        // 全局已检测集合（避免重复检测）
        std::unordered_set<ShaderPath> globalChecked;

        for (const auto& [nodePath, node] : m_nodes)
        {
            // 如果该节点已检测（作为其他路径的一部分），跳过
            if (globalChecked.find(nodePath) != globalChecked.end())
            {
                continue;
            }

            // 当前路径（用于报告循环路径）
            std::vector<ShaderPath> path;

            // 当前路径中的节点集合（用于快速检测循环）
            std::unordered_set<ShaderPath> visited;

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
        const ShaderPath&               frontier,
        std::vector<ShaderPath>&        path,
        std::unordered_set<ShaderPath>& visited
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
