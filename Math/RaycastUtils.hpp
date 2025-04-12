#pragma once
#include "Sphere.h"
#include "Vec2.hpp"
#include "Vec3.hpp"

class ZCylinder;
class AABB3;
class AABB2;
struct LineSegment2;

struct RaycastResult2D
{
    // Basic raycast result information (required)
    bool  m_didImpact  = false;
    float m_impactDist = 0.f;
    Vec2  m_impactPos;
    Vec2  m_impactNormal;

    // Original raycast information (optional)
    Vec2  m_rayFwdNormal;
    Vec2  m_rayStartPos;
    float m_rayMaxLength = 1.f;
};

struct RaycastResult3D
{
    // Basic raycast result information (required)
    bool  m_didImpact  = false;
    float m_impactDist = 0.f;
    Vec3  m_impactPos;
    Vec3  m_impactNormal;

    // Original raycast information (optional)
    Vec3  m_rayFwdNormal;
    Vec3  m_rayStartPos;
    float m_rayMaxLength = 1.f;
};

RaycastResult2D RaycastVsDisc2D(Vec2 startPos, Vec2 fwdNormal, float maxDist, Vec2 discCenter, float discRadius);
RaycastResult2D RaycastVsLineSegment2D(Vec2 startPos, Vec2 fwdNormal, float maxDist, LineSegment2& lineSegment2D);
RaycastResult2D RaycastVsAABB2(Vec2 startPos, Vec2 fwdNormal, float maxDist, AABB2& aabb2);
RaycastResult3D RaycastVsSphere3D(const Vec3 startPos, const Vec3 fwdNormal, float maxDist, const Sphere& sphere);
RaycastResult3D RaycastVsAABB3D(const Vec3 startPos, const Vec3 fwdNormal, float maxDist, const AABB3& aabb3);
RaycastResult3D RaycastVsZCylinder3D(const Vec3 startPos, const Vec3 fwdNormal, float maxDist, const ZCylinder& cylinder);
