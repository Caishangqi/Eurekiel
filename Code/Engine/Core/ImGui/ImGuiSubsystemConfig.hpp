#pragma once

#include <string>
#include <cstdint>
#include "ThirdParty/json/json.hpp"

namespace enigma::graphic
{
    class RendererSubsystem;
}

class Window; // 前置声明Window类
class IImGuiRenderContext; // 前向声明ImGui渲染上下文接口

namespace enigma::core
{
    // ImGuiSubsystem配置结构（使用依赖注入模式）
    struct ImGuiSubsystemConfig
    {
        // ImGui渲染上下文接口（提供ImGui所需的DirectX资源访问）
        std::shared_ptr<IImGuiRenderContext> renderContext = nullptr;

        // 通用配置
        Window* targetWindow      = nullptr;
        bool    enableDocking     = false; // Docking功能需要ImGui docking分支（标准版本不支持）
        bool    enableViewports   = false; // 多窗口支持（独立窗口模式，需要docking分支）
        bool    enableKeyboardNav = true;
        bool    enableGamepadNav  = false;

        // 字体配置
        std::string defaultFontPath = ".enigma/assets/engine/font/JetBrainsMono-Regular.ttf";
        float       defaultFontSize = 16.0f;

        // ImGui配置文件路径（相对于工作目录或绝对路径）
        // 默认为".enigma/config/imgui.ini"
        // 设置为空字符串将禁用ini文件保存/加载
        std::string iniFilePath = ".enigma/config/imgui.ini";
    };
} // namespace enigma::core
