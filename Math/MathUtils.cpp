#include "Engine/Math/MathUtils.hpp"
#include <cmath>

#include "Vec2.hpp"
#include "Vec3.hpp"

constexpr float PI = 3.14159265358979323846f;

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
