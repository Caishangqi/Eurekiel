// XboxButtonID.hpp
#pragma once

enum XboxButtonID
{
    // Enum make the first element is "0" unless your override it
    XBOX_BUTTON_INVALID = -1,
    // Each element will automatically increment
    XBOX_BUTTON_A, // Button A
    XBOX_BUTTON_B, // Button B
    XBOX_BUTTON_X, // Button X
    XBOX_BUTTON_Y, // Button Y
    XBOX_BUTTON_DPAD_UP, // D-Pad Up
    XBOX_BUTTON_DPAD_DOWN, // D-Pad Down
    XBOX_BUTTON_DPAD_LEFT, // D-Pad Left
    XBOX_BUTTON_DPAD_RIGHT, // D-Pad Right
    XBOX_BUTTON_LB, // Left Bumper
    XBOX_BUTTON_RB, // Right Bumper
    XBOX_BUTTON_START, // Start Button
    XBOX_BUTTON_BACK, // Back Button
    XBOX_BUTTON_LS, // Left Stick (pressed as a button)
    XBOX_BUTTON_RS, // Right Stick (pressed as a button)

    NUM // Total number of buttons
};
