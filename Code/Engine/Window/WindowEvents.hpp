#pragma once

#include "Engine/Core/Event/MulticastDelegate.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Window/Window.hpp"

namespace enigma::window
{
    enum class WindowResizeReason
    {
        Unknown,
        ClientSizeChanged,
        UserResize,
        Minimized,
        Maximized,
        Restored,
        ModeChanged
    };

    enum class WindowModeRequestType
    {
        ToggleFullscreen,
        SetWindowed,
        SetBorderlessFullscreen
    };

    enum class WindowCloseReason
    {
        CloseButton,
        AltF4,
        SystemMenu,
        ConsoleCommand,
        Unknown
    };

    struct WindowClientSizeEvent
    {
        void*              windowHandle  = nullptr;
        IntVec2            oldClientSize = IntVec2();
        IntVec2            newClientSize = IntVec2();
        WindowMode         mode          = WindowMode::Windowed;
        WindowResizeReason reason        = WindowResizeReason::Unknown;
        bool               isMinimized   = false;
        bool               isRestored    = false;
    };

    struct WindowModeChangedEvent
    {
        void*      windowHandle = nullptr;
        WindowMode oldMode      = WindowMode::Windowed;
        WindowMode newMode      = WindowMode::Windowed;
        IntVec2    clientSize   = IntVec2();
    };

    struct WindowModeRequest
    {
        WindowModeRequestType type   = WindowModeRequestType::ToggleFullscreen;
        const char*           source = nullptr;
    };

    struct WindowCloseRequest
    {
        void*             windowHandle = nullptr;
        WindowCloseReason reason       = WindowCloseReason::Unknown;
    };

    struct WindowEvents
    {
        static enigma::event::MulticastDelegate<const WindowClientSizeEvent&> OnClientSizeChanged;
        static enigma::event::MulticastDelegate<const WindowModeChangedEvent&> OnWindowModeChanged;
        static enigma::event::MulticastDelegate<const WindowCloseRequest&> OnWindowCloseRequested;
    };

    struct WindowModeRequestEvents
    {
        static enigma::event::MulticastDelegate<const WindowModeRequest&> OnWindowModeRequested;
    };
} // namespace enigma::window
