#include "Engine/Input/InputSystem.hpp"
#include "stdio.h"

InputSystem::InputSystem()
{
}

InputSystem::~InputSystem()
{
}

void InputSystem::Startup()
{
    for (int controllerID = 0; controllerID < NUM_XBOX_CONTROLLERS; ++controllerID)
    {
        m_controllers[controllerID].m_id = controllerID;
    }
    printf("[input]     Initialize input system\n");
}

void InputSystem::Shutdown()
{
}

// Immediately use get the button and use in Update()
void InputSystem::BeginFrame()
{
    for (XboxController& m_controller : m_controllers)
    {
        m_controller.Update();
    }
}

void InputSystem::EndFrame()
{
    for (int keyIndex = 0; keyIndex < NUM_KEYCODES; ++keyIndex)
    {
        m_keyStates[keyIndex].m_wasPressedLastFrame = m_keyStates[keyIndex].m_isPressed;
    }
}

bool InputSystem::WasKeyJustPressed(unsigned char keyCode)
{
    bool isKeyDown        = m_keyStates[keyCode].m_isPressed;
    bool isLastFramePress = m_keyStates[keyCode].m_wasPressedLastFrame;
    return !isKeyDown && isLastFramePress;
}

bool InputSystem::WasKeyJustReleased(unsigned char keyCode)
{
    bool isKeyReleased       = !m_keyStates[keyCode].m_isPressed;
    bool isLastFrameReleased = !m_keyStates[keyCode].m_wasPressedLastFrame;
    return !isKeyReleased && isLastFrameReleased;
}

bool InputSystem::IsKeyDown(unsigned char keyCode)
{
    return m_keyStates[keyCode].m_isPressed;
}

void InputSystem::HandleKeyPressed(unsigned char keyCode)
{
    m_keyStates[keyCode].m_isPressed = true;
}

void InputSystem::HandleKeyReleased(unsigned char keyCode)
{
    m_keyStates[keyCode].m_isPressed = false;
}

XboxController const& InputSystem::GetController(unsigned char controllerIndex)
{
    return m_controllers[controllerIndex];
}
