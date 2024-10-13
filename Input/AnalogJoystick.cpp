#include "Engine/Input/AnalogJoystick.hpp"

#include <cmath>
#include "Engine/Math/MathUtils.hpp"

AnalogJoystick::AnalogJoystick(): m_rawPosition(Vec2()), m_correctedPosition(Vec2()), m_innerDeadZoneFraction(0.3f),
                                  m_outerDeadZoneFraction(0.95f)
{
}

Vec2 AnalogJoystick::GetPosition() const
{
    return m_correctedPosition;
}

float AnalogJoystick::GetMagnitude() const
{
    // printf("x: %f   y: %f\n", m_correctedPosition.x, m_correctedPosition.y);
    return GetPosition().GetLength();
}

Vec2 AnalogJoystick::GetRawUncorrectedPosition() const
{
    return m_rawPosition;
}

float AnalogJoystick::GetInnerDeadZoneFraction() const
{
    return m_innerDeadZoneFraction;
}

float AnalogJoystick::GetOuterDeadZoneFraction() const
{
    return m_outerDeadZoneFraction;
}

void AnalogJoystick::Reset()
{
    m_rawPosition       = Vec2();
    m_correctedPosition = Vec2();
}

void AnalogJoystick::SetDeadZoneThresholds(float normalizedInnerDeadzoneThreshold,
                                           float normalizedOuterDeadzoneThreshold)
{
    m_innerDeadZoneFraction = normalizedInnerDeadzoneThreshold;
    m_outerDeadZoneFraction = normalizedOuterDeadzoneThreshold;
}

void AnalogJoystick::UpdatePosition(float rawNormalizedX, float rawNormalizedY)
{
    //printf("x: %f   y: %f\n", m_rawPosition.x, m_rawPosition.y);

    m_rawPosition.x = rawNormalizedX;
    m_rawPosition.y = rawNormalizedY;

    // Turn to raw polar
    float length = sqrt(m_rawPosition.x * m_rawPosition.x + m_rawPosition.y * m_rawPosition.y); // x * x + y * y
    float theta  = Atan2Degrees(m_rawPosition.y, m_rawPosition.x);

    // Clamped polar
    float lengthClamped = GetClamped(length, m_innerDeadZoneFraction, m_outerDeadZoneFraction);

    // corrected polar
    float lengthCorrected = RangeMap(lengthClamped, m_innerDeadZoneFraction, m_outerDeadZoneFraction, 0, 1);

    // convert to cardigan
    float convertX = lengthCorrected * cosf(ConvertDegreesToRadians(theta));
    float convertY = lengthCorrected * sinf(ConvertDegreesToRadians(theta));

    //printf("x: %f   y: %f\n", convertX, convertY);
    m_correctedPosition = Vec2(convertX, convertY);
}
