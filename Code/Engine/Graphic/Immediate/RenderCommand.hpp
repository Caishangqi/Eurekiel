#pragma once

#include <memory>
#include <string>
#include <d3d12.h>
#include <wrl/client.h>
#include "../Core/Pipeline/WorldRenderingPhase.hpp"

namespace enigma::graphic
{
    // Forward declarations
    class D12Buffer;
    class D12Texture;
    class CommandListManager;

    /// <summary>
    /// 渲染指令的基础类型枚举
    /// 简化为只支持绘制指令
    /// </summary>
    enum class RenderCommandType
    {
        DRAW_INDEXED, // 索引化绘制
        DRAW_INSTANCED, // 实例化绘制
        DRAW_INDEXED_INSTANCED // 索引化实例化绘制
    };

    /// <summary>
    /// immediate模式渲染指令基类
    /// 
    /// 设计原则:
    /// - 支持延迟执行，指令可以被记录并稍后执行
    /// - 与Iris 10阶段渲染管线兼容
    /// - 支持自动Phase检测和状态管理
    /// - 集成DirectX 12命令列表系统
    /// 
    /// 教学要点:
    /// - Command Pattern实现，将操作封装为对象
    /// - 支持Undo/Redo功能的基础设计
    /// - Type-safe的参数传递机制
    /// </summary>
    class IRenderCommand
    {
    public:
        virtual ~IRenderCommand() = default;

        /// <summary>
        /// 获取指令类型
        /// </summary>
        virtual RenderCommandType GetType() const = 0;

        /// <summary>
        /// 执行渲染指令
        /// </summary>
        /// <param name="commandManager">命令列表管理器</param>
        virtual void Execute(std::shared_ptr<CommandListManager> commandManager) = 0;

        /// <summary>
        /// 获取指令名称(用于调试)
        /// </summary>
        virtual std::string GetName() const = 0;

        /// <summary>
        /// 判断指令是否有效
        /// </summary>
        virtual bool IsValid() const = 0;
    };

    /// <summary>
    /// 渲染指令的智能指针类型定义
    /// 使用unique_ptr确保内存安全
    /// </summary>
    using RenderCommandPtr = std::unique_ptr<IRenderCommand>;

    /// <summary>
    /// 简单的绘制指令工厂
    /// 
    /// 职责:
    /// - 创建绘制类型的渲染指令
    /// - 提供type-safe的参数验证
    /// 
    /// 设计模式: 简化的Factory Pattern
    /// </summary>
    class RenderCommandFactory
    {
    public:
        /// <summary>
        /// 创建索引化绘制指令
        /// </summary>
        static RenderCommandPtr CreateDrawIndexedCommand(
            uint32_t indexCount,
            uint32_t instanceCount         = 1,
            uint32_t startIndexLocation    = 0,
            int32_t  baseVertexLocation    = 0,
            uint32_t startInstanceLocation = 0
        );

        /// <summary>
        /// 创建实例化绘制指令
        /// </summary>
        static RenderCommandPtr CreateDrawInstancedCommand(
            uint32_t vertexCountPerInstance,
            uint32_t instanceCount,
            uint32_t startVertexLocation   = 0,
            uint32_t startInstanceLocation = 0
        );
    };

    /// <summary>
    /// 简化的渲染指令执行上下文
    /// 
    /// 包含执行指令所需的基本状态信息:
    /// - 当前渲染阶段(使用WorldRenderingPhase)
    /// - 基础性能计数器
    /// </summary>
    struct RenderCommandContext
    {
        using Phase        = WorldRenderingPhase;
        Phase currentPhase = Phase::NONE;

        // 基础性能计数器
        uint64_t frameIndex       = 0;
        uint64_t commandsExecuted = 0;
        uint64_t drawCalls        = 0;

        /// <summary>
        /// 重置上下文到初始状态
        /// </summary>
        void Reset()
        {
            currentPhase     = Phase::NONE;
            frameIndex       = 0;
            commandsExecuted = 0;
            drawCalls        = 0;
        }

        /// <summary>
        /// 获取当前阶段名称(用于调试)
        /// </summary>
        std::string GetPhaseName() const;
    };
} // namespace enigma::graphic
