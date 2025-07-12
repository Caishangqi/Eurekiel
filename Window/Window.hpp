#pragma once
#include <string>

#include "Engine/Math/IntVec2.hpp"

struct Vec2;
class InputSystem;

struct WindowConfig
{
    InputSystem* m_inputSystem = nullptr;
    float        m_aspectRatio = (16.f / 9.f);
    std::string  m_windowTitle = "Unnamed Application";
};

class Window
{
public:
    friend class IRenderer;
    Window(const WindowConfig& config);
    ~Window();

    void Startup();
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    const WindowConfig& GetConfig() const;
    void*               GetDisplayContext() const;
    void*               GetWindowHandle() const;
    Vec2                GetNormalizedMouseUV();
    IntVec2             GetClientDimensions() const;

    static Window* s_mainWindow; // fancy way of advertising global variables (advertisement)

private:
    void CreateOSWindow();

    WindowConfig m_config;
    void*        m_windowHandle   = nullptr; // Actually a Windows HWND on the Windows platform
    void*        m_displayContext = nullptr; // Actually a Windows HDC on the Windows platform
};
