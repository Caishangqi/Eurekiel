#include "Plane3.hpp"

#include "MathUtils.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/VertexUtils.hpp"

Plane3::Plane3(const Vec3& normal, float distToPlaneAlongNormalFromOrigin): m_normal(normal), m_distToPlaneAlongNormalFromOrigin(distToPlaneAlongNormalFromOrigin)
{
}

Plane3::~Plane3()
{
}

Plane3::Plane3(const Plane3& copyFrom)
{
    m_normal   = copyFrom.m_normal;
    m_distance = copyFrom.m_distance;
}

Vec3 Plane3::GetNearestPoint(const Vec3& point) const
{
    return GetNearestPoint(point, *this);
}

Vec3 Plane3::GetCenter() const
{
    return GetCenter(*this);
}

bool Plane3::IsPointInFrontOfPlane(const Vec3& point) const
{
    return IsPointInFrontOfPlane(point, *this);
}

bool Plane3::IsOverlapping(const OBB3& other) const
{
    return IsOverlapping(*this, other);
}

bool Plane3::IsOverlapping(const Sphere& other) const
{
    return IsOverlapping(*this, other);
}

bool Plane3::IsOverlapping(const AABB3& other) const
{
    return IsOverlapping(*this, other);
}

RaycastResult3D Plane3::Raycast(const Vec3 startPos, const Vec3 fwdNormal, float maxDist) const
{
    return Raycast(startPos, fwdNormal, maxDist, *this);
}


void Plane3::AddVerts(std::vector<Vertex_PCU>& verts, const IntVec2& dimensions, const float thickness, const Rgba8& colorX, const Rgba8& colorY)
{
    AddVerts(verts, *this, dimensions, thickness, colorX, colorY);
}

bool Plane3::IsPointInFrontOfPlane(const Vec3& point, const Plane3& plane3)
{
    // Compute signed distance from the point to the plane
    float signedDistance = DotProduct3D(plane3.m_normal, point) - plane3.m_distToPlaneAlongNormalFromOrigin;
    // If the distance is positive, the point is in front of the plane (in the direction of the normal)
    return signedDistance > 0.f;
}

RaycastResult3D Plane3::Raycast(const Vec3 startPos, const Vec3 fwdNormal, float maxDist, const Plane3& plane)
{
    RaycastResult3D result;
    result.m_didImpact    = false;
    result.m_rayStartPos  = startPos;
    result.m_rayFwdNormal = fwdNormal;
    result.m_rayMaxLength = maxDist;

    // Dot product between ray direction and plane normal
    float nDotFwd = DotProduct3D(plane.m_normal, fwdNormal);

    // If the ray is parallel to the plane (no intersection)
    if (fabsf(nDotFwd) < 1e-6f)
    {
        return result;
    }

    // Compute intersection time t along the ray
    float t = (plane.m_distToPlaneAlongNormalFromOrigin - DotProduct3D(plane.m_normal, startPos)) / nDotFwd;

    // If intersection is outside of ray bounds
    if (t < 0.f || t > maxDist)
    {
        return result;
    }

    if (plane.IsPointInFrontOfPlane(startPos))
    {
        result.m_impactNormal = plane.m_normal;
    }
    else
    {
        result.m_impactNormal = -plane.m_normal;
    }

    result.m_didImpact  = true;
    result.m_impactDist = t;
    result.m_impactPos  = startPos + t * fwdNormal;

    return result;
}

bool Plane3::IsOverlapping(const Plane3& plane, const OBB3& other)
{
    UNUSED(plane)
    UNUSED(other)
    return false;
}

bool Plane3::IsOverlapping(const Plane3& plane, const Sphere& other)
{
    UNUSED(plane)
    UNUSED(other)
    return false;
}

bool Plane3::IsOverlapping(const Plane3& plane, const AABB3& other)
{
    UNUSED(plane)
    UNUSED(other)
    return false;
}

void Plane3::AddVerts(std::vector<Vertex_PCU>& verts, const Plane3& plane3, const IntVec2& dimensions, const float thickness, const Rgba8& colorX, const Rgba8& colorY)
{
    Vec3 iBasis    = plane3.m_normal;
    Vec3 arbitrary = (fabsf(iBasis.z) < 0.99f) ? Vec3(0, 0, 1) : Vec3(0, 1, 0);
    Vec3 jBasis    = CrossProduct3D(iBasis, arbitrary).GetNormalized();
    Vec3 kBasis    = CrossProduct3D(jBasis, iBasis).GetNormalized();

    Vec3 center = plane3.GetCenter();

    for (int i = -dimensions.x; i <= dimensions.x; ++i)
    {
        Vec3 start = center + jBasis * static_cast<float>(i) + kBasis * -static_cast<float>(dimensions.y);
        AddVertsForCylinder3D(verts, start, start + kBasis * static_cast<float>(dimensions.y) * 2.f, thickness, colorY, AABB2::ZERO_TO_ONE, 3);
    }

    for (int i = -dimensions.y; i <= dimensions.y; ++i)
    {
        Vec3 start = center + kBasis * static_cast<float>(i) + jBasis * -static_cast<float>(dimensions.x);
        AddVertsForCylinder3D(verts, start, start + jBasis * static_cast<float>(dimensions.x) * 2.f, thickness, colorX, AABB2::ZERO_TO_ONE, 3);
    }

    // Used for debug and development
    //AddVertsForCylinder3D(verts, center, center + jBasis, thickness, colorX, AABB2::ZERO_TO_ONE, 3);
    //AddVertsForCylinder3D(verts, center, center + kBasis, thickness, colorY, AABB2::ZERO_TO_ONE, 3);
}

Vec3 Plane3::GetNearestPoint(const Vec3& point, const Plane3& plane3)
{
    float distanceFromPlane = DotProduct3D(plane3.m_normal, point) - plane3.m_distToPlaneAlongNormalFromOrigin;
    return point - distanceFromPlane * plane3.m_normal;
}

Vec3 Plane3::GetCenter(const Plane3& plane3)
{
    return plane3.m_normal * plane3.m_distToPlaneAlongNormalFromOrigin;
}
