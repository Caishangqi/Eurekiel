#pragma once

#include <string>
#include "ThirdParty/json/json.hpp"

namespace enigma::graphic
{
    class RendererSubsystem;
}

class Window; // Pre-declare the Window class
class IImGuiRenderContext; // Forward declaration of ImGui rendering context interface
namespace enigma::core
{
    // ImGuiSubsystem configuration structure (using dependency injection mode)
    struct ImGuiSubsystemConfig
    {
        // ImGui rendering context interface (provides DirectX resource access required by ImGui)
        std::shared_ptr<IImGuiRenderContext> renderContext = nullptr;
        //General configuration
        Window* targetWindow      = nullptr;
        bool    enableDocking     = true; // Docking function (window docking)
        bool    enableViewports   = false; //Multiple viewport support (independent window mode, may require additional SwapChain management)
        bool    enableKeyboardNav = true;
        bool    enableGamepadNav  = false;

        // font configuration
        std::string defaultFontPath = ".enigma/assets/engine/font/JetBrainsMono-Regular.ttf";
        float       defaultFontSize = 16.0f;

        // ImGui configuration file path (relative to the working directory or absolute path)
        //The default is ".enigma/config/imgui.ini"
        // Set to an empty string to disable ini file saving/loading
        std::string iniFilePath = ".enigma/config/imgui.ini";
    };
} // namespace enigma::core
