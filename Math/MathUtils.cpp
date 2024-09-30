#include "Engine/Math/MathUtils.hpp"
#include <cmath>

#include "Vec2.hpp"
#include "Vec3.hpp"

constexpr float PI = 3.14159265358979323846f;

float GetClamped(float value, float minValue, float maxValue)
{
    if (value < minValue)
    {
        return minValue;
    }
    else if (value > maxValue)
        return maxValue;
    else
        return value;
}

float GetClampedZeroToOne(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    else if (value > 1.0f)
    {
        return 1.0f;
    }
    else
    {
        return value;
    }
}

float Interpolate(float start, float end, float fractionTowardEnd)
{
    return start + (end - start) * fractionTowardEnd;
}

float GetFractionWithinRange(float value, float rangeStart, float rangeEnd)
{
    float range = rangeEnd - rangeStart;
    if (range == 0.f)
    {
        return 0.f;
    }
    return (value - rangeStart) / range;
}

float RangeMap(float inValue, float inStart, float inEnd, float outStart, float outEnd)
{
    float const t = GetFractionWithinRange(inValue, inStart, inEnd);
    return Interpolate(outStart, outEnd, t);
}

float RangeMapClamped(float inValue, float inStart, float inEnd, float outStart, float outEnd)
{
    float valueWithClamped = GetClamped(inValue, inStart, inEnd);
    return RangeMap(valueWithClamped, inStart, inEnd, outStart, outEnd);
}

int RoundDownToInt(float value)
{
    return static_cast<int>(floor(value));
}

float ConvertDegreesToRadians(float degrees)
{
    return degrees * (PI / 180.f);
}

float ConvertRadiansToDegrees(float radians)
{
    return radians * (180.f / PI);
}

float CosDegrees(float degrees)
{
    return cosf(ConvertDegreesToRadians(degrees));
}

float SinDegrees(float degrees)
{
    return sinf(ConvertDegreesToRadians(degrees));
}

float Atan2Degrees(float y, float x)
{
    return ConvertRadiansToDegrees(atan2f(y, x));
}

/**
 * @brief Calculates the shortest angular displacement in degrees between two given angles.
 * 
 * This function takes two angles in degrees, `startDegree` and `endDegree`, and calculates the shortest angular
 * displacement needed to move from `startDegree` to `endDegree`. The result is returned as a float value.
 * 
 * @param startDegree The initial angle in degrees.
 * @param endDegree The final angle in degrees.
 * @return The shortest angular displacement in degrees from `startDegree` to `endDegree`.
 */
float GetShortestAngularDispDegrees(float startDegree, float endDegree)
{
    float displacement = endDegree - startDegree;
    while (displacement > 180.f)
        displacement -= 360.f;


    while (displacement < -180.f)
        displacement += 360.f;

    return displacement;
}

/**
 * @brief Calculate the new degree value after turning towards a goal degree within a specified maximum degree per turn.
 * 
 * @param currentDegrees Current degree
 * @param goalDegrees The goal degree
 * @param maxDeltaDegrees Maximum degree that can be added per turn
 * @return float The new degree value after turning
 */
float GetTurnedTowardDegrees(float currentDegrees, float goalDegrees, float maxDeltaDegrees)
{
    // Calculate the angular displacement between current and goal degrees
    float angDisplacementDegree = GetShortestAngularDispDegrees(currentDegrees, goalDegrees);

    // Check if the absolute angular displacement is less than the maximum degree per turn
    if (fabs(angDisplacementDegree) < maxDeltaDegrees)
    {
        return goalDegrees;
    }

    // Adjust the current degree based on the direction of angular displacement
    if (angDisplacementDegree > 0.f)
        return currentDegrees + maxDeltaDegrees;
    else
    {
        return currentDegrees - maxDeltaDegrees;
    }
}

float DotProduct2D(Vec2 const& a, Vec2 const& b)
{
    return a.x * b.x + a.y * b.y;
}

float GetDistance2D(Vec2 const& positionA, Vec2 const& positionB)
{
    float deltaX = positionB.x - positionA.x;
    float deltaY = positionB.y - positionA.y;
    return sqrtf(deltaX * deltaX + deltaY * deltaY);
}

float GetDistanceSquared2D(Vec2 const& positionA, Vec2 const& positionB)
{
    float deltaX = positionB.x - positionA.x;
    float deltaY = positionB.y - positionA.y;
    return deltaX * deltaX + deltaY * deltaY;
}

float GetDistance3D(Vec3 const& positionA, Vec3 const& positionB)
{
    float deltaX = positionB.x - positionA.x;
    float deltaY = positionB.y - positionA.y;
    float deltaZ = positionB.z - positionA.z;

    return sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
}

float GetDistanceSquared3D(Vec3 const& positionA, Vec3 const& positionB)
{
    float deltaX = positionB.x - positionA.x;
    float deltaY = positionB.y - positionA.y;
    float deltaZ = positionB.z - positionA.z;
    return deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;
}

float GetDistanceXY3D(Vec3 const& positionA, Vec3 const& positionB)
{
    float deltaX = positionB.x - positionA.x;
    float deltaY = positionB.y - positionA.y;

    return sqrt(deltaX * deltaX + deltaY * deltaY);
}

float GetDistanceXYSquared3D(Vec3 const& positionA, Vec3 const& positionB)
{
    float deltaX = positionB.x - positionA.x;
    float deltaY = positionB.y - positionA.y;
    return deltaX * deltaX + deltaY * deltaY;
}

bool DoDiscsOverlap(Vec2 const& centerA, float radiusA, Vec2 const& centerB, float radiusB)
{
    float discsTotalLength = radiusA + radiusB;
    float centerDistance = GetDistance2D(centerA, centerB);
    if (discsTotalLength <= centerDistance)
        return false;
    return true;
}

bool DoSpheresOverlap(Vec3 const& centerA, float radiusA, Vec3 const& centerB, float radiusB)
{
    float discsTotalLength = radiusA + radiusB;
    float centerDistance = GetDistance3D(centerA, centerB);
    if (discsTotalLength <= centerDistance)
        return false;
    return true;
}

void TransformPosition2D(Vec2& posToTransform, float uniformScale, float rotationDegrees, Vec2 const& translation)
{
    // Apply scaling
    posToTransform.x *= uniformScale;
    posToTransform.y *= uniformScale;

    // Rotate Using Polar coordinate
    float length = sqrt(posToTransform.x * posToTransform.x + posToTransform.y * posToTransform.y);
    float theta = Atan2Degrees(posToTransform.y, posToTransform.x);
    theta += rotationDegrees;

    // Translate
    float convertX = length * cosf(ConvertDegreesToRadians(theta));
    float convertY = length * sinf(ConvertDegreesToRadians(theta));

    posToTransform.x = convertX + translation.x;
    posToTransform.y = convertY + translation.y;
}

void TransformPositionXY3D(Vec3& positionToTransform, float scaleXY, float zRotationDegrees, Vec2 const& translationXY)
{
    // Apply scaling
    positionToTransform.x *= scaleXY;
    positionToTransform.y *= scaleXY;

    // Rotate Using Polar coordinate
    float length = sqrt(positionToTransform.x * positionToTransform.x + positionToTransform.y * positionToTransform.y);
    float theta = Atan2Degrees(positionToTransform.y, positionToTransform.x);
    theta += zRotationDegrees;

    // Translate
    float convertX = length * cosf(ConvertDegreesToRadians(theta));
    float convertY = length * sinf(ConvertDegreesToRadians(theta));

    positionToTransform.x = convertX + translationXY.x;
    positionToTransform.y = convertY + translationXY.y;
}
