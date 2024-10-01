#define WIN32_LEAN_AND_MEAN		// Always #define this before #including <windows.h>
#include <windows.h>			// #include this (massive, platform-specific) header in VERY few places (and .CPPs only)
#include <cassert>
#include <crtdbg.h>
#include <cstdio>
#include <iostream>

#include "Engine/Input/InputSystem.hpp"
#include "Game/App.hpp"


//-----------------------------------------------------------------------------------------------
// #SD1ToDo: Later we will move this useful macro to a more central place, e.g. Engine/Core/EngineCommon.hpp
//
#define UNUSED(x) (void)(x);

//-----------------------------------------------------------------------------------------------
// #SD1ToDo: This will eventually go away once we add a Window engine class later on.
// 
constexpr float CLIENT_ASPECT = 2.0f; // We are requesting a 1:1 aspect (square) window area

#define CONSOLE_HANDLER HANDLE

//-----------------------------------------------------------------------------------------------
// #SD1ToDo: We will move each of these items to its proper place, once that place is established later on
//

extern App* g_theApp;
extern HANDLE g_consoleHandle;

HWND g_hWnd = nullptr; // ...becomes void* Window::m_windowHandle
HDC g_displayDeviceContext = nullptr; // ...becomes void* Window::m_displayContext
auto APP_NAME = "SD1-A2: Starship Prototype"; // ...becomes ??? (Change this per project!)

//-----------------------------------------------------------------------------------------------
// Handles Windows (Win32) messages/events; i.e. the OS is trying to tell us something happened.
// This function is called back by Windows whenever we tell it to (by calling DispatchMessage).
//
// #SD1ToDo: Eventually, we will move this function to a more appropriate place (Engine/Renderer/Window.cpp) later on...
//
LRESULT CALLBACK WindowsMessageHandlingProcedure(HWND windowHandle, UINT wmMessageCode, WPARAM wParam, LPARAM lParam)
{
    switch (wmMessageCode)
    {
    // App close requested via "X" button, or right-click "Close Window" on task bar, or "Close" from system menu, or Alt-F4
    case WM_CLOSE:
        {
            g_theApp->HandleQuitRequested();
            return 0; // "Consumes" this message (tells Windows "okay, we handled it")
        }

    // Raw physical keyboard "key-was-just-depressed" event (case-insensitive, not translated)
    case WM_KEYDOWN:
        {
            unsigned char asKey = static_cast<unsigned char>(wParam);
            
            g_theInput->HandleKeyPressed(asKey);
            break;
        }

    // Raw physical keyboard "key-was-just-released" event (case-insensitive, not translated)
    case WM_KEYUP:
        {
            unsigned char asKey = static_cast<unsigned char>(wParam);

            
            g_theInput->HandleKeyReleased(asKey);

            break;
        }
    default: ;
    }

    // Send back to Windows any unhandled/unconsumed messages we want other apps to see (e.g. play/pause in music apps, etc.)
    return DefWindowProc(windowHandle, wmMessageCode, wParam, lParam);
}


//-----------------------------------------------------------------------------------------------
// #SD1ToDo: We will move this function to a more appropriate place later on... (Engine/Renderer/Window.cpp)
//
void CreateOSWindow(void* applicationInstanceHandle, float clientAspect)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Define a window style/class
    WNDCLASSEX windowClassDescription;
    memset(&windowClassDescription, 0, sizeof(windowClassDescription));
    windowClassDescription.cbSize = sizeof(windowClassDescription);
    windowClassDescription.style = CS_OWNDC; // Redraw on move, request own Display Context
    windowClassDescription.lpfnWndProc = static_cast<WNDPROC>(WindowsMessageHandlingProcedure);
    // Register our Windows message-handling function
    windowClassDescription.hInstance = GetModuleHandle(nullptr);
    windowClassDescription.hIcon = nullptr;
    windowClassDescription.hCursor = nullptr;
    windowClassDescription.lpszClassName = TEXT("Simple Window Class");
    RegisterClassEx(&windowClassDescription);

    // #SD1ToDo: Add support for fullscreen mode (requires different window style flags than windowed mode)
    constexpr DWORD windowStyleFlags = WS_CAPTION | WS_BORDER | WS_THICKFRAME | WS_SYSMENU | WS_OVERLAPPED;
    constexpr DWORD windowStyleExFlags = WS_EX_APPWINDOW;

    // Get desktop rect, dimensions, aspect
    RECT desktopRect;
    HWND desktopWindowHandle = GetDesktopWindow();
    GetClientRect(desktopWindowHandle, &desktopRect);
    float desktopWidth = static_cast<float>(desktopRect.right - desktopRect.left);
    float desktopHeight = static_cast<float>(desktopRect.bottom - desktopRect.top);
    float desktopAspect = desktopWidth / desktopHeight;

    // Calculate maximum client size (as some % of desktop size)
    constexpr float maxClientFractionOfDesktop = 0.90f;
    float clientWidth = desktopWidth * maxClientFractionOfDesktop;
    float clientHeight = desktopHeight * maxClientFractionOfDesktop;
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
    RECT clientRect;
    clientRect.left = static_cast<int>(clientMarginX);
    clientRect.right = clientRect.left + static_cast<int>(clientWidth);
    clientRect.top = static_cast<int>(clientMarginY);
    clientRect.bottom = clientRect.top + static_cast<int>(clientHeight);

    // Calculate the outer dimensions of the physical window, including frame et. al.
    RECT windowRect = clientRect;
    AdjustWindowRectEx(&windowRect, windowStyleFlags, FALSE, windowStyleExFlags);

    WCHAR windowTitle[1024];
    MultiByteToWideChar(GetACP(), 0, APP_NAME, -1, windowTitle, sizeof(windowTitle) / sizeof(windowTitle[0]));
    g_hWnd = CreateWindowEx(
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
        static_cast<HINSTANCE>(applicationInstanceHandle),
        nullptr);

    ShowWindow(g_hWnd, SW_SHOW);
    SetForegroundWindow(g_hWnd);
    SetFocus(g_hWnd);

    g_displayDeviceContext = GetDC(g_hWnd);

    HCURSOR cursor = LoadCursor(nullptr, IDC_ARROW);
    SetCursor(cursor);
}


//-----------------------------------------------------------------------------------------------
// Processes all Windows messages (WM_xxx) for this app that have queued up since last frame.
// For each message in the queue, our WindowsMessageHandlingProcedure (or "WinProc") function
//	is called, telling us what happened (key up/down, minimized/restored, gained/lost focus, etc.)
//
// #SD1ToDo: Eventually, we will move this function to a more appropriate place later on... (Engine/Renderer/Window.cpp)
//
void RunMessagePump()
{
    MSG queuedMessage;
    for (;;)
    {
        const BOOL wasMessagePresent = PeekMessage(&queuedMessage, nullptr, 0, 0, PM_REMOVE);
        if (!wasMessagePresent)
        {
            break;
        }

        TranslateMessage(&queuedMessage);
        DispatchMessage(&queuedMessage);
        // This tells Windows to call our "WindowsMessageHandlingProcedure" (a.k.a. "WinProc") function
    }
}

#ifdef CONSOLE_HANDLER
HANDLE g_consoleHandle = nullptr; // 定义控制台句柄
void CreateConsole()
{
    // 分配控制台窗口
    AllocConsole();
    // 重定向标准输出、标准错误和标准输入到控制台
    FILE* stream;
    freopen_s(&stream, "CONOUT$", "w", stdout); // 标准输出
    freopen_s(&stream, "CONOUT$", "w", stderr); // 标准错误
    freopen_s(&stream, "CONIN$", "r", stdin); // 标准输入

    // 获取控制台句柄，允许通过 WinAPI 与控制台进行更多的交互
    g_consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    if (g_consoleHandle == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Failed to get console handle!" << '\n';
    }
    else
    {
        puts("[/] Initialize......");
        puts("   _____ _             _    _____ _     _        ");
        puts("  / ____| |           | |  / ____| |   (_)       ");
        puts(" | (___ | |_ __ _ _ __| |_| (___ | |__  _ _ __   ");
        puts("  \\___ \\| __/ _` | '__| __|\\___ \\| '_ \\| | '_ \\       Running v2.3-BETA (v1_20)");
        puts("  ____) | || (_| | |  | |_ ____) | | | | | |_) |      Platform Windows (11)");
        puts(" |_____/ \\__\\__,_|_|   \\__|_____/|_| |_|_| .__/  ");
        puts("                                         | |     ");
        puts("                                         |_|     ");
        puts("             Developed by Caizii                 ");
        puts("\n");
    }

    if (g_consoleHandle)
    {
        // 例如更改控制台文字颜色 (白色背景，蓝色文字)
        SetConsoleTextAttribute(g_consoleHandle, BACKGROUND_BLUE | FOREGROUND_INTENSITY);
    }
}
#endif
//-----------------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE applicationInstanceHandle, HINSTANCE, LPSTR commandLineString, int)
{
    UNUSED(commandLineString);

#ifdef CONSOLE_HANDLER
    CreateConsole();
#endif

    CreateOSWindow(applicationInstanceHandle, CLIENT_ASPECT);
    //CreateRenderingContext();


    // TheApp_Startup(applicationInstanceHandle, commandLineString); // This will get replaced with:
    // #SD1ToDo: g_theApp = new App();
    g_theApp = new App();
    // #SD1ToDo: g_theApp->Startup();
    g_theApp->Startup();

    // Program main loop; keep running frames until it's time to quit
    while (!g_theApp->IsQuitting()) // #SD1ToDo: ...becomes:  !g_theApp->IsQuitting()
    {
        //Sleep(16); // Temporary code to "slow down" our app to ~60Hz until we have proper frame timing in
        // #SD1ToDo: This call will move to Window::BeginFrame() once we have a Window engine system
        // Process OS messages (keyboard/mouse button clicked, application lost/gained focus, etc.)
        RunMessagePump(); // calls our own WindowsMessageHandlingProcedure() function for us!

        g_theApp->RunFrame();

        // #SD1ToDo: This call will move to Renderer::EndFrame() once we complete our Window refactor
    }

    // TheApp_Shutdown(); // This will get replaced with:
    // #SD1ToDo:	g_theApp->Shutdown();
    g_theApp->Shutdown();
    // #SD1ToDo:	delete g_theApp;
    delete g_theApp;
    // #SD1ToDo:	g_theApp = nullptr;
    g_theApp = nullptr;

    return 0;
}
