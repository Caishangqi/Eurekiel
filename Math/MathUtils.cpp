#include "Engine/Math/MathUtils.hpp"
#include <cmath>

#include "AABB2.hpp"
#include "Capsule2.hpp"
#include "Disc2.hpp"
#include "EulerAngles.hpp"
#include "IntVec2.hpp"
#include "LineSegment2.hpp"
#include "OBB2.hpp"
#include "Sphere.h"
#include "Triangle2.hpp"
#include "Vec2.hpp"
#include "Vec3.hpp"
#include "ZCylinder.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

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

Vec2 Interpolate(Vec2 start, Vec2 end, float fractionTowardEnd)
{
    return Vec2(Interpolate(start.x, end.x, fractionTowardEnd), Interpolate(start.y, end.y, fractionTowardEnd));
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

float DotProduct3D(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float DotProduct4D(const Vec4& a, const Vec4& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

float CrossProduct2D(const Vec2& a, const Vec2& b)
{
    return a.x * b.y - a.y * b.x;
}

Vec3 CrossProduct3D(const Vec3& a, const Vec3& b)
{
    return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
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

bool DoSpheresOverlap(const Sphere& sphereA, const Sphere& sphereB)
{
    return DoSpheresOverlap(sphereA.m_position, sphereA.m_radius, sphereB.m_position, sphereB.m_radius);
}

bool DoAABB3DOverlap(const AABB3& aabbA, const AABB3& aabbB)
{
    // 如果在任意维度上分离，则不重叠
    if (aabbB.m_mins.x > aabbA.m_maxs.x) return false;
    if (aabbB.m_maxs.x < aabbA.m_mins.x) return false;

    if (aabbB.m_mins.y > aabbA.m_maxs.y) return false;
    if (aabbB.m_maxs.y < aabbA.m_mins.y) return false;

    if (aabbB.m_mins.z > aabbA.m_maxs.z) return false;
    if (aabbB.m_maxs.z < aabbA.m_mins.z) return false;

    // 如果所有维度都没有分离，就表示AABB3相交
    return true;
}

bool DoZCylinder3DOverlap(const ZCylinder& cylinderA, const ZCylinder& cylinderB)
{
    // 1) 先检查XY平面上两圆是否重叠
    //    即判断圆心XY距离是否 <= (rA + rB)
    Vec2  centerA2D(cylinderA.m_center.x, cylinderA.m_center.y);
    Vec2  centerB2D(cylinderB.m_center.x, cylinderB.m_center.y);
    float sumRadius = cylinderA.m_radius + cylinderB.m_radius;
    float distXY    = GetDistance2D(centerA2D, centerB2D);
    if (distXY >= sumRadius)
    {
        return false; // XY 平面上就已经没有相交
    }

    // 2) 再检查它们在Z方向上的区间是否重叠
    //    cylinder的z范围: [ zCenter - height/2, zCenter + height/2 ]
    float aMinZ = cylinderA.m_center.z - (cylinderA.m_height * 0.5f);
    float aMaxZ = cylinderA.m_center.z + (cylinderA.m_height * 0.5f);
    float bMinZ = cylinderB.m_center.z - (cylinderB.m_height * 0.5f);
    float bMaxZ = cylinderB.m_center.z + (cylinderB.m_height * 0.5f);

    // 区间分离判断: 如果其中一个区间完全在另一段之外，则不相交
    if (bMinZ >= aMaxZ) return false;
    if (bMaxZ <= aMinZ) return false;

    return true;
}

bool DoSphereAndAABB3DOverlap(const Sphere& sphere, const AABB3& aabb)
{
    // 算出AABB在XYZ方向与球心最近的点，然后比较该点与球心的距离
    // 如果距离 <= 半径，说明有重叠
    float clampedX = GetClamped(sphere.m_position.x, aabb.m_mins.x, aabb.m_maxs.x);
    float clampedY = GetClamped(sphere.m_position.y, aabb.m_mins.y, aabb.m_maxs.y);
    float clampedZ = GetClamped(sphere.m_position.z, aabb.m_mins.z, aabb.m_maxs.z);
    Vec3  nearestPointOnBox(clampedX, clampedY, clampedZ);

    float distSquared   = GetDistanceSquared3D(sphere.m_position, nearestPointOnBox);
    float radiusSquared = sphere.m_radius * sphere.m_radius;

    return (distSquared <= radiusSquared);
}

bool DoZCylinder3DAndAABB3DOverlap(const AABB3& aabb, const ZCylinder& cylinder)
{
    // 1) 先检查XY平面上投影是否重叠。
    //    可以将AABB的XY范围与圆做一次最近点检测
    Vec2 cylinderCenter2D(cylinder.m_center.x, cylinder.m_center.y);
    // 取出AABB2 的 X范围/Y范围
    AABB2 box2D(
        Vec2(aabb.m_mins.x, aabb.m_mins.y),
        Vec2(aabb.m_maxs.x, aabb.m_maxs.y)
    );
    Vec2  nearestOnBox2D = box2D.GetNearestPoint(cylinderCenter2D);
    float dist2D         = GetDistance2D(nearestOnBox2D, cylinderCenter2D);
    if (dist2D > cylinder.m_radius)
    {
        return false;
    }

    // 2) 检查Z方向的区间是否重叠
    float boxMinZ = aabb.m_mins.z;
    float boxMaxZ = aabb.m_maxs.z;

    float cylMinZ = cylinder.m_center.z - (cylinder.m_height * 0.5f);
    float cylMaxZ = cylinder.m_center.z + (cylinder.m_height * 0.5f);

    if (cylMinZ > boxMaxZ) return false;
    if (cylMaxZ < boxMinZ) return false;

    return true;
}

bool DoZCylinder3DAndSphereOverlap(const Sphere& sphere, const ZCylinder& cylinder)
{
    // 可以把球心与圆柱做“最近点”求距离，如果距离 <= 球的半径，则表示有重叠
    // 最近点分XY和平面以及Z两个方向:
    //  1) XY: 若球心的(x,y) 超过圆柱半径，则需要夹到半径上
    //  2) Z: 也同样夹到 [center.z - height/2, center.z + height/2]
    // 然后与球心做距离比较
    Vec3 nearestPoint = GetNearestPointOnZCylinder3D(sphere.m_position, cylinder);

    // 如果中心与这个最近点距离 <= sphere radius
    float distSquared = GetDistanceSquared3D(sphere.m_position, nearestPoint);
    if (distSquared <= (sphere.m_radius * sphere.m_radius))
    {
        return true;
    }
    return false;
}

Vec2 GetNearestPointOnDisc2D(const Vec2& referencePosition, const Vec2& discCenter, float discRadius)
{
    float distSquared       = GetDistanceSquared2D(referencePosition, discCenter);
    float discRadiusSquared = discRadius * discRadius;

    if (distSquared <= discRadiusSquared)
        return referencePosition;

    return discCenter + (referencePosition - discCenter).GetNormalized() * discRadius;
}

Vec2 GetNearestPointOnDisc2D(const Vec2& referencePosition, const Disc2& disc)
{
    return GetNearestPointOnDisc2D(referencePosition, disc.m_position, disc.m_radius);
}

Vec2 GetNearestPointOnAABB2D(const Vec2& referencePos, const AABB2& alignedBox)
{
    float clampedX = GetClamped(referencePos.x, alignedBox.m_mins.x, alignedBox.m_maxs.x);
    float clampedY = GetClamped(referencePos.y, alignedBox.m_mins.y, alignedBox.m_maxs.y);

    return Vec2(clampedX, clampedY);
}

Vec2 GetNearestPointOnOBB2D(const Vec2& referencePos, const OBB2& alignedBox)
{
    if (IsPointInsideOBB2D(referencePos, alignedBox))
    {
        return referencePos;
    }
    Vec2 referenceLocal = referencePos - alignedBox.m_center;

    float xProj = DotProduct2D(referenceLocal, alignedBox.m_iBasisNormal);
    float yProj = DotProduct2D(referenceLocal, alignedBox.m_iBasisNormal.GetRotated90Degrees());

    float clampedX = GetClamped(xProj, -alignedBox.m_halfDimensions.x, alignedBox.m_halfDimensions.x);
    float clampedY = GetClamped(yProj, -alignedBox.m_halfDimensions.y, alignedBox.m_halfDimensions.y);

    // Convert the clamped local position back to world space
    Vec2 nearestPointLocal = (clampedX * alignedBox.m_iBasisNormal) + (clampedY * alignedBox.m_iBasisNormal.
                                                                                             GetRotated90Degrees());
    return alignedBox.m_center + nearestPointLocal;
}

Vec2 GetNearestPointOnInfiniteLine2D(const Vec2& referencePos, const LineSegment2& infiniteLine)
{
    return GetNearestPointOnLineSegment2D(referencePos, infiniteLine);
}

Vec2 GetNearestPointOnLineSegment2D(const Vec2& referencePos, const LineSegment2& lineSegment)
{
    Vec2 SE = lineSegment.m_end - lineSegment.m_start;
    Vec2 SP = referencePos - lineSegment.m_start;

    float lineLengthSquared = SE.GetLengthSquared();
    if (lineLengthSquared == 0.0f)
    {
        return lineSegment.m_start; // Handle point
    }

    float t = DotProduct2D(SP, SE) / lineLengthSquared;
    t       = GetClamped(t, 0.0f, 1.0f);

    return lineSegment.m_start + SE * t;
}

Vec2 GetNearestPointOnCapsule2D(const Vec2& referencePos, const Capsule2& capsule)
{
    Vec2 nearestPointOnSegment = GetNearestPointOnLineSegment2D(referencePos,
                                                                LineSegment2(capsule.m_start, capsule.m_end));
    return GetNearestPointOnDisc2D(referencePos, nearestPointOnSegment, capsule.m_radius);
}

Vec2 GetNearestPointOnTriangle2D(const Vec2& referencePos, const Triangle2& triangle)
{
    if (IsPointInsideTriangle(referencePos, triangle))
    {
        return referencePos;
    }
    Vec2 pointA = triangle.m_positionCounterClockwise[0];
    Vec2 pointB = triangle.m_positionCounterClockwise[1];
    Vec2 pointC = triangle.m_positionCounterClockwise[2];

    Vec2 nearestOnAB = GetNearestPointOnLineSegment2D(referencePos, LineSegment2(pointA, pointB));
    Vec2 nearestOnBC = GetNearestPointOnLineSegment2D(referencePos, LineSegment2(pointB, pointC));
    Vec2 nearestOnCA = GetNearestPointOnLineSegment2D(referencePos, LineSegment2(pointC, pointA));

    float distToAB = GetDistanceSquared2D(referencePos, nearestOnAB);
    float distToBC = GetDistanceSquared2D(referencePos, nearestOnBC);
    float distToCA = GetDistanceSquared2D(referencePos, nearestOnCA);

    if (distToAB <= distToBC && distToAB <= distToCA)
    {
        return nearestOnAB;
    }
    if (distToBC <= distToCA)
    {
        return nearestOnBC;
    }
    return nearestOnCA;
}

Vec3 GetNearestPointOnSphere(const Vec3& referencePosition, const Sphere& sphere)
{
    float distSquared       = GetDistanceSquared3D(referencePosition, sphere.m_position);
    float discRadiusSquared = sphere.m_radius * sphere.m_radius;

    if (distSquared <= discRadiusSquared)
        return referencePosition;

    return sphere.m_position + (referencePosition - sphere.m_position).GetNormalized() * sphere.m_radius;
}

Vec3 GetNearestPointOnCube3D(const Vec3& referencePos, const AABB3& aabb)
{
    float clampedX = GetClamped(referencePos.x, aabb.m_mins.x, aabb.m_maxs.x);
    float clampedY = GetClamped(referencePos.y, aabb.m_mins.y, aabb.m_maxs.y);
    float clampedZ = GetClamped(referencePos.z, aabb.m_mins.z, aabb.m_maxs.z);

    return Vec3(clampedX, clampedY, clampedZ);
}

Vec3 GetNearestPointOnZCylinder3D(const Vec3& referencePos, const ZCylinder& cylinder)
{
    // Project referencePos onto the X-Y plane
    Vec3 projectedPoint(referencePos.x, referencePos.y, cylinder.m_center.z);

    // Calculate the distance from the projected point to the cylinder's center
    Vec2  offsetFromCenter   = Vec2(projectedPoint.x, projectedPoint.y) - Vec2(cylinder.m_center.x, cylinder.m_center.y);
    float distanceFromCenter = offsetFromCenter.GetLength();

    // Clamp the point within the cylinder's radius
    if (distanceFromCenter > cylinder.m_radius)
    {
        offsetFromCenter = offsetFromCenter.GetNormalized() * cylinder.m_radius;
        projectedPoint.x = cylinder.m_center.x + offsetFromCenter.x;
        projectedPoint.y = cylinder.m_center.y + offsetFromCenter.y;
    }

    // Calculate the nearest point on the Z-axis
    float minZ       = cylinder.m_center.z - (cylinder.m_height * 0.5f);
    float maxZ       = cylinder.m_center.z + (cylinder.m_height * 0.5f);
    projectedPoint.z = GetClamped(referencePos.z, minZ, maxZ);

    return projectedPoint;
}

bool IsPointInsideDisc2D(const Vec2& point, const Vec2& discCenter, float discRadius)
{
    float distance = GetDistance2D(point, discCenter);
    return distance <= discRadius;
}

bool IsPointInsideDisc2D(const Vec2& point, const Disc2& disc)
{
    return IsPointInsideDisc2D(point, disc.m_position, disc.m_radius);
}

bool IsPointInsideAABB2D(const Vec2& point, const AABB2& box)
{
    return box.IsPointInside(point);
}

bool IsPointInsideOBB2D(const Vec2& point, const OBB2& box)
{
    Vec2 pointLocal = point - box.m_center;

    float xProj = DotProduct2D(pointLocal, box.m_iBasisNormal);
    float yProj = DotProduct2D(pointLocal, box.m_iBasisNormal.GetRotated90Degrees());

    if (xProj >= box.m_halfDimensions.x)
    {
        return false;
    }
    if (xProj <= -box.m_halfDimensions.x)
    {
        return false;
    }
    if (yProj >= box.m_halfDimensions.y)
    {
        return false;
    }
    if (yProj <= -box.m_halfDimensions.y)
    {
        return false;
    }
    return true;
}

bool IsPointInsideZCylinder3D(const Vec3& point, const ZCylinder& cylinder)
{
    float halfHeight = cylinder.m_height * 0.5f;
    float zMin       = cylinder.m_center.z - halfHeight;
    float zMax       = cylinder.m_center.z + halfHeight;
    // 2D 圆心
    Vec2 cylinderCenterXY(cylinder.m_center.x, cylinder.m_center.y);
    Vec2 startXY(point.x, point.y);
    // 先判断 XY 是否在圆内
    float distXY   = (startXY - cylinderCenterXY).GetLength();
    bool  insideXY = (distXY <= cylinder.m_radius);
    // 再判断 z 是否在 [zMin,zMax]
    bool insideZ = (point.z >= zMin) && (point.z <= zMax);
    return insideZ && insideXY;
}

bool IsPointInsideCapsule(const Vec2& point, const Capsule2& capsule)
{
    Vec2 np = GetNearestPointOnLineSegment2D(point, LineSegment2(capsule.m_start, capsule.m_end));
    Vec2 PN = np - point;
    return PN.GetLengthSquared() < capsule.m_radius * capsule.m_radius;
}

bool IsPointInsideTriangle(const Vec2& point, const Triangle2& triangle)
{
    Vec2 pointA = triangle.m_positionCounterClockwise[0];
    Vec2 pointB = triangle.m_positionCounterClockwise[1];
    Vec2 pointC = triangle.m_positionCounterClockwise[2];

    Vec2 normalAB = (pointB - pointA).GetRotated90Degrees();
    Vec2 normalBC = (pointC - pointB).GetRotated90Degrees();
    Vec2 normalCA = (pointA - pointC).GetRotated90Degrees();

    if (DotProduct2D((point - pointA), normalAB) <= 0)
    {
        return false;
    }
    if (DotProduct2D((point - pointB), normalBC) <= 0)
    {
        return false;
    }
    if (DotProduct2D((point - pointC), normalCA) <= 0)
    {
        return false;
    }
    return true;
}

bool IsPointInsideOrientedSector2D(const Vec2& point, const Vec2&           sectorTip, float sectorForwardDegrees,
                                   float       sectorApertureDegrees, float sectorRadius)
{
    float distanceSquared = GetDistanceSquared2D(point, sectorTip);
    if (distanceSquared > sectorRadius * sectorRadius)
    {
        return false;
    }

    Vec2  directionToPoint    = (point - sectorTip).GetNormalized();
    float pointAngleDegrees   = Atan2Degrees(directionToPoint.y, directionToPoint.x);
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

bool PushDiscOutOfPoint2D(Disc2& mobileDisc, const Vec2& fixedPoint)
{
    return PushDiscOutOfPoint2D(mobileDisc.m_position, mobileDisc.m_radius, fixedPoint);
}

bool PushDiscOutOfCapsule2D(Vec2& mobileDiscCenter, float discRadius, const Capsule2& fixedCapsule2D)
{
    Vec2 nearestPointOnCapsule = GetNearestPointOnCapsule2D(mobileDiscCenter, fixedCapsule2D);

    Vec2  displacement = mobileDiscCenter - nearestPointOnCapsule;
    float dist         = displacement.GetLength();
    if (dist >= discRadius)
    {
        return false;
    }
    float overlap = discRadius - dist;

    // In case dist is nearly zero, pick a fallback direction to avoid division by zero.
    if (dist < 1e-6f)
    {
        Vec2 capsuleCenter = (fixedCapsule2D.m_start + fixedCapsule2D.m_end) * 0.5f;
        displacement       = mobileDiscCenter - capsuleCenter;
        if (displacement.GetLength() < 1e-6f)
        {
            displacement = Vec2(1.f, 0.f);
        }
        dist = displacement.GetLength();
    }
    displacement.SetLength(overlap);
    mobileDiscCenter += displacement;
    return true;
    /*if (IsPointInsideCapsule(mobileDiscCenter, fixedCapsule2D))
    {
        Vec2  nearestPoint     = GetNearestPointOnLineSegment2D(mobileDiscCenter, LineSegment2(fixedCapsule2D.m_start, fixedCapsule2D.m_end));
        Vec2  distanceToCenter = mobileDiscCenter - nearestPoint;
        float diff             = fixedCapsule2D.m_radius - (distanceToCenter.GetLength() + discRadius);
        float adjust           = discRadius * 2 + diff;
        mobileDiscCenter       = mobileDiscCenter + distanceToCenter.GetNormalized() * adjust;
        return true;
    }
    else
    {
        Vec2 nearestPoint     = GetNearestPointOnCapsule2D(mobileDiscCenter, fixedCapsule2D);
        Vec2 distanceToCenter = mobileDiscCenter - nearestPoint;
        if (discRadius * discRadius >= distanceToCenter.GetLengthSquared())
        {
            float diff       = discRadius - distanceToCenter.GetLength();
            mobileDiscCenter = mobileDiscCenter + distanceToCenter.GetNormalized() * diff;
            return true;
        }
        else
        {
            return false;
        }
    }*/
}

bool PushDiscOutOfOBB2D(Vec2& mobileDiscCenter, float discRadius, const OBB2& fixedOBB2D)
{
    Vec2  nearestPointOnOBB = GetNearestPointOnOBB2D(mobileDiscCenter, fixedOBB2D);
    Vec2  displacement      = mobileDiscCenter - nearestPointOnOBB;
    float distance          = displacement.GetLength();

    if (distance >= discRadius)
    {
        return false;
    }

    float overlap = discRadius - distance;

    if (distance < 1e-6f)
    {
        Vec2 obbCenter = fixedOBB2D.m_center;
        displacement   = mobileDiscCenter - obbCenter;

        if (displacement.GetLength() < 1e-6f)
        {
            displacement = Vec2(1.f, 0.f);
        }
        displacement.SetLength(overlap);
    }
    else
    {
        displacement.SetLength(overlap);
    }
    mobileDiscCenter += displacement;
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

bool PushDiscsOutOfEachOther2D(Disc2& mobileDiscA, Disc2& mobileDiscB)
{
    return PushDiscsOutOfEachOther2D(mobileDiscA.m_position, mobileDiscA.m_radius, mobileDiscB.m_position, mobileDiscB.m_radius);
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

/// 
/// @param mobileDisc 
/// @param mobileDiscVelocity 
/// @param mobileDiscElasticity 
/// @param bounceOffPoint Fixed bounce Point
/// @return 
bool BounceDiscOffPoint2D(Disc2& mobileDisc, Vec2& mobileDiscVelocity, float mobileDiscElasticity, Vec2& bounceOffPoint)
{
    if (PushDiscOutOfPoint2D(mobileDisc, bounceOffPoint))
    {
        Vec2  normal       = (mobileDisc.m_position - bounceOffPoint).GetNormalized();
        float scalarVn     = DotProduct2D(normal, mobileDiscVelocity);
        Vec2  vectorVn     = normal * scalarVn;
        Vec2  vectorVt     = mobileDiscVelocity - vectorVn;
        mobileDiscVelocity = vectorVt + (-vectorVn * mobileDiscElasticity);
        return true;
    }
    return false;
}

bool BounceDiscsOffEachOther(Disc2& mobileDiscA, Vec2& mobileDiscVelocityA, float mobileDiscElasticityA, Disc2& mobileDiscB, Vec2& mobileDiscVelocityB, float mobileDiscElasticityB)
{
    if (PushDiscsOutOfEachOther2D(mobileDiscA, mobileDiscB))
    {
        float finalElastic = mobileDiscElasticityA * mobileDiscElasticityB;
        Vec2  normalA      = (mobileDiscB.m_position - mobileDiscA.m_position).GetNormalized();
        float scalarAvn    = DotProduct2D(normalA, mobileDiscVelocityA);
        Vec2  vectorAvn    = normalA * scalarAvn;
        Vec2  vectorAvt    = mobileDiscVelocityA - vectorAvn;

        Vec2  normalB   = (mobileDiscA.m_position - mobileDiscB.m_position).GetNormalized();
        float scalarBvn = DotProduct2D(normalB, mobileDiscVelocityB);
        Vec2  vectorBvn = normalB * scalarBvn;
        Vec2  vectorBvt = mobileDiscVelocityB - vectorBvn;

        // Exchange momentum
        mobileDiscVelocityA = vectorAvt + (vectorBvn * finalElastic);
        mobileDiscVelocityB = vectorBvt + (vectorAvn * finalElastic);
        return true;
    }
    else
    {
        return false;
    }
}

bool BounceDiscsOffEachOther(Vec2& mobileDiscCenterA, float   mobileDiscRadiusA, Vec2& mobileDiscVelocityA, float mobileDiscElasticityA, Vec2& mobileDiscCenterB, float mobileDiscRadiusB,
                             Vec2& mobileDiscVelocityB, float mobileDiscElasticityB)
{
    Disc2 mobileDiscA = Disc2(mobileDiscCenterA, mobileDiscRadiusA);
    Disc2 mobileDiscB = Disc2(mobileDiscCenterB, mobileDiscRadiusB);
    bool  result      = BounceDiscsOffEachOther(mobileDiscA, mobileDiscVelocityA, mobileDiscElasticityA, mobileDiscB, mobileDiscVelocityB, mobileDiscElasticityB);
    mobileDiscCenterA = mobileDiscA.m_position;
    mobileDiscCenterB = mobileDiscB.m_position;
    return result;
}

bool BounceDiscOffCapsule2D(Vec2& mobileDiscCenter, float mobileDiscRadiusA, Vec2& mobileDiscVelocity, float mobileDiscElasticity, const Capsule2& fixedCapsule2D, float capsule2DElasticity)
{
    if (PushDiscOutOfCapsule2D(mobileDiscCenter, mobileDiscRadiusA, fixedCapsule2D))
    {
        Vec2  nearestPoint = GetNearestPointOnCapsule2D(mobileDiscCenter, fixedCapsule2D);
        Vec2  normal       = (mobileDiscCenter - nearestPoint).GetNormalized();
        float scalarVn     = DotProduct2D(normal, mobileDiscVelocity);
        Vec2  vectorVn     = normal * scalarVn;
        Vec2  vectorVt     = mobileDiscVelocity - vectorVn;
        mobileDiscVelocity = vectorVt + (-vectorVn * mobileDiscElasticity * capsule2DElasticity);
        return true;
    }
    return false;
}

bool BounceDiscOffDisc2D(Vec2& mobileDiscCenter, float mobileDiscRadiusA, Vec2& mobileDiscVelocity, float mobileDiscElasticity, const Disc2& fixedDisc2D, float disc2DElasticity)
{
    if (PushDiscOutOfDisc2D(mobileDiscCenter, mobileDiscRadiusA, fixedDisc2D.m_position, fixedDisc2D.m_radius))
    {
        Vec2  nearestPoint = GetNearestPointOnDisc2D(mobileDiscCenter, fixedDisc2D);
        Vec2  normal       = (mobileDiscCenter - nearestPoint).GetNormalized();
        float scalarVn     = DotProduct2D(normal, mobileDiscVelocity);
        Vec2  vectorVn     = normal * scalarVn;
        Vec2  vectorVt     = mobileDiscVelocity - vectorVn;
        mobileDiscVelocity = vectorVt + (-vectorVn * mobileDiscElasticity * disc2DElasticity);
        return true;
    }
    return false;
}

bool BounceDiscOffOBB2D(Vec2& mobileDiscCenter, float mobileDiscRadius, Vec2& mobileDiscVelocity, float mobileDiscElasticity, const OBB2& fixedOBB2D, float OBB2DElasticity)
{
    if (PushDiscOutOfOBB2D(mobileDiscCenter, mobileDiscRadius, fixedOBB2D))
    {
        Vec2  nearestPoint = GetNearestPointOnOBB2D(mobileDiscCenter, fixedOBB2D);
        Vec2  normal       = (mobileDiscCenter - nearestPoint).GetNormalized();
        float scalarVn     = DotProduct2D(normal, mobileDiscVelocity);
        Vec2  vectorVn     = normal * scalarVn;
        Vec2  vectorVt     = mobileDiscVelocity - vectorVn;
        mobileDiscVelocity = vectorVt + (-vectorVn * mobileDiscElasticity * OBB2DElasticity);
        return true;
    }
    return false;
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

int RecursiveSum(int From)
{
    if (From == 0)
    {
        return 0;
    }
    if (From == 1)
    {
        return 1;
    }
    return From + RecursiveSum(From - 1);
}

float NormalizeByte(unsigned char byte)
{
    return static_cast<float>(byte) / 255.f;
}

unsigned char DenormalizeByte(float range)
{
    if (range >= 1)
    {
        return 255;
    }

    if (range < 0)
    {
        return 0;
    }
    float normalized = GetClamped(range, 0, 1.0f);
    return static_cast<unsigned char>(floorf(normalized * 256.0f));
}

/// @brief  Computes a transform matrix for a billboarded object based on the specified billboard type.
///         By default, the object is scaled according to billboardScale and positioned at billboardPosition.
/// 
/// The function uses billboardType to decide how (and whether) the object should face or oppose the camera.
///   - WORLD_UP_FACING, WORLD_UP_OPPOSING: Orients the object using the world up-axis, though these are not implemented.
///   - FULL_FACING, FULL_OPPOSING: Orients the object so it fully faces or opposes the camera. 
///     (For instance, billboarded world text often uses FULL_OPPOSING to remain legible from the camera's viewpoint.)
///   - NONE: Disables billboard logic, effectively returning an unchanged or default transform.
///
/// @param [in] billboardType    An enum specifying how the billboarded object should orient relative to the camera.
/// @param [in] targetTransform  The base transform to which the billboard logic is applied.
/// @param [in] billboardPosition
///                             The world-space position around which the billboard transform is set.
/// @param [in] billboardScale   Uniform (X,Y) scaling applied to the billboarded object. Defaults to Vec2(1.f, 1.f).
///
/// @return A Mat44 representing the final billboard transform, incorporating position and scale,
///         and oriented according to the chosen billboard type.

Mat44 GetBillboardTransform(BillboardType billboardType, Mat44 const& targetTransform, Vec3 const& billboardPosition, Vec2 const& billboardScale)
{
    UNUSED(billboardScale)
    Mat44 transform;
    switch (billboardType)
    {
    case BillboardType::WORLD_UP_FACING:
        ERROR_RECOVERABLE("BillboardType is BillboardType::WORLD_UP_FACING is not implemented")
        return transform;
    case BillboardType::WORLD_UP_OPPOSING:
        ERROR_RECOVERABLE("BillboardType is BillboardType::WORLD_UP_OPPOSING is not implemented")
        return transform;
    case BillboardType::FULL_FACING:
        {
            Vec3 iBasis = (targetTransform.GetTranslation3D() - billboardPosition).GetNormalized();
            Vec3 jBasis;
            Vec3 kBasis;
            if (abs(DotProduct3D(iBasis, Vec3(0, 0, 1)) < 1))
            {
                jBasis = CrossProduct3D(Vec3(0, 0, 1), iBasis).GetNormalized();
                kBasis = CrossProduct3D(iBasis, jBasis);
            }
            else
            {
                kBasis = CrossProduct3D(iBasis, Vec3(0, 1, 0)).GetNormalized();
                jBasis = CrossProduct3D(kBasis, iBasis);
            }
            transform.SetIJK3D(iBasis, jBasis, kBasis);
            return transform;
        }
    case BillboardType::FULL_OPPOSING:
        {
            Mat44 cameraOrientation;
            cameraOrientation.SetIJK3D(targetTransform.GetIBasis3D(), targetTransform.GetJBasis3D(), targetTransform.GetKBasis3D());

            transform.Append(cameraOrientation);
            return transform;
        }
    case BillboardType::NONE:
        ERROR_AND_DIE("BillboardType not recognized")
    case BillboardType::COUNT:
        ERROR_AND_DIE("BillboardType not recognized")
    }
    return transform;
}

float CycleValue(float time, float period)
{
    float sinValue = sinf(2.0f * PI * time / period);
    return RangeMap(sinValue, -1.0f, 1.0f, 0.0f, 1.0f);
}
