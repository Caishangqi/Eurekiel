#include "Engine/Math/MathUtils.hpp"
#include <cmath>

#include "AABB2.hpp"
#include "IntVec2.hpp"
#include "Vec2.hpp"
#include "Vec3.hpp"

constexpr float PI = 3.14159265358979323846f;

float GetClamped(float value, float minValue, float maxValue)
{
    if (value < minValue)
    {
        return minValue;
    }
    if (value > maxValue)
        return maxValue;
    return value;
}

float GetClampedZeroToOne(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    if (value > 1.0f)
    {
        return 1.0f;
    }
    return value;
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
    const float t = GetFractionWithinRange(inValue, inStart, inEnd);
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
    return currentDegrees - maxDeltaDegrees;
}

float GetAngleDegreesBetweenVectors2D(const Vec2& first, const Vec2& second)
{
    Vec2 normalizedFirst  = first.GetNormalized();
    Vec2 normalizedSecond = second.GetNormalized();
    return ConvertRadiansToDegrees(acosf(DotProduct2D(normalizedFirst, normalizedSecond)));
}


float DotProduct2D(const Vec2& a, const Vec2& b)
{
    return a.x * b.x + a.y * b.y;
}

float GetDistance2D(const Vec2& positionA, const Vec2& positionB)
{
    float deltaX = positionB.x - positionA.x;
    float deltaY = positionB.y - positionA.y;
    return sqrtf(deltaX * deltaX + deltaY * deltaY);
}

float GetDistanceSquared2D(const Vec2& positionA, const Vec2& positionB)
{
    float deltaX = positionB.x - positionA.x;
    float deltaY = positionB.y - positionA.y;
    return deltaX * deltaX + deltaY * deltaY;
}

float GetDistance3D(const Vec3& positionA, const Vec3& positionB)
{
    float deltaX = positionB.x - positionA.x;
    float deltaY = positionB.y - positionA.y;
    float deltaZ = positionB.z - positionA.z;

    return sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
}

float GetDistanceSquared3D(const Vec3& positionA, const Vec3& positionB)
{
    float deltaX = positionB.x - positionA.x;
    float deltaY = positionB.y - positionA.y;
    float deltaZ = positionB.z - positionA.z;
    return deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;
}

float GetDistanceXY3D(const Vec3& positionA, const Vec3& positionB)
{
    float deltaX = positionB.x - positionA.x;
    float deltaY = positionB.y - positionA.y;

    return sqrt(deltaX * deltaX + deltaY * deltaY);
}

float GetDistanceXYSquared3D(const Vec3& positionA, const Vec3& positionB)
{
    float deltaX = positionB.x - positionA.x;
    float deltaY = positionB.y - positionA.y;
    return deltaX * deltaX + deltaY * deltaY;
}

int GetTaxicabDistance2D(const IntVec2& pointA, const IntVec2& pointB)
{
    return abs(pointB.x - pointA.x) + abs(pointB.y - pointA.y);
}

float GetProjectedLength2D(const Vec2& vectorToProject, const Vec2& vectorToProjectOnto)
{
    // No normalization of the vector to project
    Vec2 vectorToProjectOntoNormalized = vectorToProjectOnto.GetNormalized();
    return DotProduct2D(vectorToProject, vectorToProjectOntoNormalized);
}


const Vec2 GetProjectedOnto2D(const Vec2& vectorToProject, const Vec2& vectorToProjectOnto)
{
    Vec2  vectorToProjectOntoNormalized = vectorToProjectOnto.GetNormalized();
    float projectionLength              = DotProduct2D(vectorToProject, vectorToProjectOntoNormalized);
    return vectorToProjectOntoNormalized * projectionLength;
}


bool DoDiscsOverlap(const Vec2& centerA, float radiusA, const Vec2& centerB, float radiusB)
{
    float discsTotalLength = radiusA + radiusB;
    float centerDistance   = GetDistance2D(centerA, centerB);
    if (discsTotalLength <= centerDistance)
        return false;
    return true;
}

bool DoSpheresOverlap(const Vec3& centerA, float radiusA, const Vec3& centerB, float radiusB)
{
    float discsTotalLength = radiusA + radiusB;
    float centerDistance   = GetDistance3D(centerA, centerB);
    if (discsTotalLength <= centerDistance)
        return false;
    return true;
}

Vec2 GetNearestPointOnDisc2D(const Vec2& referencePosition, const Vec2& discCenter, float discRadius)
{
    float distSquared       = GetDistanceSquared2D(referencePosition, discCenter);
    float discRadiusSquared = discRadius * discRadius;

    if (distSquared <= discRadiusSquared)
        return referencePosition;

    return discCenter + (referencePosition - discCenter).GetNormalized() * discRadius;
}

bool IsPointInsideDisc2D(const Vec2& point, const Vec2& discCenter, float discRadius)
{
    float distance = GetDistance2D(point, discCenter);
    return distance <= discRadius;
}

bool IsPointInsideOrientedSector2D(const Vec2& point, const Vec2&           sectorTip, float sectorForwardDegrees,
                                   float       sectorApertureDegrees, float sectorRadius)
{
    float distanceSquared = GetDistanceSquared2D(point, sectorTip);
    if (distanceSquared > sectorRadius * sectorRadius)
    {
        return false;
    }

    Vec2  directionToPoint  = (point - sectorTip).GetNormalized();
    float pointAngleDegrees = Atan2Degrees(directionToPoint.y, directionToPoint.x);
    float angularDisplacement = GetShortestAngularDispDegrees(sectorForwardDegrees, pointAngleDegrees);
    
    if (fabs(angularDisplacement) <= sectorApertureDegrees * 0.5f)
    {
        return true;
    }

    return false;
}

bool IsPointInsideDirectedSector2D(const Vec2& point, const Vec2&           sectorTip, const Vec2& sectorForwardNormal,
                                   float       sectorApertureDegrees, float sectorRadius)
{
    float distanceSquared = GetDistanceSquared2D(point, sectorTip);
    if (distanceSquared > sectorRadius * sectorRadius)
    {
        return false;
    }
    Vec2  directionToPoint = (point - sectorTip).GetNormalized();
    float dotProduct       = DotProduct2D(directionToPoint, sectorForwardNormal);
    float angleBetween     = ConvertRadiansToDegrees(acosf(dotProduct));
    if (angleBetween <= (sectorApertureDegrees * 0.5f))
    {
        return true;
    }

    return false;
}


bool PushDiscOutOfPoint2D(Vec2& mobileDiscCenter, float discRadius, const Vec2& fixedPoint)
{
    Vec2 fixedPointToDiscCenter = mobileDiscCenter - fixedPoint;

    if (fixedPointToDiscCenter.GetLengthSquared() >= discRadius * discRadius)
    {
        return false;
    }

    fixedPointToDiscCenter.SetLength(discRadius);

    mobileDiscCenter = fixedPoint + fixedPointToDiscCenter;

    return true;
}


bool PushDiscOutOfDisc2D(Vec2& mobileDiscCenter, float discRadius, const Vec2& fixedDiscCenter, float fixedDiscRadius)
{
    Vec2 mobileToFixDirection = mobileDiscCenter - fixedDiscCenter;
    if (mobileToFixDirection.GetLengthSquared() >= ((discRadius + fixedDiscRadius) * (discRadius + fixedDiscRadius)))
    {
        return false;
    }
    // put mobile to the fix but need adjust radius
    mobileToFixDirection.SetLength(fixedDiscRadius + discRadius);

    mobileDiscCenter = fixedDiscCenter + mobileToFixDirection;

    return true;
}

bool PushDiscsOutOfEachOther2D(Vec2& aCenter, float aRadius, Vec2& bCenter, float bRadius)
{
    Vec2 direction = aCenter - bCenter;
    if (direction.GetLengthSquared() >= (aRadius + bRadius) * (aRadius + bRadius))
        return false;

    float overlap = aRadius + bRadius - (aCenter - bCenter).GetLength();
    aCenter       = aCenter + direction.GetNormalized() * overlap / 2;
    bCenter       = bCenter - direction.GetNormalized() * overlap / 2; // opposite direction
    return true;
}


bool PushDiscOutOfAABB2D(Vec2& mobileDiscCenter, float discRadius, const AABB2& fixedBox)
{
    Vec2 discToBoxNearestPoint = fixedBox.GetNearestPoint(mobileDiscCenter);
    Vec2 pointToDiscCenter     = mobileDiscCenter - discToBoxNearestPoint;

    if (pointToDiscCenter.GetLength() >= discRadius)
    {
        return false;
    }

    float overlap    = discRadius - pointToDiscCenter.GetLength();
    mobileDiscCenter = mobileDiscCenter + pointToDiscCenter.GetNormalized() * overlap;
    return true;
}

void TransformPosition2D(Vec2& posToTransform, float uniformScale, float rotationDegrees, const Vec2& translation)
{
    // Apply scaling
    posToTransform.x *= uniformScale;
    posToTransform.y *= uniformScale;

    // Rotate Using Polar coordinate
    float length = sqrt(posToTransform.x * posToTransform.x + posToTransform.y * posToTransform.y);
    float theta  = Atan2Degrees(posToTransform.y, posToTransform.x);
    theta += rotationDegrees;

    // Translate
    float convertX = length * cosf(ConvertDegreesToRadians(theta));
    float convertY = length * sinf(ConvertDegreesToRadians(theta));

    posToTransform.x = convertX + translation.x;
    posToTransform.y = convertY + translation.y;
}

void TransformPositionXY3D(Vec3& positionToTransform, float scaleXY, float zRotationDegrees, const Vec2& translationXY)
{
    // Apply scaling
    positionToTransform.x *= scaleXY;
    positionToTransform.y *= scaleXY;

    // Rotate Using Polar coordinate
    float length = sqrt(positionToTransform.x * positionToTransform.x + positionToTransform.y * positionToTransform.y);
    float theta  = Atan2Degrees(positionToTransform.y, positionToTransform.x);
    theta += zRotationDegrees;

    // Translate
    float convertX = length * cosf(ConvertDegreesToRadians(theta));
    float convertY = length * sinf(ConvertDegreesToRadians(theta));

    positionToTransform.x = convertX + translationXY.x;
    positionToTransform.y = convertY + translationXY.y;
}

void TransformPosition2D(Vec2& posToTransform, const Vec2& iBasis, const Vec2& jBasis, const Vec2& translation)
{
    // Compute the transformed position
    float newX = (posToTransform.x * iBasis.x) + (posToTransform.y * jBasis.x) + translation.x;
    float newY = (posToTransform.x * iBasis.y) + (posToTransform.y * jBasis.y) + translation.y;

    posToTransform.x = newX;
    posToTransform.y = newY;
}


void TransformPositionXY3D(Vec3& posToTransform, const Vec2& iBasis, const Vec2& jBasis, const Vec2& translation)
{
    // Compute the transformed x and y positions
    float newX = (posToTransform.x * iBasis.x) + (posToTransform.y * jBasis.x) + translation.x;
    float newY = (posToTransform.x * iBasis.y) + (posToTransform.y * jBasis.y) + translation.y;

    posToTransform.x = newX;
    posToTransform.y = newY;
}
