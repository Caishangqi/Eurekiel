#pragma once
#include <string>

#include "Engine/Math/IntVec2.hpp"

// Forward declare Windows types to avoid including windows.h in header
#ifndef WIN32_LEAN_AND_MEAN
typedef unsigned long DWORD;
typedef struct tagRECT RECT;
#endif

struct Vec2;
class InputSystem;

enum class WindowMode
{
    Windowed = 0,
    Fullscreen = 1,
    BorderlessFullscreen = 2
};

struct WindowConfig
{
    InputSystem* m_inputSystem  = nullptr;
    float        m_aspectRatio  = (16.f / 9.f);
    std::string  m_windowTitle  = "Unnamed Application";
    WindowMode   m_windowMode   = WindowMode::Windowed;
    IntVec2      m_resolution   = IntVec2(1600, 900);
    bool         m_alwaysOnTop  = false; // Window always stays on top of other windows

    // Backward compatibility method
    bool IsFullscreen() const {
        return m_windowMode != WindowMode::Windowed;
    }
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

    float GetClientWidth() const;
    float GetClientHeight() const;
    float GetClientAspectRatio() const;

    WindowMode GetWindowMode() const;
    IntVec2    GetConfiguredResolution() const;
    bool       IsInFullscreenMode() const;
    bool       IsInWindowedMode() const;
    bool       IsAlwaysOnTop() const;
    void       SetAlwaysOnTop(bool alwaysOnTop);

    static Window* s_mainWindow; // fancy way of advertising global variables (advertisement)

private:
    void CreateOSWindow();
    void CreateFullscreenWindow(float desktopWidth, float desktopHeight, DWORD& windowStyleFlags, DWORD& windowStyleExFlags, RECT& windowRect);
    void CreateBorderlessFullscreenWindow(float desktopWidth, float desktopHeight, DWORD& windowStyleFlags, DWORD& windowStyleExFlags, RECT& windowRect);
    void CreateWindowedWindow(float desktopWidth, float desktopHeight, DWORD& windowStyleFlags, DWORD& windowStyleExFlags, RECT& windowRect);

    WindowConfig m_config;
    void*        m_windowHandle   = nullptr; // Actually a Windows HWND on the Windows platform
    void*        m_displayContext = nullptr; // Actually a Windows HDC on the Windows platform
};
