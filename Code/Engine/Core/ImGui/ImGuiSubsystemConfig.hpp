#pragma once

#include <string>
#include <cstdint>
#include "Engine/Renderer/IRenderer.hpp"

class Window;  // 前置声明Window类

namespace enigma::core
{

// ImGuiSubsystem配置结构（使用依赖注入模式）
struct ImGuiSubsystemConfig
{
    // 渲染器引用（通过IRenderer接口获取所有必需的DirectX资源）
    IRenderer* renderer = nullptr;

    // 通用配置
    Window* targetWindow = nullptr;
    bool enableDocking = false;        // Docking功能需要ImGui docking分支（标准版本不支持）
    bool enableViewports = false;      // 多窗口支持（独立窗口模式，需要docking分支）
    bool enableKeyboardNav = true;
    bool enableGamepadNav = false;

    // 字体配置
    std::string defaultFontPath = ".enigma/data/Fonts/KGRedHands.ttf";
    float defaultFontSize = 16.0f;

    // ImGui配置文件路径（相对于工作目录或绝对路径）
    // 默认为".enigma/config/imgui.ini"
    // 设置为空字符串将禁用ini文件保存/加载
    std::string iniFilePath = ".enigma/config/imgui.ini";
};

} // namespace enigma::core
