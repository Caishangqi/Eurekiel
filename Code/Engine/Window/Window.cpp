#include "Window.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "IWindowsMessagePreprocessor.hpp"
#include "WindowEvents.hpp"

// static variables
Window* Window::s_mainWindow = nullptr;

//-----------------------------------------------------------------------------------------------
// Handles Windows (Win32) messages/events; i.e. the OS is trying to tell us something happened.
// This function is called back by Windows whenever we tell it to (by calling DispatchMessage).

LRESULT CALLBACK WindowsMessageHandlingProcedure(HWND windowHandle, UINT wmMessageCode, WPARAM wParam, LPARAM lParam)
{
    if (Window::s_mainWindow)
    {
        switch (wmMessageCode)
        {
        case WM_SIZE:
            {
                const int width  = LOWORD(lParam);
                const int height = HIWORD(lParam);
                Window::s_mainWindow->HandleClientSizeChanged(
                    windowHandle,
                    static_cast<unsigned int>(wParam),
                    width,
                    height);
                break;
            }
        case WM_CLOSE:
            {
                Window::s_mainWindow->RequestClose(enigma::window::WindowCloseReason::CloseButton);
                return 0;
            }
        default: ;
        }
    }

    if (Window::s_mainWindow)
    {
        LRESULT result = 0;
        for (auto* preprocessor : Window::s_mainWindow->m_messagePreprocessors)
        {
            if (preprocessor->ProcessMessage(windowHandle, wmMessageCode, wParam, lParam, result))
            {
                return result;
            }
        }
    }

    InputSystem* input = nullptr;

    if (Window::s_mainWindow && Window::s_mainWindow->GetConfig().m_inputSystem)
    {
        input = Window::s_mainWindow->GetConfig().m_inputSystem;
    }

    switch (wmMessageCode)
    {
    case WM_CHAR:
        {
            EventArgs args;
            args.SetValue("KeyCode", Stringf("%d", static_cast<unsigned char>(wParam)));
            FireEvent("CharInput", args);
            return 0;
        }
    // Raw physical keyboard "key-was-just-depressed" event (case-insensitive, not translated)
    case WM_KEYDOWN:
        {
            unsigned char keyCode  = static_cast<unsigned char>(wParam);
            bool          ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (ctrlDown && keyCode == 'V')
            {
                FireEvent("PasteClipboard");
                return 0;
            }

            EventArgs args;
            args.SetValue("KeyCode", Stringf("%d", keyCode));
            FireEvent("KeyPressed", args);
            return 0;
        }

    // Raw physical keyboard "key-was-just-released" event (case-insensitive, not translated)
    case WM_KEYUP:
        {
            EventArgs args;
            args.SetValue("KeyCode", Stringf("%d", static_cast<unsigned char>(wParam)));
            FireEvent("KeyReleased", args);
            return 0;
        }

    case WM_LBUTTONDOWN:
        {
            // Handle left mouse button press event
            if (input)
                input->HandleMouseButtonPressed(KEYCODE_LEFT_MOUSE);
            break;
        }
    case WM_LBUTTONUP:
        {
            //Handle the left mouse button release event
            if (input)
                input->HandleMouseButtonReleased(KEYCODE_LEFT_MOUSE);
            break;
        }
    case WM_RBUTTONDOWN:
        {
            //Handle the right mouse button press event
            if (input)
                input->HandleMouseButtonPressed(KEYCODE_RIGHT_MOUSE);
            break;
        }
    case WM_RBUTTONUP:
        {
            //Handle the right mouse button release event
            if (input)
                input->HandleMouseButtonReleased(KEYCODE_RIGHT_MOUSE);
            break;
        }
    case WM_MBUTTONDOWN:
        {
            //Handle the middle mouse button press event
            if (input)
                input->HandleMouseButtonPressed(KEYCODE_MIDDLE_MOUSE);
            break;
        }
    case WM_MBUTTONUP:
        {
            //Handle the middle mouse button release event
            if (input)
                input->HandleMouseButtonReleased(KEYCODE_MIDDLE_MOUSE);
            break;
        }
    case WM_MOUSEMOVE:
        {
            // Get the screen coordinates of the mouse
            int mouseX = LOWORD(lParam);
            int mouseY = HIWORD(lParam);
            if (input)
                input->HandleMouseMove(mouseX, mouseY);
            break;
        }
    case WM_MOUSEWHEEL:
        {
            //Handle mouse wheel events
            short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (input)
                input->HandleMouseWheel(wheelDelta);
            break;
        }

    default: ;
    }

    // Send back to Windows any unhandled/unconsumed messages we want other apps to see (e.g. play/pause in music apps, etc.)
    return DefWindowProc(windowHandle, wmMessageCode, wParam, lParam);
}


Window::Window(const WindowConfig& config)
{
    m_config     = config;
    s_mainWindow = this;
}

Window::~Window()
{
}

void Window::Startup()
{
    CreateOSWindow();
}

void Window::Shutdown()
{
    m_messagePreprocessors.clear();
}

void Window::BeginFrame()
{
}

void Window::EndFrame()
{
}

// return a reference that is read only
const WindowConfig& Window::GetConfig() const
{
    return m_config;
}

void* Window::GetDisplayContext() const
{
    return m_displayContext;
}

void* Window::GetWindowHandle() const
{
    return m_windowHandle;
}

Vec2 Window::GetNormalizedMouseUV()
{
    auto  windowHandle = static_cast<HWND>(GetWindowHandle());
    POINT cursorCoords;
    RECT  clientRect;
    GetCursorPos(&cursorCoords); // in Windows screen coordinates; (0,0) is top-left
    ScreenToClient(windowHandle, &cursorCoords); // get relative to window's client area
    GetClientRect(windowHandle, &clientRect); // dimensions of client are (0,0 to width, height)
    float cursorX = static_cast<float>(cursorCoords.x) / static_cast<float>(clientRect.right);
    float cursorY = static_cast<float>(cursorCoords.y) / static_cast<float>(clientRect.bottom);
    return Vec2(cursorX, 1.f - cursorY); // Flip Y; we want (0,0) bottom-left, not top-left
}

IntVec2 Window::GetClientDimensions() const
{
    auto windowHandle = static_cast<HWND>(GetWindowHandle());
    if (!windowHandle)
    {
        return m_config.m_resolution;
    }

    RECT clientRect;
    GetClientRect(windowHandle, &clientRect);
    return IntVec2(clientRect.right, clientRect.bottom);
}

float Window::GetClientWidth() const
{
    return static_cast<float>(GetClientDimensions().x);
}

float Window::GetClientHeight() const
{
    return static_cast<float>(GetClientDimensions().y);
}

float Window::GetClientAspectRatio() const
{
    IntVec2 dimensions = GetClientDimensions();
    if (dimensions.y == 0)
        return m_config.m_aspectRatio;
    return static_cast<float>(dimensions.x) / static_cast<float>(dimensions.y);
}

WindowMode Window::GetWindowMode() const
{
    return m_config.m_windowMode;
}

IntVec2 Window::GetConfiguredResolution() const
{
    return m_config.m_resolution;
}

bool Window::IsInFullscreenMode() const
{
    return m_config.m_windowMode == WindowMode::BorderlessFullscreen;
}

bool Window::IsInWindowedMode() const
{
    return m_config.m_windowMode == WindowMode::Windowed;
}

bool Window::IsAlwaysOnTop() const
{
    return m_config.m_alwaysOnTop;
}

bool Window::IsMinimized() const
{
    return m_isMinimized;
}

void Window::SetAlwaysOnTop(bool alwaysOnTop)
{
    if (m_config.m_alwaysOnTop == alwaysOnTop)
        return; // No change needed

    m_config.m_alwaysOnTop = alwaysOnTop;

    // Apply the change to the actual window if it exists
    if (m_windowHandle)
    {
        auto windowHandle = static_cast<HWND>(m_windowHandle);
        HWND insertAfter  = alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;
        SetWindowPos(windowHandle, insertAfter, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

        DebuggerPrintf("Window always-on-top set to: %s\n",
                       alwaysOnTop ? "true" : "false");
    }
}

bool Window::ApplyWindowModeRequest(const enigma::window::WindowModeRequest& request)
{
    switch (request.type)
    {
    case enigma::window::WindowModeRequestType::ToggleFullscreen:
        return SetWindowMode(IsInWindowedMode() ? WindowMode::BorderlessFullscreen : WindowMode::Windowed);
    case enigma::window::WindowModeRequestType::SetWindowed:
        return SetWindowMode(WindowMode::Windowed);
    case enigma::window::WindowModeRequestType::SetBorderlessFullscreen:
        return SetWindowMode(WindowMode::BorderlessFullscreen);
    }

    return false;
}

bool Window::SetWindowMode(WindowMode mode)
{
    WindowMode targetMode = mode;
    if (m_config.m_windowMode == targetMode)
    {
        return true;
    }

    auto windowHandle = static_cast<HWND>(m_windowHandle);
    if (!windowHandle)
    {
        return false;
    }

    const WindowMode oldMode = m_config.m_windowMode;
    if (targetMode == WindowMode::BorderlessFullscreen && oldMode == WindowMode::Windowed)
    {
        SaveWindowedPlacementIfNeeded();
    }

    m_config.m_windowMode = targetMode;

    bool             success = false;
    switch (targetMode)
    {
    case WindowMode::Windowed:
        success = ApplyWindowedMode();
        break;
    case WindowMode::BorderlessFullscreen:
        success = ApplyBorderlessFullscreenMode();
        break;
    default:
        m_config.m_windowMode = oldMode;
        return false;
    }

    if (!success)
    {
        m_config.m_windowMode = oldMode;
        return false;
    }

    m_isMinimized = IsIconic(windowHandle) != FALSE;
    m_lastClientDimensions = GetClientDimensions();
    BroadcastWindowModeChanged(oldMode, targetMode);
    return true;
}

void Window::HandleClientSizeChanged(void* platformWindowHandle, unsigned int sizeType, int width, int height)
{
    const IntVec2 newClientSize(width > 0 ? width : 0, height > 0 ? height : 0);
    const IntVec2 oldClientSize = m_lastClientDimensions;
    const bool    wasMinimized  = m_isMinimized;

    enigma::window::WindowResizeReason reason = enigma::window::WindowResizeReason::ClientSizeChanged;
    switch (sizeType)
    {
    case SIZE_MINIMIZED:
        reason        = enigma::window::WindowResizeReason::Minimized;
        m_isMinimized = true;
        break;
    case SIZE_MAXIMIZED:
        reason        = enigma::window::WindowResizeReason::Maximized;
        m_isMinimized = false;
        break;
    case SIZE_RESTORED:
        reason        = wasMinimized ? enigma::window::WindowResizeReason::Restored : enigma::window::WindowResizeReason::UserResize;
        m_isMinimized = false;
        break;
    default:
        m_isMinimized = false;
        break;
    }

    if (oldClientSize == newClientSize && wasMinimized == m_isMinimized)
    {
        return;
    }

    m_lastClientDimensions = newClientSize;

    enigma::window::WindowClientSizeEvent event;
    event.windowHandle  = platformWindowHandle;
    event.oldClientSize = oldClientSize;
    event.newClientSize = newClientSize;
    event.mode          = m_config.m_windowMode;
    event.reason        = reason;
    event.isMinimized   = m_isMinimized;
    event.isRestored    = !m_isMinimized && wasMinimized;

    enigma::window::WindowEvents::OnClientSizeChanged.Broadcast(event);
}

void Window::RequestClose(enigma::window::WindowCloseReason reason)
{
    enigma::window::WindowCloseRequest request;
    request.windowHandle = m_windowHandle;
    request.reason       = reason;

    enigma::window::WindowEvents::OnWindowCloseRequested.Broadcast(request);
}

bool Window::ApplyWindowedMode()
{
    auto windowHandle = static_cast<HWND>(m_windowHandle);
    if (!windowHandle)
    {
        return false;
    }

    ClipCursor(nullptr);
    ShowWindow(windowHandle, SW_RESTORE);

    const DWORD windowStyleFlags = WS_OVERLAPPEDWINDOW;
    const DWORD windowStyleExFlags = WS_EX_APPWINDOW;
    SetWindowLongPtr(windowHandle, GWL_STYLE, static_cast<LONG_PTR>(windowStyleFlags));
    SetWindowLongPtr(windowHandle, GWL_EXSTYLE, static_cast<LONG_PTR>(windowStyleExFlags));

    RECT windowRect = {};
    if (m_hasSavedWindowedPlacement)
    {
        windowRect.left = m_savedWindowedPosition.x;
        windowRect.top = m_savedWindowedPosition.y;
        windowRect.right = windowRect.left + m_savedWindowedDimensions.x;
        windowRect.bottom = windowRect.top + m_savedWindowedDimensions.y;
    }
    else
    {
        HMONITOR monitor = MonitorFromWindow(windowHandle, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo = {};
        monitorInfo.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(monitor, &monitorInfo);

        RECT clientRect = {};
        clientRect.right = m_config.m_resolution.x;
        clientRect.bottom = m_config.m_resolution.y;
        AdjustWindowRectEx(&clientRect, windowStyleFlags, FALSE, windowStyleExFlags);

        const int windowWidth = clientRect.right - clientRect.left;
        const int windowHeight = clientRect.bottom - clientRect.top;
        const int monitorWidth = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
        const int monitorHeight = monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;

        windowRect.left = monitorInfo.rcWork.left + (monitorWidth - windowWidth) / 2;
        windowRect.top = monitorInfo.rcWork.top + (monitorHeight - windowHeight) / 2;
        windowRect.right = windowRect.left + windowWidth;
        windowRect.bottom = windowRect.top + windowHeight;
    }

    HWND insertAfter = m_config.m_alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;
    return SetWindowPos(
        windowHandle,
        insertAfter,
        windowRect.left,
        windowRect.top,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW) != FALSE;
}

bool Window::ApplyBorderlessFullscreenMode()
{
    auto windowHandle = static_cast<HWND>(m_windowHandle);
    if (!windowHandle)
    {
        return false;
    }

    SaveWindowedPlacementIfNeeded();
    ShowWindow(windowHandle, SW_RESTORE);

    HMONITOR monitor = MonitorFromWindow(windowHandle, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(monitor, &monitorInfo))
    {
        return false;
    }

    const DWORD windowStyleFlags = WS_POPUP | WS_VISIBLE;
    const DWORD windowStyleExFlags = WS_EX_APPWINDOW;
    SetWindowLongPtr(windowHandle, GWL_STYLE, static_cast<LONG_PTR>(windowStyleFlags));
    SetWindowLongPtr(windowHandle, GWL_EXSTYLE, static_cast<LONG_PTR>(windowStyleExFlags));

    HWND insertAfter = m_config.m_alwaysOnTop ? HWND_TOPMOST : HWND_TOP;
    return SetWindowPos(
        windowHandle,
        insertAfter,
        monitorInfo.rcMonitor.left,
        monitorInfo.rcMonitor.top,
        monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
        monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW) != FALSE;
}

void Window::SaveWindowedPlacementIfNeeded()
{
    if (m_config.m_windowMode != WindowMode::Windowed)
    {
        return;
    }

    auto windowHandle = static_cast<HWND>(m_windowHandle);
    if (!windowHandle)
    {
        return;
    }

    RECT windowRect = {};
    if (!GetWindowRect(windowHandle, &windowRect))
    {
        return;
    }

    m_savedWindowedPosition = IntVec2(windowRect.left, windowRect.top);
    m_savedWindowedDimensions = IntVec2(windowRect.right - windowRect.left, windowRect.bottom - windowRect.top);
    m_hasSavedWindowedPlacement = true;
}

void Window::BroadcastWindowModeChanged(WindowMode oldMode, WindowMode newMode)
{
    enigma::window::WindowModeChangedEvent event;
    event.windowHandle = m_windowHandle;
    event.oldMode = oldMode;
    event.newMode = newMode;
    event.clientSize = GetClientDimensions();
    enigma::window::WindowEvents::OnWindowModeChanged.Broadcast(event);
}

//-----------------------------------------------------------------------------------------------
// #SD1ToDo: We will move this function to a more appropriate place later on... (Engine/Renderer/Window.cpp)
//
void Window::CreateOSWindow()
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HMODULE applicationInstanceHandle = ::GetModuleHandle(nullptr);

    // Define a window style/class
    WNDCLASSEX windowClassDescription;
    memset(&windowClassDescription, 0, sizeof(windowClassDescription));
    windowClassDescription.cbSize        = sizeof(windowClassDescription);
    windowClassDescription.style         = CS_OWNDC; // Redraw on move, request own Display Context
    windowClassDescription.lpfnWndProc   = static_cast<WNDPROC>(WindowsMessageHandlingProcedure);
    windowClassDescription.hInstance     = GetModuleHandle(nullptr);
    windowClassDescription.hIcon         = nullptr;
    windowClassDescription.hCursor       = nullptr;
    windowClassDescription.lpszClassName = TEXT("Simple Window Class");
    RegisterClassEx(&windowClassDescription);

    // Get the resolution of the main monitor - use a more accurate method
    int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    DebuggerPrintf("Primary display resolution: %dx%d\n", screenWidth, screenHeight);

    DWORD windowStyleFlags;
    DWORD windowStyleExFlags;
    RECT  windowRect;

    switch (m_config.m_windowMode)
    {
    case WindowMode::BorderlessFullscreen:
        DebuggerPrintf("Window mode: BorderlessFullscreen\n");
        CreateBorderlessFullscreenWindow(static_cast<float>(screenWidth), static_cast<float>(screenHeight), windowStyleFlags, windowStyleExFlags, windowRect);
        break;

    case WindowMode::Windowed:
    default:
        DebuggerPrintf("Window mode: Windowed\n");
        CreateWindowedWindow(static_cast<float>(screenWidth), static_cast<float>(screenHeight), windowStyleFlags, windowStyleExFlags, windowRect);
        break;
    }

    WCHAR windowTitle[1024];
    MultiByteToWideChar(GetACP(), 0, m_config.m_windowTitle.c_str(), -1, windowTitle,
                        sizeof(windowTitle) / sizeof(windowTitle[0]));

    DebuggerPrintf("Creating window with rect: %d,%d to %d,%d (size: %dx%d)\n",
                   windowRect.left, windowRect.top, windowRect.right, windowRect.bottom,
                   windowRect.right - windowRect.left, windowRect.bottom - windowRect.top);

    m_windowHandle = CreateWindowEx(
        windowStyleExFlags,
        windowClassDescription.lpszClassName,
        windowTitle,
        windowStyleFlags,
        windowRect.left,
        windowRect.top,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        applicationInstanceHandle,
        nullptr);

    auto windowHandle = static_cast<HWND>(m_windowHandle);

    if (!windowHandle)
    {
        DWORD error = GetLastError();
        DebuggerPrintf("ERROR: Failed to create window. Error code: %d\n", error);
        return;
    }

    ShowWindow(windowHandle, SW_SHOW);
    SetForegroundWindow(windowHandle);
    SetFocus(windowHandle);

    // Apply always-on-top setting if configured
    if (m_config.m_alwaysOnTop)
    {
        SetWindowPos(windowHandle, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        DebuggerPrintf("Window set to always-on-top as configured\n");
    }

    m_displayContext        = GetDC(windowHandle);
    m_lastClientDimensions = GetClientDimensions();
    m_isMinimized          = IsIconic(windowHandle) != FALSE;

    HCURSOR cursor = LoadCursor(nullptr, IDC_ARROW);
    SetCursor(cursor);
}

void Window::CreateBorderlessFullscreenWindow(float desktopWidth, float desktopHeight, DWORD& windowStyleFlags, DWORD& windowStyleExFlags, RECT& windowRect)
{
    // Borderless fullscreen mode - covers entire screen without border
    windowStyleFlags   = WS_POPUP | WS_VISIBLE;
    windowStyleExFlags = WS_EX_APPWINDOW;

    // Use the entire desktop area
    windowRect.left   = 0;
    windowRect.top    = 0;
    windowRect.right  = static_cast<int>(desktopWidth);
    windowRect.bottom = static_cast<int>(desktopHeight);
}

void Window::CreateWindowedWindow(float desktopWidth, float desktopHeight, DWORD& windowStyleFlags, DWORD& windowStyleExFlags, RECT& windowRect)
{
    // Regular windowed mode
    windowStyleFlags   = WS_OVERLAPPEDWINDOW;
    windowStyleExFlags = WS_EX_APPWINDOW;

    float clientAspect  = m_config.m_aspectRatio;
    float desktopAspect = desktopWidth / desktopHeight;

    // Use configured resolution or calculate based on desktop size
    float clientWidth, clientHeight;

    if (m_config.m_resolution.x > 0 && m_config.m_resolution.y > 0)
    {
        // Use configured resolution
        clientWidth  = static_cast<float>(m_config.m_resolution.x);
        clientHeight = static_cast<float>(m_config.m_resolution.y);

        // Ensure it fits on screen
        constexpr float maxClientFractionOfDesktop = 1.0f;
        float           maxWidth                   = desktopWidth * maxClientFractionOfDesktop;
        float           maxHeight                  = desktopHeight * maxClientFractionOfDesktop;

        if (clientWidth > maxWidth)
        {
            clientWidth  = maxWidth;
            clientHeight = clientWidth / clientAspect;
        }
        if (clientHeight > maxHeight)
        {
            clientHeight = maxHeight;
            clientWidth  = clientHeight * clientAspect;
        }
    }
    else
    {
        // Calculate maximum client size (as some % of desktop size)
        constexpr float maxClientFractionOfDesktop = 0.90f;
        clientWidth                                = desktopWidth * maxClientFractionOfDesktop;
        clientHeight                               = desktopHeight * maxClientFractionOfDesktop;

        if (clientAspect > desktopAspect)
        {
            clientHeight = clientWidth / clientAspect;
        }
        else
        {
            clientWidth = clientHeight * clientAspect;
        }
    }

    // Calculate client rect bounds by centering the client area
    float clientMarginX = 0.5f * (desktopWidth - clientWidth);
    float clientMarginY = 0.5f * (desktopHeight - clientHeight);
    RECT  clientRect;
    clientRect.left   = static_cast<int>(clientMarginX);
    clientRect.right  = clientRect.left + static_cast<int>(clientWidth);
    clientRect.top    = static_cast<int>(clientMarginY);
    clientRect.bottom = clientRect.top + static_cast<int>(clientHeight);

    // Calculate the outer dimensions of the physical window, including frame et. al.
    windowRect = clientRect;
    AdjustWindowRectEx(&windowRect, windowStyleFlags, FALSE, windowStyleExFlags);
}

//-----------------------------------------------------------------------------------------------
void Window::RegisterMessagePreprocessor(enigma::window::IWindowsMessagePreprocessor* preprocessor)
{
    if (!preprocessor)
    {
        DebuggerPrintf("[Window] Warning: Attempted to register null preprocessor\n");
        return;
    }

    auto it = std::find(m_messagePreprocessors.begin(), m_messagePreprocessors.end(), preprocessor);
    if (it != m_messagePreprocessors.end())
    {
        DebuggerPrintf("[Window] Warning: Preprocessor '%s' already registered\n", preprocessor->GetName());
        return;
    }

    m_messagePreprocessors.push_back(preprocessor);
    SortPreprocessors();

    DebuggerPrintf("[Window] Registered message preprocessor: %s (priority: %d)\n",
                   preprocessor->GetName(), preprocessor->GetPriority());
}

void Window::UnregisterMessagePreprocessor(enigma::window::IWindowsMessagePreprocessor* preprocessor)
{
    if (!preprocessor)
    {
        return;
    }

    auto it = std::find(m_messagePreprocessors.begin(), m_messagePreprocessors.end(), preprocessor);
    if (it != m_messagePreprocessors.end())
    {
        DebuggerPrintf("[Window] Unregistered message preprocessor: %s\n", preprocessor->GetName());
        m_messagePreprocessors.erase(it);
    }
}

void Window::SortPreprocessors()
{
    std::sort(m_messagePreprocessors.begin(), m_messagePreprocessors.end(),
              [](const enigma::window::IWindowsMessagePreprocessor* a, const enigma::window::IWindowsMessagePreprocessor* b)
              {
                  return a->GetPriority() < b->GetPriority();
              });
}
