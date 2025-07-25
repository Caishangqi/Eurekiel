#pragma once
#include "Engine/Math/Vec2.hpp"


class AnalogJoystick
{
public:
    AnalogJoystick();

    Vec2  GetPosition() const;
    float GetMagnitude() const;
    float GetOrientationDegrees() const;

    Vec2  GetRawUncorrectedPosition() const;
    float GetInnerDeadZoneFraction() const;
    float GetOuterDeadZoneFraction() const;

    // called by XboxController
    void Reset();
    void SetDeadZoneThresholds(float normalizedInnerDeadzoneThreshold, float normalizedOuterDeadzoneThreshold);
    void UpdatePosition(float rawNormalizedX, float rawNormalizedY);

protected:
    Vec2  m_rawPosition;
    Vec2  m_correctedPosition; // Deadzone-corrected m_position
    float m_innerDeadZoneFraction = 0.0f; // if R < this % . R = 0; "input range start" for corrective range map
    float m_outerDeadZoneFraction = 1.0f; // if R > this % . R = 1; "input range end" for corrective range map
};
