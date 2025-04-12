#include "Engine/Input/XboxController.hpp"
#include <Windows.h> // must #include Windows.h before #including Xinput.h
#include <Xinput.h> // include the Xinput API header file (interface)

#include "Engine/Math/MathUtils.hpp"

#pragma comment(lib, "xinput9_1_0.lib")

XboxController::XboxController()
{
    printf("InputSystem::XboxController     Create controller instance ready for connected\n");
}

XboxController::~XboxController()
{
}

bool XboxController::IsConnected() const
{
    return m_isConnected;
}

int XboxController::GetControllerId() const
{
    return m_id;
}

const AnalogJoystick& XboxController::GetLeftStick() const
{
    return m_leftStick;
}

const AnalogJoystick& XboxController::GetRightStick() const
{
    return m_rightStick;
}

float XboxController::GetLeftTrigger() const
{
    return m_leftTrigger;
}

float XboxController::GetRightTrigger() const
{
    return m_rightTrigger;
}

const KeyButtonState& XboxController::GetButton(XboxButtonID buttonID)
{
    return m_buttons[buttonID];
}

bool XboxController::IsButtonDown(XboxButtonID buttonID) const
{
    return m_buttons[buttonID].m_isPressed;
}

bool XboxController::WasButtonJustPressed(XboxButtonID buttonID) const
{
    bool isKeyDown        = m_buttons[buttonID].m_isPressed;
    bool isLastFramePress = m_buttons[buttonID].m_wasPressedLastFrame;
    return !isKeyDown && isLastFramePress;
}

bool XboxController::WasButtonJustReleased(XboxButtonID buttonID) const
{
    bool isKeyReleased       = !m_buttons[buttonID].m_isPressed;
    bool isLastFrameReleased = !m_buttons[buttonID].m_wasPressedLastFrame;
    return !isKeyReleased && isLastFrameReleased;
}


void XboxController::Update()
{
    XINPUT_STATE xboxControllerState = {}; // Clear (zero-out) the controller state structure
    DWORD        result              = XInputGetState(m_id, &xboxControllerState); // Get fresh state info


    // Check if controller is connected
    if (XInputGetState(m_id, &xboxControllerState) == ERROR_SUCCESS)
    {
        if (m_isConnected == false)
        {
            printf("[INPUT]     Connect to Xbox controller [%d]\n", m_id);
        }
        m_isConnected = true;
    }

    else if (result == ERROR_DEVICE_NOT_CONNECTED) // Result if this controller ID# is disconnected
    {
        if (m_isConnected == true)
        {
            printf("[INPUT]     Disconnect to Xbox controller [%d]\n", m_id);
        }
        m_isConnected = false;
        // Reset controller state
        Reset();
    }
    else
    {
        printf("[INPUT]     Unknown Error in controller [%d]\n", m_id);
    }

    if (m_isConnected)
    {
        const XINPUT_GAMEPAD& state = xboxControllerState.Gamepad;

        UpdateJoystick(m_leftStick, state.sThumbLX, state.sThumbLY);
        UpdateJoystick(m_rightStick, state.sThumbRX, state.sThumbRY);

        UpdateTrigger(m_leftTrigger, state.bLeftTrigger);
        UpdateTrigger(m_rightTrigger, state.bRightTrigger);

        UpdateButton(XBOX_BUTTON_A, state.wButtons, XINPUT_GAMEPAD_A);
        UpdateButton(XBOX_BUTTON_B, state.wButtons, XINPUT_GAMEPAD_B);
        UpdateButton(XBOX_BUTTON_X, state.wButtons, XINPUT_GAMEPAD_X);
        UpdateButton(XBOX_BUTTON_Y, state.wButtons, XINPUT_GAMEPAD_Y);
        UpdateButton(XBOX_BUTTON_BACK, state.wButtons, XINPUT_GAMEPAD_BACK);
        UpdateButton(XBOX_BUTTON_START, state.wButtons, XINPUT_GAMEPAD_START);
        UpdateButton(XBOX_BUTTON_LS, state.wButtons,XINPUT_GAMEPAD_LEFT_SHOULDER);
        UpdateButton(XBOX_BUTTON_RS, state.wButtons,XINPUT_GAMEPAD_RIGHT_SHOULDER);
        UpdateButton(XBOX_BUTTON_LB, state.wButtons,XINPUT_GAMEPAD_LEFT_THUMB);
        UpdateButton(XBOX_BUTTON_RB, state.wButtons,XINPUT_GAMEPAD_RIGHT_THUMB);
        UpdateButton(XBOX_BUTTON_DPAD_RIGHT, state.wButtons,XINPUT_GAMEPAD_DPAD_RIGHT);
        UpdateButton(XBOX_BUTTON_DPAD_UP, state.wButtons,XINPUT_GAMEPAD_DPAD_UP);
        UpdateButton(XBOX_BUTTON_DPAD_LEFT, state.wButtons,XINPUT_GAMEPAD_DPAD_LEFT);
        UpdateButton(XBOX_BUTTON_DPAD_DOWN, state.wButtons,XINPUT_GAMEPAD_DPAD_DOWN);
    }
}

void XboxController::Reset()
{
    for (KeyButtonState& m_button : m_buttons)
    {
        m_button.m_isPressed           = false;
        m_button.m_wasPressedLastFrame = false;
    }

    m_leftTrigger  = 0.f;
    m_rightTrigger = 0.f;

    m_rightStick.Reset();
    m_leftStick.Reset();
}

void XboxController::UpdateJoystick(AnalogJoystick& out_joystick, short rawX, short rawY)
{
    float rawNormalizedX = RangeMap(rawX, -32768, 32767, -1, 1);
    float rawNormalizedY = RangeMap(rawY, -32768, 32767, -1, 1);
    out_joystick.UpdatePosition(rawNormalizedX, rawNormalizedY);
}

void XboxController::UpdateTrigger(float& out_triggerValue, unsigned char rawValue)
{
    out_triggerValue = rawValue;
}

void XboxController::UpdateButton(int buttonID, unsigned short buttonFlags, unsigned short buttonFlag)
{
    //printf("[INPUT]     Update button [%d]\n", buttonID);
    KeyButtonState& button       = m_buttons[buttonID];
    button.m_wasPressedLastFrame = button.m_isPressed;
    // bitwise operation
    m_buttons[buttonID].m_isPressed = ((buttonFlags & buttonFlag) == buttonFlag);
}
