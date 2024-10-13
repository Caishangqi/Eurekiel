#pragma once
#include <windows.h>
#include "Engine/Input/XboxController.hpp"
/**
 * An InputSystem instance should be owned (created, managed, destroyed) by the App, much like Renderer.
 */
// Key constants definition
constexpr unsigned char KEYCODE_F1    = VK_F1;
constexpr unsigned char KEYCODE_F2    = VK_F2;
constexpr unsigned char KEYCODE_F3    = VK_F3;
constexpr unsigned char KEYCODE_F4    = VK_F4;
constexpr unsigned char KEYCODE_F5    = VK_F5;
constexpr unsigned char KEYCODE_F6    = VK_F6;
constexpr unsigned char KEYCODE_F7    = VK_F7;
constexpr unsigned char KEYCODE_F8    = VK_F8;
constexpr unsigned char KEYCODE_F9    = VK_F9;
constexpr unsigned char KEYCODE_F10   = VK_F10;
constexpr unsigned char KEYCODE_F11   = VK_F11;
constexpr unsigned char KEYCODE_ESC   = VK_ESCAPE;
constexpr unsigned char KEYCODE_SPACE = VK_SPACE;
constexpr unsigned char KEYCODE_ENTER = VK_RETURN;

//
constexpr int NUM_KEYCODES         = 256;
constexpr int NUM_XBOX_CONTROLLERS = 4;

class InputSystem
{
public:
    InputSystem();
    ~InputSystem();

    void Startup();
    void Shutdown();
    void BeginFrame();
    void EndFrame();

    bool                  WasKeyJustPressed(unsigned char keyCode);
    bool                  WasKeyJustReleased(unsigned char keyCode);
    bool                  IsKeyDown(unsigned char keyCode);
    void                  HandleKeyPressed(unsigned char keyCode);
    void                  HandleKeyReleased(unsigned char keyCode);
    const XboxController& GetController(unsigned char controllerIndex);

    void HandleMouseButtonPressed(int button);
    void HandleMouseButtonReleased(int button);
    void HandleMouseMove(int mouseX, int mouseY);
    void HandleMouseWheel(short wheelDelta);

    bool  IsMouseButtonDown(int button) const;
    bool  WasMouseButtonJustPressed(int button) const;
    Vec2  GetMousePosition() const;
    Vec2  GetMousePositionOnWorld(Vec2& cameraBottomLeft, Vec2& cameraTopRight) const;
    short GetMouseWheelDelta() const;

    HWND hWnd;

protected:
    KeyButtonState m_keyStates[NUM_KEYCODES];
    XboxController m_controllers[NUM_XBOX_CONTROLLERS];

    KeyButtonState m_buttonStates[3];

    Vec2  m_mousePosition   = Vec2(0.f, 0.f);
    short m_mouseWheelDelta = 0;
};
