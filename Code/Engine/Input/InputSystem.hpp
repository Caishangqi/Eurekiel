#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "Engine/Core/Event/StringEventBus.hpp"
#include "Engine/Input/XboxController.hpp"
#include "Engine/Math/IntVec2.hpp"

// Import EventArgs type alias for legacy compatibility
using enigma::event::EventArgs;

/**
 * An InputSystem instance should be owned (created, managed, destroyed) by the App, much like Renderer.
 */
// Key constants definition
constexpr unsigned char KEYCODE_F1           = VK_F1;
constexpr unsigned char KEYCODE_F2           = VK_F2;
constexpr unsigned char KEYCODE_F3           = VK_F3;
constexpr unsigned char KEYCODE_F4           = VK_F4;
constexpr unsigned char KEYCODE_F5           = VK_F5;
constexpr unsigned char KEYCODE_F6           = VK_F6;
constexpr unsigned char KEYCODE_F7           = VK_F7;
constexpr unsigned char KEYCODE_F8           = VK_F8;
constexpr unsigned char KEYCODE_F9           = VK_F9;
constexpr unsigned char KEYCODE_F10          = VK_F10;
constexpr unsigned char KEYCODE_F11          = VK_F11;
constexpr unsigned char KEYCODE_ESC          = VK_ESCAPE;
constexpr unsigned char KEYCODE_SPACE        = VK_SPACE;
constexpr unsigned char KEYCODE_ENTER        = VK_RETURN;
constexpr unsigned char KEYCODE_UPARROW      = VK_UP;
constexpr unsigned char KEYCODE_DOWNARROW    = VK_DOWN;
constexpr unsigned char KEYCODE_LEFTARROW    = VK_LEFT;
constexpr unsigned char KEYCODE_RIGHTARROW   = VK_RIGHT;
constexpr unsigned char KEYCODE_LEFT_MOUSE   = VK_LBUTTON;
constexpr unsigned char KEYCODE_RIGHT_MOUSE  = VK_RBUTTON;
constexpr unsigned char KEYCODE_LEFT_CTRL    = VK_CONTROL; // TODO: Fix Left ctrl
constexpr unsigned char KEYCODE_RIGHT_CTRL   = VK_CONTROL; // TODO: Fix Right ctrl
constexpr unsigned char KEYCODE_LEFTBRACKET  = 0xDB;
constexpr unsigned char KEYCODE_RIGHTBRACKET = 0xDD;
constexpr unsigned char KEYCODE_TILDE        = 0xC0;
constexpr unsigned char KEYCODE_BACKSPACE    = VK_BACK;
constexpr unsigned char KEYCODE_INSERT       = VK_INSERT;
constexpr unsigned char KEYCODE_DELETE       = VK_DELETE;
constexpr unsigned char KEYCODE_HOME         = VK_HOME;
constexpr unsigned char KEYCODE_END          = VK_END;
constexpr unsigned char KEYCODE_LEFT_SHIFT   = VK_SHIFT;

//
constexpr int NUM_KEYCODES         = 256;
constexpr int NUM_XBOX_CONTROLLERS = 4;

/// Definition

enum class CursorMode
{
    POINTER,
    FPS
};

struct CursorState
{
    IntVec2 m_cursorClientDelta;
    IntVec2 m_cursorClientPosition;

    CursorMode m_cursorMode = CursorMode::POINTER;
};

struct InputSystemConfig
{
};

class InputSystem
{
    /// Event
public:
    static bool Event_KeyPressed(EventArgs& args);
    static bool Event_KeyReleased(EventArgs& args);
    /// 
    InputSystem(const InputSystemConfig& config);
    ~InputSystem();

    void Startup();
    void Shutdown();
    void BeginFrame();
    void EndFrame();

    /// Cursor
    // In pointer mode, the cursor should be visible, freely able to move, and not recentered.
    // In FPS mode, the cursor should be hidden, reset to the center of the window each frame,
    // and record the delta each frame.
    void       SetCursorMode(CursorMode cursorMode);
    CursorMode GetCursorMode() const;

    /// Returns the current frame cursor delta in pixels, relative to the client region.
    /// This is how much the cursor moved last frame before it was reset to the center of the screen.
    /// Only valid in FPS mode, will be zero otherwise.
    Vec2 GetCursorClientDelta() const;

    /// Returns the cursor position, in pixels relative to the client region.
    Vec2 GetCursorClientPosition() const;

    /// Returns the cursor position, normalized to the range [0,1], relative to the client region,
    /// with the y-axis inverted to map from Windows coordinates to game screen camera conventions
    Vec2 GetCursorNormalizedPosition() const;
    /// 

    bool                  WasKeyJustPressed(unsigned char keyCode);
    bool                  WasKeyJustReleased(unsigned char keyCode);
    bool                  IsKeyDown(unsigned char keyCode);
    void                  HandleKeyPressed(unsigned char keyCode);
    void                  HandleKeyReleased(unsigned char keyCode);
    const XboxController& GetController(unsigned char controllerIndex);

    void HandleMouseButtonPressed(unsigned char keyCode);
    void HandleMouseButtonReleased(unsigned char keyCode);
    void HandleMouseMove(int mouseX, int mouseY);
    void HandleMouseWheel(short wheelDelta);

    bool  IsMouseButtonDown(unsigned char keyCode) const;
    bool  WasMouseButtonJustPressed(unsigned char keyCode) const;
    Vec2  GetMousePosition() const;
    Vec2  GetMousePositionOnWorld(Vec2& cameraBottomLeft, Vec2& cameraTopRight) const;
    short GetMouseWheelDelta() const;

protected:
    KeyButtonState m_keyStates[NUM_KEYCODES];
    XboxController m_controllers[NUM_XBOX_CONTROLLERS];

    Vec2  m_mousePosition   = Vec2(0.f, 0.f);
    short m_mouseWheelDelta = 0;

    CursorState m_cursorState;
    bool        m_isCursorHidden = false;

private:
    InputSystemConfig m_inputConfig;
};
