#include "Window.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "IWindowsMessagePreprocessor.hpp"

// static variables
Window* Window::s_mainWindow = nullptr;

//-----------------------------------------------------------------------------------------------
// Handles Windows (Win32) messages/events; i.e. the OS is trying to tell us something happened.
// This function is called back by Windows whenever we tell it to (by calling DispatchMessage).

LRESULT CALLBACK WindowsMessageHandlingProcedure(HWND windowHandle, UINT wmMessageCode, WPARAM wParam, LPARAM lParam)
{
    // 优先调用消息预处理器链
    if (Window::s_mainWindow)
    {
        LRESULT result = 0;
        for (auto* preprocessor : Window::s_mainWindow->m_messagePreprocessors)
        {
            if (preprocessor->ProcessMessage(windowHandle, wmMessageCode, wParam, lParam, result))
            {
                // 消息已被此预处理器消费，立即返回
                return result;
            }
        }
    }

    // 如果没有预处理器消费消息，继续原有的InputSystem处理逻辑
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
    // App close requested via "X" button, or right-click "Close Window" on task bar, or "Close" from system menu, or Alt-F4
    case WM_CLOSE:
        {
            g_theEventSubsystem->FireStringEvent("WindowCloseEvent"); // Fire the WindowCloseEvent
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

            /*unsigned char asKey = static_cast<unsigned char>(wParam);
            if (input)
            {
                input->HandleKeyPressed(asKey);
            }

            break;*/
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
    if (m_config.m_windowMode == WindowMode::Fullscreen)
    {
        ClipCursor(nullptr);
        DebuggerPrintf("Mouse cursor clipping released\n");
    }

    // 清空预处理器列表(但不删除,因为生命周期由外部管理)
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
    return m_config.m_windowMode == WindowMode::Fullscreen ||
        m_config.m_windowMode == WindowMode::BorderlessFullscreen;
}

bool Window::IsInWindowedMode() const
{
    return m_config.m_windowMode == WindowMode::Windowed;
}

bool Window::IsAlwaysOnTop() const
{
    return m_config.m_alwaysOnTop;
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
    case WindowMode::Fullscreen:
        DebuggerPrintf("Window mode: Fullscreen\n");
        CreateFullscreenWindow(static_cast<float>(screenWidth), static_cast<float>(screenHeight), windowStyleFlags, windowStyleExFlags, windowRect);
        break;

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

    // For full screen mode, make sure the window is at the top and limit the mouse
    if (m_config.m_windowMode == WindowMode::Fullscreen)
    {
        // Force window to front desk and top floor
        SetWindowPos(windowHandle, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

        RECT windowClientRect;
        GetClientRect(windowHandle, &windowClientRect);

        // Convert to screen coordinates
        POINT topLeft     = {windowClientRect.left, windowClientRect.top};
        POINT bottomRight = {windowClientRect.right, windowClientRect.bottom};
        ClientToScreen(windowHandle, &topLeft);
        ClientToScreen(windowHandle, &bottomRight);

        RECT clipRect = {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
        ClipCursor(&clipRect);

        DebuggerPrintf("Mouse cursor clipped to fullscreen window: %d,%d to %d,%d\n",
                       clipRect.left, clipRect.top, clipRect.right, clipRect.bottom);
    }

    m_displayContext = GetDC(windowHandle);

    HCURSOR cursor = LoadCursor(nullptr, IDC_ARROW);
    SetCursor(cursor);
}

void Window::CreateFullscreenWindow(float desktopWidth, float desktopHeight, DWORD& windowStyleFlags, DWORD& windowStyleExFlags, RECT& windowRect)
{
    DebuggerPrintf("Creating exclusive fullscreen window\n");

    // True exclusive full screen mode - use minimal window style
    windowStyleFlags   = WS_POPUP | WS_VISIBLE;
    windowStyleExFlags = WS_EX_APPWINDOW | WS_EX_TOPMOST;

    // Get the exact resolution of the main monitor
    HDC hdc          = GetDC(nullptr);
    int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    ReleaseDC(nullptr, hdc);

    DebuggerPrintf("System metrics - Screen: %dx%d, Desktop: %.0fx%.0f\n",
                   screenWidth, screenHeight, desktopWidth, desktopHeight);

    // Use system metrics to precise screen size
    windowRect.left   = 0;
    windowRect.top    = 0;
    windowRect.right  = screenWidth;
    windowRect.bottom = screenHeight;

    DebuggerPrintf("Fullscreen window rect: left=%d, top=%d, right=%d, bottom=%d\n",
                   windowRect.left, windowRect.top, windowRect.right, windowRect.bottom);
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
    windowStyleFlags   = WS_CAPTION | WS_BORDER | WS_SYSMENU | WS_OVERLAPPED;
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
// 消息预处理器管理

void Window::RegisterMessagePreprocessor(enigma::window::IWindowsMessagePreprocessor* preprocessor)
{
    if (!preprocessor)
    {
        DebuggerPrintf("[Window] Warning: Attempted to register null preprocessor\n");
        return;
    }

    // 检查是否已注册
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
