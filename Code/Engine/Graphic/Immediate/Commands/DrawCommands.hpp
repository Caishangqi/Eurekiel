#pragma once

#include "../RenderCommand.hpp"
#include <memory>

namespace enigma::graphic
{
    // Forward declarations
    class CommandListManager;

    /// <summary>
    /// 索引化绘制指令实现
    /// 
    /// 对应DirectX 12的DrawIndexedInstanced调用
    /// 用于绘制使用索引缓冲区的几何体
    /// 
    /// 教学要点:
    /// - 理解索引缓冲区的作用和优势
    /// - 学习DirectX 12绘制调用的参数含义
    /// - 掌握immediate模式指令的具体实现
    /// </summary>
    class DrawIndexedCommand : public IRenderCommand
    {
    private:
        uint32_t m_indexCount;
        uint32_t m_instanceCount;
        uint32_t m_startIndexLocation;
        int32_t  m_baseVertexLocation;
        uint32_t m_startInstanceLocation;

    public:
        /// <summary>
        /// 构造函数
        /// </summary>
        DrawIndexedCommand(
            uint32_t indexCount,
            uint32_t instanceCount         = 1,
            uint32_t startIndexLocation    = 0,
            int32_t  baseVertexLocation    = 0,
            uint32_t startInstanceLocation = 0
        );

        /// <summary>
        /// 获取指令类型
        /// </summary>
        RenderCommandType GetType() const override;

        /// <summary>
        /// 执行绘制指令
        /// </summary>
        void Execute(std::shared_ptr<CommandListManager> commandManager) override;

        /// <summary>
        /// 获取指令名称
        /// </summary>
        std::string GetName() const override;

        /// <summary>
        /// 验证指令有效性
        /// </summary>
        bool IsValid() const override;
    };

    /// <summary>
    /// 非索引化实例绘制指令实现
    /// 
    /// 对应DirectX 12的DrawInstanced调用
    /// 用于绘制不使用索引缓冲区的几何体
    /// 
    /// 适用场景:
    /// - 简单几何体(如全屏四边形)
    /// - 程序化生成的几何体
    /// - 粒子系统等
    /// </summary>
    class DrawInstancedCommand : public IRenderCommand
    {
    private:
        uint32_t m_vertexCountPerInstance;
        uint32_t m_instanceCount;

    public:
        /// <summary>
        /// 构造函数
        /// </summary>
        DrawInstancedCommand(
            uint32_t vertexCountPerInstance,
            uint32_t instanceCount,
            uint32_t startVertexLocation   = 0,
            uint32_t startInstanceLocation = 0
        );

        RenderCommandType GetType() const override;

        void Execute(std::shared_ptr<CommandListManager> commandManager) override;

        std::string GetName() const override;

        bool IsValid() const override;
    };
} // namespace enigma::graphic
