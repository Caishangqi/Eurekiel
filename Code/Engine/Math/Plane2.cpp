#include "Plane2.hpp"
#include "MathUtils.hpp"

Plane2::Plane2(const Vec2& normal, float distance)
    : m_normal(normal)
    , m_distance(distance)
{
}

Plane2::Plane2(const Vec2& pointOnPlane, const Vec2& normal)
    : m_normal(normal)
    , m_distance(DotProduct2D(normal, pointOnPlane))
{
}

Plane2::Plane2(const Plane2& copyFrom)
    : m_normal(copyFrom.m_normal)
    , m_distance(copyFrom.m_distance)
{
}

float Plane2::GetSignedDistance(const Vec2& point) const
{
    return GetSignedDistance(point, *this);
}

bool Plane2::IsPointInFrontOfPlane(const Vec2& point) const
{
    return IsPointInFrontOfPlane(point, *this);
}

bool Plane2::IsPointBehind(const Vec2& point) const
{
    return IsPointBehind(point, *this);
}

Vec2 Plane2::GetNearestPoint(const Vec2& point) const
{
    return GetNearestPoint(point, *this);
}

Vec2 Plane2::GetCenter() const
{
    return GetCenter(*this);
}

float Plane2::GetSignedDistance(const Vec2& point, const Plane2& plane)
{
    return DotProduct2D(plane.m_normal, point) - plane.m_distance;
}

bool Plane2::IsPointInFrontOfPlane(const Vec2& point, const Plane2& plane)
{
    return GetSignedDistance(point, plane) > 0.0f;
}

bool Plane2::IsPointBehind(const Vec2& point, const Plane2& plane)
{
    return GetSignedDistance(point, plane) < 0.0f;
}

Vec2 Plane2::GetNearestPoint(const Vec2& point, const Plane2& plane)
{
    float signedDist = GetSignedDistance(point, plane);
    return point - signedDist * plane.m_normal;
}

Vec2 Plane2::GetCenter(const Plane2& plane)
{
    return plane.m_normal * plane.m_distance;
}

Plane2 Plane2::FromEdge(const Vec2& startPos, const Vec2& endPos)
{
    Vec2 edgeDir = (endPos - startPos).GetNormalized();
    // CCW vertices -> outward normal (perpendicular to edge, pointing right/outward)
    // For CCW winding, the outward normal is (edgeDir.y, -edgeDir.x)
    Vec2 normal = Vec2(edgeDir.y, -edgeDir.x);
    float distance = DotProduct2D(normal, startPos);
    return Plane2(normal, distance);
}
