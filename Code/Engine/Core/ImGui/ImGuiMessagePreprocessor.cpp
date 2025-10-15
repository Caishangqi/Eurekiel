#include "ImGuiMessagePreprocessor.hpp"

#ifndef IMGUI_DISABLE
#include "ThirdParty/imgui/imgui.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace enigma::core
{

ImGuiMessagePreprocessor::ImGuiMessagePreprocessor()
{
    m_isInitialized = true;
}

ImGuiMessagePreprocessor::~ImGuiMessagePreprocessor()
{
}

bool ImGuiMessagePreprocessor::ProcessMessage(
    HWND windowHandle,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    LRESULT& outResult)
{
#ifndef IMGUI_DISABLE
    if (m_isInitialized)
    {
        LRESULT result = ImGui_ImplWin32_WndProcHandler(windowHandle, message, wParam, lParam);
        if (result)
        {
            outResult = result;
            return true;  // ImGui消费了此消息
        }
    }
#endif

    return false;  // ImGui未消费，继续传递
}

} // namespace enigma::core
