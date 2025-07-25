#pragma once
#include "AnalogJoystick.hpp"
#include "Engine/Input/KeyButtonState.hpp"
#include "Engine/Input/XboxButtonID.hpp"

class AnalogJoystick;

class XboxController
{
    friend class InputSystem;

public:
    XboxController();
    ~XboxController();

    bool                  IsConnected() const;
    int                   GetControllerId() const;
    const AnalogJoystick& GetLeftStick() const;
    const AnalogJoystick& GetRightStick() const;
    float                 GetLeftTrigger() const;
    float                 GetRightTrigger() const;
    const KeyButtonState& GetButton(XboxButtonID buttonID);
    bool                  IsButtonDown(XboxButtonID buttonID) const;
    bool                  WasButtonJustPressed(XboxButtonID buttonID) const;
    bool                  WasButtonJustReleased(XboxButtonID buttonID) const;

private:
    // If connected, each XboxController should update the state of its 14 buttons, 2 triggers, and 2 joysticks.
    void Update();
    // XboxController::Update() should check if the controller is connected; if not, reset the joysticks, buttons, triggers, etc.
    void Reset();
    void UpdateJoystick(AnalogJoystick& out_joystick, short rawX, short rawY);
    void UpdateTrigger(float& out_triggerValue, unsigned char rawValue);
    void UpdateButton(int buttonID, unsigned short buttonFlags, unsigned short buttonFlag);

    int            m_id           = -1;
    bool           m_isConnected  = false;
    float          m_leftTrigger  = 0.0f;
    float          m_rightTrigger = 0.0f;
    KeyButtonState m_buttons[static_cast<int>(NUM)];
    AnalogJoystick m_leftStick;
    AnalogJoystick m_rightStick;
};
