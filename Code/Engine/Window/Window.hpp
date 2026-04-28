#pragma once
#include <string>
#include <vector>

#include "Engine/Math/IntVec2.hpp"

// Forward declare Windows types to avoid including windows.h in header
#ifndef WIN32_LEAN_AND_MEAN
typedef unsigned long  DWORD;
typedef struct tagRECT RECT;
#endif

struct Vec2;
class InputSystem;

namespace enigma::window
{
    class IWindowsMessagePreprocessor;
    enum class WindowCloseReason;
    struct WindowModeRequest;
}

enum class WindowMode
{
    Windowed = 0,
    BorderlessFullscreen = 1
};

struct WindowConfig
{
    InputSystem* m_inputSystem = nullptr;
    float        m_aspectRatio = (16.f / 9.f);
    std::string  m_windowTitle = "Unnamed Application";
    WindowMode   m_windowMode  = WindowMode::Windowed;
    IntVec2      m_resolution  = IntVec2(1920, 1080);
    bool         m_alwaysOnTop = false; // Window always stays on top of other windows
};

class Window
{
public:
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
    bool       IsMinimized() const;
    void       SetAlwaysOnTop(bool alwaysOnTop);
    bool       ApplyWindowModeRequest(const enigma::window::WindowModeRequest& request);
    bool       SetWindowMode(WindowMode mode);
    void       HandleClientSizeChanged(void* platformWindowHandle, unsigned int sizeType, int width, int height);
    void       RequestClose(enigma::window::WindowCloseReason reason);

    void RegisterMessagePreprocessor(enigma::window::IWindowsMessagePreprocessor* preprocessor);
    void UnregisterMessagePreprocessor(enigma::window::IWindowsMessagePreprocessor* preprocessor);

    static Window* s_mainWindow; // fancy way of advertising global variables (advertisement)

    std::vector<enigma::window::IWindowsMessagePreprocessor*> m_messagePreprocessors;

private:
    void CreateOSWindow();
    void CreateBorderlessFullscreenWindow(float desktopWidth, float desktopHeight, DWORD& windowStyleFlags, DWORD& windowStyleExFlags, RECT& windowRect);
    void CreateWindowedWindow(float desktopWidth, float desktopHeight, DWORD& windowStyleFlags, DWORD& windowStyleExFlags, RECT& windowRect);
    bool ApplyWindowedMode();
    bool ApplyBorderlessFullscreenMode();
    void SaveWindowedPlacementIfNeeded();
    void BroadcastWindowModeChanged(WindowMode oldMode, WindowMode newMode);

    void SortPreprocessors();

    WindowConfig m_config;
    void*        m_windowHandle   = nullptr; // Actually a Windows HWND on the Windows platform
    void*        m_displayContext = nullptr; // Actually a Windows HDC on the Windows platform
    IntVec2      m_lastClientDimensions = IntVec2();
    IntVec2      m_savedWindowedPosition = IntVec2();
    IntVec2      m_savedWindowedDimensions = IntVec2();
    bool         m_isMinimized          = false;
    bool         m_hasSavedWindowedPlacement = false;
};
