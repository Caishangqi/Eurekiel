#pragma once

#include "../RenderCommand.hpp"
#include "Engine/Core/Logger/Logger.hpp"
#include <string>
#include <algorithm>

#include "Engine/Math/Vec2.hpp"

struct Vec2;

namespace enigma::graphic
{
    /// <summary>
    /// 调试文本绘制指令实现
    ///
    /// 用于Immediate模式的调试文本渲染，主要用于测试和验证渲染管线的完整调用链。
    /// 当前实现专注于日志输出验证，不进行实际的文本渲染到屏幕。
    ///
    /// 教学要点:
    /// - 理解Immediate模式指令的设计模式
    /// - 学习调试工具在渲染管线中的重要作用
    /// - 掌握命令模式(Command Pattern)的具体应用
    /// - 了解渲染系统的分层验证方法
    ///
    /// 使用场景:
    /// - 验证EnigmaRenderingPipeline的SetPhase机制
    /// - 测试RenderCommandQueue的阶段分发功能
    /// - 调试DebugRenderer的RenderAll执行流程
    /// - 确认完整的四层架构调用链正确性
    /// </summary>
    class DebugDrawTextCommand : public IRenderCommand
    {
    private:
        std::string m_text; // 要绘制的文本内容
        Vec2        m_position;
        Rgba8       m_color; // 文本颜色 (RGBA格式)
        std::string m_debugTag; // 调试标签，用于日志追踪

    public:
        /// <summary>
        /// 构造函数
        /// </summary>
        /// <param name="text">要显示的文本内容</param>
        /// <param name="color">文本颜色，RGBA格式 (默认白色)</param>
        /// <param name="debugTag">调试标签 (默认为空)</param>
        DebugDrawTextCommand(const std::string& text, Vec2 position, Rgba8 color, const std::string& debugTag = "");

        /// <summary>
        /// 获取指令类型
        /// 注意：由于这是调试指令，我们复用DRAW_INSTANCED类型
        /// 在实际应用中可能需要扩展RenderCommandType枚举
        /// </summary>
        RenderCommandType GetType() const override;
        /// <summary>
        /// 执行渲染指令
        ///
        /// 当前实现专注于验证调用链的正确性，通过详细的日志输出
        /// 来确认指令能够正确到达执行阶段。
        ///
        /// 教学要点:
        /// - 在实际项目中，这里会包含文本几何体生成
        /// - 顶点缓冲区创建和绑定
        /// - 字体纹理采样设置
        /// - 文本着色器程序激活
        /// - DrawIndexed调用执行
        /// </summary>
        /// <param name="commandManager">命令列表管理器</param>
        void Execute(std::shared_ptr<CommandListManager> commandManager) override;


        /// <summary>
        /// 获取指令名称(用于调试)
        /// </summary>
        std::string GetName() const override;

        /// <summary>
        /// 判断指令是否有效
        /// </summary>
        bool IsValid() const override;

        // === 访问器方法 ===

        /// <summary>获取文本内容</summary>
        const std::string& GetText() const { return m_text; }

        /// <summary>获取X坐标</summary>
        float GetX() const { return m_position.x; }

        /// <summary>获取Y坐标</summary>
        float GetY() const { return m_position.y; }

        /// <summary>获取颜色值</summary>
        Rgba8 GetColor() const { return m_color; }

        /// <summary>获取调试标签</summary>
        const std::string& GetDebugTag() const { return m_debugTag; }

        /// <summary>
        /// 设置新的文本内容（用于动态更新）
        /// </summary>
        void SetText(const std::string& newText);

        /// <summary>
        /// 设置新的位置（用于动态更新）
        /// </summary>
        void SetPosition(float x, float y);
    };
} // namespace enigma::graphic
