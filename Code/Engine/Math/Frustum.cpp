#include "Frustum.hpp"

#include "MathUtils.hpp"

#include <cmath>

namespace
{
    Plane3 MakePlaneFacingPoint(
        const Vec3& a,
        const Vec3& b,
        const Vec3& c,
        const Vec3& insidePoint)
    {
        Vec3 normal = CrossProduct3D(b - a, c - a).GetNormalized();
        float distance = DotProduct3D(normal, a);

        if (DotProduct3D(normal, insidePoint) < distance)
        {
            normal = -normal;
            distance = -distance;
        }

        return Plane3(normal, distance);
    }

    float GetSignedDistanceToPlane(const Plane3& plane, const Vec3& point)
    {
        return DotProduct3D(plane.m_normal, point) - plane.m_distToPlaneAlongNormalFromOrigin;
    }

    Vec3 GetPositiveVertex(const AABB3& bounds, const Vec3& normal)
    {
        return Vec3(
            normal.x >= 0.0f ? bounds.m_maxs.x : bounds.m_mins.x,
            normal.y >= 0.0f ? bounds.m_maxs.y : bounds.m_mins.y,
            normal.z >= 0.0f ? bounds.m_maxs.z : bounds.m_mins.z);
    }
}

Frustum::Frustum(const std::array<Plane3, PLANE_COUNT>& planes)
    : m_planes(planes)
{
}

Frustum Frustum::CreatePerspective(
    const Vec3& position,
    const Vec3& forward,
    const Vec3& left,
    const Vec3& up,
    float       verticalFovDegrees,
    float       aspectRatio,
    float       nearPlane,
    float       farPlane)
{
    const Vec3 normalizedForward = forward.GetNormalized();
    const Vec3 normalizedLeft = left.GetNormalized();
    const Vec3 normalizedUp = up.GetNormalized();

    const float halfVerticalRadians = ConvertDegreesToRadians(verticalFovDegrees * 0.5f);
    const float nearHalfHeight = tanf(halfVerticalRadians) * nearPlane;
    const float nearHalfWidth = nearHalfHeight * aspectRatio;
    const float farHalfHeight = tanf(halfVerticalRadians) * farPlane;
    const float farHalfWidth = farHalfHeight * aspectRatio;

    const Vec3 nearCenter = position + normalizedForward * nearPlane;
    const Vec3 farCenter = position + normalizedForward * farPlane;
    const Vec3 insidePoint = position + normalizedForward * ((nearPlane + farPlane) * 0.5f);

    const Vec3 nearTopLeft = nearCenter + normalizedLeft * nearHalfWidth + normalizedUp * nearHalfHeight;
    const Vec3 nearTopRight = nearCenter - normalizedLeft * nearHalfWidth + normalizedUp * nearHalfHeight;
    const Vec3 nearBottomLeft = nearCenter + normalizedLeft * nearHalfWidth - normalizedUp * nearHalfHeight;
    const Vec3 nearBottomRight = nearCenter - normalizedLeft * nearHalfWidth - normalizedUp * nearHalfHeight;

    const Vec3 farTopLeft = farCenter + normalizedLeft * farHalfWidth + normalizedUp * farHalfHeight;
    const Vec3 farTopRight = farCenter - normalizedLeft * farHalfWidth + normalizedUp * farHalfHeight;
    const Vec3 farBottomLeft = farCenter + normalizedLeft * farHalfWidth - normalizedUp * farHalfHeight;
    std::array<Plane3, PLANE_COUNT> planes;
    planes[NearPlane] = MakePlaneFacingPoint(nearTopLeft, nearTopRight, nearBottomRight, insidePoint);
    planes[FarPlane] = MakePlaneFacingPoint(farTopRight, farTopLeft, farBottomLeft, insidePoint);
    planes[LeftPlane] = MakePlaneFacingPoint(position, nearBottomLeft, nearTopLeft, insidePoint);
    planes[RightPlane] = MakePlaneFacingPoint(position, nearTopRight, nearBottomRight, insidePoint);
    planes[TopPlane] = MakePlaneFacingPoint(position, nearTopLeft, nearTopRight, insidePoint);
    planes[BottomPlane] = MakePlaneFacingPoint(position, nearBottomRight, nearBottomLeft, insidePoint);
    return Frustum(planes);
}

Frustum Frustum::CreateOrthographic(
    const Vec3& position,
    const Vec3& forward,
    const Vec3& left,
    const Vec3& up,
    const Vec2& mins,
    const Vec2& maxs,
    float       nearPlane,
    float       farPlane)
{
    const Vec3 normalizedForward = forward.GetNormalized();
    const Vec3 normalizedLeft = left.GetNormalized();
    const Vec3 normalizedUp = up.GetNormalized();

    const Vec3 nearCenter = position + normalizedForward * nearPlane;
    const Vec3 farCenter = position + normalizedForward * farPlane;
    const Vec3 insidePoint =
        position +
        normalizedForward * ((nearPlane + farPlane) * 0.5f) +
        normalizedLeft * ((mins.x + maxs.x) * 0.5f) +
        normalizedUp * ((mins.y + maxs.y) * 0.5f);

    const Vec3 nearMinMin = nearCenter + normalizedLeft * mins.x + normalizedUp * mins.y;
    const Vec3 nearMaxMin = nearCenter + normalizedLeft * maxs.x + normalizedUp * mins.y;
    const Vec3 nearMaxMax = nearCenter + normalizedLeft * maxs.x + normalizedUp * maxs.y;
    const Vec3 nearMinMax = nearCenter + normalizedLeft * mins.x + normalizedUp * maxs.y;

    const Vec3 farMinMin = farCenter + normalizedLeft * mins.x + normalizedUp * mins.y;
    const Vec3 farMaxMax = farCenter + normalizedLeft * maxs.x + normalizedUp * maxs.y;
    const Vec3 farMinMax = farCenter + normalizedLeft * mins.x + normalizedUp * maxs.y;

    std::array<Plane3, PLANE_COUNT> planes;
    planes[NearPlane] = MakePlaneFacingPoint(nearMinMax, nearMaxMax, nearMaxMin, insidePoint);
    planes[FarPlane] = MakePlaneFacingPoint(farMaxMax, farMinMax, farMinMin, insidePoint);
    planes[LeftPlane] = MakePlaneFacingPoint(nearMaxMin, nearMaxMax, farMaxMax, insidePoint);
    planes[RightPlane] = MakePlaneFacingPoint(nearMinMax, nearMinMin, farMinMin, insidePoint);
    planes[TopPlane] = MakePlaneFacingPoint(nearMinMax, nearMaxMax, farMaxMax, insidePoint);
    planes[BottomPlane] = MakePlaneFacingPoint(nearMaxMin, nearMinMin, farMinMin, insidePoint);
    return Frustum(planes);
}

bool Frustum::IsPointInside(const Vec3& point) const
{
    for (const Plane3& plane : m_planes)
    {
        if (GetSignedDistanceToPlane(plane, point) < 0.0f)
        {
            return false;
        }
    }

    return true;
}

bool Frustum::IsOverlapping(const AABB3& bounds) const
{
    for (const Plane3& plane : m_planes)
    {
        const Vec3 positiveVertex = GetPositiveVertex(bounds, plane.m_normal);
        if (GetSignedDistanceToPlane(plane, positiveVertex) < 0.0f)
        {
            return false;
        }
    }

    return true;
}

const Plane3& Frustum::GetPlane(PlaneIndex index) const
{
    return m_planes[index];
}
