#pragma once
#include "Engine/Window/IWindowsMessagePreprocessor.hpp"

namespace enigma::core
{

// ImGui专用的消息预处理器
// 将Windows消息转发给ImGui_ImplWin32_WndProcHandler
class ImGuiMessagePreprocessor : public enigma::window::IWindowsMessagePreprocessor
{
public:
    ImGuiMessagePreprocessor();
    ~ImGuiMessagePreprocessor() override;

    // IWindowsMessagePreprocessor接口实现
    bool ProcessMessage(
        HWND windowHandle,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        LRESULT& outResult
    ) override;

    int GetPriority() const override { return 100; }  // UI框架优先级
    const char* GetName() const override { return "ImGuiMessagePreprocessor"; }

private:
    bool m_isInitialized = false;
};

} // namespace enigma::core
