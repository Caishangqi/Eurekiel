#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace enigma::window
{

// Windows消息预处理器接口
// 所有需要接收原始Windows消息的组件都应实现此接口
class IWindowsMessagePreprocessor
{
public:
    virtual ~IWindowsMessagePreprocessor() = default;

    // 处理Windows消息
    // 返回值：
    //   - true: 消息已被此预处理器消费，阻止后续处理
    //   - false: 消息未消费，继续传递给下一个预处理器
    virtual bool ProcessMessage(
        HWND windowHandle,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        LRESULT& outResult
    ) = 0;

    // 获取处理器优先级（数值越小越优先）
    // 推荐优先级范围：
    //   0-99:   系统级（调试工具、性能分析器）
    //   100-199: UI框架（ImGui等）
    //   200-299: 游戏输入系统
    //   300+:    其他
    virtual int GetPriority() const = 0;

    // 获取处理器名称（用于调试）
    virtual const char* GetName() const = 0;
};

} // namespace enigma::window
