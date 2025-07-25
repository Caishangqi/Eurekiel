#include "Window.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Input/InputSystem.hpp"
// static variables
Window* Window::s_mainWindow = nullptr;

//-----------------------------------------------------------------------------------------------
// Handles Windows (Win32) messages/events; i.e. the OS is trying to tell us something happened.
// This function is called back by Windows whenever we tell it to (by calling DispatchMessage).

LRESULT CALLBACK WindowsMessageHandlingProcedure(HWND windowHandle, UINT wmMessageCode, WPARAM wParam, LPARAM lParam)
{
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
            g_theEventSystem->FireEvent("WindowCloseEvent"); // Fire the WindowCloseEvent
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
            // 处理鼠标左键按下事件
            if (input)
                input->HandleMouseButtonPressed(KEYCODE_LEFT_MOUSE);
            break;
        }
    case WM_LBUTTONUP:
        {
            // 处理鼠标左键释放事件
            if (input)
                input->HandleMouseButtonReleased(KEYCODE_LEFT_MOUSE);
            break;
        }
    case WM_RBUTTONDOWN:
        {
            // 处理鼠标右键按下事件
            if (input)
                input->HandleMouseButtonPressed(KEYCODE_RIGHT_MOUSE);
            break;
        }
    case WM_RBUTTONUP:
        {
            // 处理鼠标右键释放事件
            if (input)
                input->HandleMouseButtonReleased(KEYCODE_RIGHT_MOUSE);
            break;
        }
    case WM_MOUSEMOVE:
        {
            // 获取鼠标的屏幕坐标
            int mouseX = LOWORD(lParam);
            int mouseY = HIWORD(lParam);
            if (input)
                input->HandleMouseMove(mouseX, mouseY);
            break;
        }
    case WM_MOUSEWHEEL:
        {
            // 处理鼠标滚轮事件
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


//-----------------------------------------------------------------------------------------------
// #SD1ToDo: We will move this function to a more appropriate place later on... (Engine/Renderer/Window.cpp)
//
void Window::CreateOSWindow()
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HMODULE applicationInstanceHandle = ::GetModuleHandle(nullptr);
    float   clientAspect              = m_config.m_aspectRatio;


    // Define a window style/class
    WNDCLASSEX windowClassDescription;
    memset(&windowClassDescription, 0, sizeof(windowClassDescription));
    windowClassDescription.cbSize      = sizeof(windowClassDescription);
    windowClassDescription.style       = CS_OWNDC; // Redraw on move, request own Display Context
    windowClassDescription.lpfnWndProc = static_cast<WNDPROC>(WindowsMessageHandlingProcedure);
    // long pointer of windows proc
    // Register our Windows message-handling function
    windowClassDescription.hInstance     = GetModuleHandle(nullptr);
    windowClassDescription.hIcon         = nullptr;
    windowClassDescription.hCursor       = nullptr;
    windowClassDescription.lpszClassName = TEXT("Simple Window Class");
    RegisterClassEx(&windowClassDescription);

    // #SD1ToDo: Add support for fullscreen mode (requires different window style flags than windowed mode)
    constexpr DWORD windowStyleFlags = WS_CAPTION | WS_BORDER | WS_SYSMENU | WS_OVERLAPPED;
    // Remove WS_THICKFRAME that do not allow user to drag the window (resize)
    //constexpr DWORD windowStyleFlags   = WS_CAPTION | WS_BORDER | WS_THICKFRAME | WS_SYSMENU | WS_OVERLAPPED;
    constexpr DWORD windowStyleExFlags = WS_EX_APPWINDOW;

    // Get desktop rect, dimensions, aspect
    RECT desktopRect;
    HWND desktopWindowHandle = GetDesktopWindow();
    GetClientRect(desktopWindowHandle, &desktopRect);
    float desktopWidth  = static_cast<float>(desktopRect.right - desktopRect.left);
    float desktopHeight = static_cast<float>(desktopRect.bottom - desktopRect.top);
    float desktopAspect = desktopWidth / desktopHeight;

    // Calculate maximum client size (as some % of desktop size)
    constexpr float maxClientFractionOfDesktop = 0.90f;
    float           clientWidth                = desktopWidth * maxClientFractionOfDesktop;
    float           clientHeight               = desktopHeight * maxClientFractionOfDesktop;

    if (clientAspect > desktopAspect)
    {
        // Client window has a wider aspect than desktop; shrink client height to match its width
        clientHeight = clientWidth / clientAspect;
    }
    else
    {
        // Client window has a taller aspect than desktop; shrink client width to match its height
        clientWidth = clientHeight * clientAspect;
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
    RECT windowRect = clientRect;
    AdjustWindowRectEx(&windowRect, windowStyleFlags, FALSE, windowStyleExFlags);

    WCHAR windowTitle[1024];
    MultiByteToWideChar(GetACP(), 0, m_config.m_windowTitle.c_str(), -1, windowTitle,
                        sizeof(windowTitle) / sizeof(windowTitle[0]));
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

    ShowWindow(windowHandle, SW_SHOW);
    SetForegroundWindow(windowHandle);
    SetFocus(windowHandle);

    m_displayContext = GetDC(windowHandle);

    HCURSOR cursor = LoadCursor(nullptr, IDC_ARROW);
    SetCursor(cursor);
}
