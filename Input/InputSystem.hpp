#pragma once
#include <windows.h>

#include "Engine/Input/XboxController.hpp"
/**
 * An InputSystem instance should be owned (created, managed, destroyed) by the App, much like Renderer.
 */
// Key constants definition
unsigned char const KEYCODE_F1    = VK_F1;
unsigned char const KEYCODE_F2    = VK_F2;
unsigned char const KEYCODE_F3    = VK_F3;
unsigned char const KEYCODE_F4    = VK_F4;
unsigned char const KEYCODE_F5    = VK_F5;
unsigned char const KEYCODE_F6    = VK_F6;
unsigned char const KEYCODE_F7    = VK_F7;
unsigned char const KEYCODE_F8    = VK_F8;
unsigned char const KEYCODE_F9    = VK_F9;
unsigned char const KEYCODE_F10   = VK_F10;
unsigned char const KEYCODE_F11   = VK_F11;
unsigned char const KEYCODE_ESC   = VK_ESCAPE;
unsigned char const KEYCODE_SPACE = VK_SPACE;
unsigned char const KEYCODE_ENTER = VK_RETURN;

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
    XboxController const& GetController(unsigned char controllerIndex);

protected:
    KeyButtonState m_keyStates[NUM_KEYCODES];
    XboxController m_controllers[NUM_XBOX_CONTROLLERS];
};
