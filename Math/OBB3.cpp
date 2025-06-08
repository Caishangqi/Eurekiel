#include "OBB3.hpp"

#include "EulerAngles.hpp"
#include "MathUtils.hpp"
#include "Plane3.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/VertexUtils.hpp"

OBB3::OBB3(Vec3& center, Vec3 halfDimensions, Vec3& iBases, Vec3& jBases, Vec3& kBases): m_center(center), m_halfDimensions(halfDimensions), m_iBasisNormal(iBases), m_jBasisNormal(jBases),
                                                                                         m_kBasisNormal(kBases)
{
}

OBB3::~OBB3()
{
}

OBB3::OBB3(const OBB3& copyFrom)
{
    m_center         = copyFrom.m_center;
    m_halfDimensions = copyFrom.m_halfDimensions;
    m_iBasisNormal   = copyFrom.m_iBasisNormal;
    m_jBasisNormal   = copyFrom.m_jBasisNormal;
    m_kBasisNormal   = copyFrom.m_kBasisNormal;
}

OBB3 OBB3::BuildVertices(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned int>& outIndices, const Rgba8& color, const AABB2& uv)
{
    std::vector<Vec3> coners = GetCorners();
    // -x
    AddVertsForQuad3D(outVerts, outIndices, coners[1], coners[2], coners[3], coners[0], color, uv);

    // +x
    AddVertsForQuad3D(outVerts, outIndices, coners[6], coners[5], coners[4], coners[7], color, uv);

    // -y
    AddVertsForQuad3D(outVerts, outIndices, coners[2], coners[6], coners[7], coners[3], color, uv);

    // +y
    AddVertsForQuad3D(outVerts, outIndices, coners[5], coners[1], coners[0], coners[4], color, uv);

    // -z
    AddVertsForQuad3D(outVerts, outIndices, coners[1], coners[5], coners[6], coners[2], color, uv);

    // +z
    AddVertsForQuad3D(outVerts, outIndices, coners[3], coners[7], coners[4], coners[0], color, uv);

    return *this;
}

void OBB3::BuildVertices(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned int>& outIndices, OBB3& obb3, const Rgba8& color, const AABB2& uv)
{
    obb3.BuildVertices(outVerts, outIndices, color, uv);
}

bool OBB3::IsPointInside(const Vec3& point) const
{
    return IsPointInsideOBB3(point, *this);
}

Vec3 OBB3::GetNearestPoint(const Vec3& point) const
{
    return GetNearestPointOnOBB3(point, *this);
}

std::vector<Vec3> OBB3::GetCorners() const
{
    std::vector<Vec3> corners;
    corners.reserve(8);

    // 1) Precompute three local half-axis vectors
    Vec3 i = m_iBasisNormal * m_halfDimensions.x; // +X half?extent
    Vec3 j = m_jBasisNormal * m_halfDimensions.y; // +Y half?extent
    Vec3 k = m_kBasisNormal * m_halfDimensions.z; // +Z half?extent


    // -X  side (left)
    corners.emplace_back(m_center - i + j + k); // v1
    corners.emplace_back(m_center - i + j - k); // v2
    corners.emplace_back(m_center - i - j - k); // v3
    corners.emplace_back(m_center - i - j + k); // v4

    // +X  side (right)
    corners.emplace_back(m_center + i + j + k); // v5
    corners.emplace_back(m_center + i + j - k); // v6
    corners.emplace_back(m_center + i - j - k); // v7
    corners.emplace_back(m_center + i - j + k); // v8

    return corners;
}

bool OBB3::IsOverlapping(const OBB3& other) const
{
    return DoOBB3sOverlap(*this, other);
}

bool OBB3::IsOverlapping(const Sphere& other) const
{
    return DoOBB3sAndSphereOverlap(*this, other);
}

bool OBB3::IsOverlapping(const Plane3& other) const
{
    return DoOBB3sAndPlane3Overlap(*this, other);
}

RaycastResult3D OBB3::Raycast(const Vec3 startPos, const Vec3 fwdNormal, float maxDist, const OBB3& obb3) const
{
    return RaycastVsOBB3D(startPos, fwdNormal, maxDist, obb3);
}

Vec3 OBB3::GetLocalPosForWorldPos(const Vec3& worldPosition) const
{
    return GetLocalPosForWorldPos(*this, worldPosition);
}

Vec3 OBB3::GetWorldPosForLocalPos(const Vec3& localPosition) const
{
    return GetWorldPosForLocalPos(*this, localPosition);
}

OBB3& OBB3::SetOrientation(EulerAngles angles)
{
    Vec3 i, j, k;
    angles.GetAsVectors_IFwd_JLeft_KUp(i, j, k);
    m_iBasisNormal = i;
    m_jBasisNormal = j;
    m_kBasisNormal = k;
    return *this;
}

bool OBB3::IsPointInsideOBB3(const Vec3& point, const OBB3& obb3)
{
    Vec3 pointLocal = obb3.GetLocalPosForWorldPos(point);
    if (pointLocal.x > obb3.m_halfDimensions.x)
    {
        return false;
    }
    if (pointLocal.x < -obb3.m_halfDimensions.x)
    {
        return false;
    }
    if (pointLocal.y > obb3.m_halfDimensions.y)
    {
        return false;
    }
    if (pointLocal.y < -obb3.m_halfDimensions.y)
    {
        return false;
    }
    if (pointLocal.z > obb3.m_halfDimensions.z)
    {
        return false;
    }
    if (pointLocal.z < -obb3.m_halfDimensions.z)
    {
        return false;
    }
    return true;
}

Vec3 OBB3::GetNearestPointOnOBB3(const Vec3& referencePoint, const OBB3& obb3)
{
    Vec3  pointLocal = obb3.GetLocalPosForWorldPos(referencePoint);
    float clampedX   = GetClamped(pointLocal.x, -obb3.m_halfDimensions.x, obb3.m_halfDimensions.x);
    float clampedY   = GetClamped(pointLocal.y, -obb3.m_halfDimensions.y, obb3.m_halfDimensions.y);
    float clampedZ   = GetClamped(pointLocal.z, -obb3.m_halfDimensions.z, obb3.m_halfDimensions.z);
    return obb3.GetWorldPosForLocalPos(Vec3(clampedX, clampedY, clampedZ));
}

RaycastResult3D OBB3::RaycastVsOBB3D(const Vec3 startPos, const Vec3 fwdNormal, float maxDist, const OBB3& obb)
{
    RaycastResult3D result;
    result.m_didImpact    = false;
    result.m_rayStartPos  = startPos;
    result.m_rayFwdNormal = fwdNormal;
    result.m_rayMaxLength = maxDist;

    // Transform the ray starting point to the OBB local space
    Vec3 localStart = obb.GetLocalPosForWorldPos(startPos);

    // Project the ray direction onto the local axes
    Vec3 localDir(
        DotProduct3D(fwdNormal, obb.m_iBasisNormal),
        DotProduct3D(fwdNormal, obb.m_jBasisNormal),
        DotProduct3D(fwdNormal, obb.m_kBasisNormal)
    );

    Vec3 h = obb.m_halfDimensions;

    // If the starting point is inside the OBB, return collision immediately
    if (fabsf(localStart.x) <= h.x &&
        fabsf(localStart.y) <= h.y &&
        fabsf(localStart.z) <= h.z)
    {
        result.m_didImpact    = true;
        result.m_impactDist   = 0.f;
        result.m_impactPos    = startPos;
        result.m_impactNormal = -fwdNormal;
        return result;
    }

    // Perform slab test in local space
    constexpr float EPS  = 1e-6f;
    float           tMin = 0.f;
    float           tMax = maxDist;

    float tEntry[3] = {0.f, 0.f, 0.f};

    // Perform slab intersection test on each axis
    float minB[3] = {-h.x, -h.y, -h.z};
    float maxB[3] = {h.x, h.y, h.z};
    float orig[3] = {localStart.x, localStart.y, localStart.z};
    float dir[3]  = {localDir.x, localDir.y, localDir.z};

    for (int axis = 0; axis < 3; axis++)
    {
        if (fabsf(dir[axis]) < EPS)
        {
            if (orig[axis] < minB[axis] || orig[axis] > maxB[axis])
                return result; // Parallel and starting point is outside
        }
        else
        {
            float invD = 1.f / dir[axis];
            float t1   = (minB[axis] - orig[axis]) * invD;
            float t2   = (maxB[axis] - orig[axis]) * invD;
            if (t1 > t2) std::swap(t1, t2);

            if (t2 < tMin || t1 > tMax)
                return result;

            if (t1 > tMin)
            {
                tMin      = t1;
                tEntry[0] = (axis == 0) ? t1 : tEntry[0];
                tEntry[1] = (axis == 1) ? t1 : tEntry[1];
                tEntry[2] = (axis == 2) ? t1 : tEntry[2];
            }
            if (t2 < tMax) tMax = t2;
        }
    }

    // If the intersection point is not within the valid range
    if (tMin < 0.f || tMin > maxDist)
        return result;

    // Fill in the result
    result.m_didImpact  = true;
    result.m_impactDist = tMin;
    result.m_impactPos  = startPos + fwdNormal * tMin;

    // Determine which side was hit
    constexpr float EPS_T = 1e-4f;
    Vec3            normalLocal(0.f, 0.f, 0.f);
    if (fabsf(tEntry[0] - tMin) < EPS_T && fabsf(localDir.x) > EPS)
        normalLocal.x = (localDir.x > 0.f) ? -1.f : 1.f;
    else if (fabsf(tEntry[1] - tMin) < EPS_T && fabsf(localDir.y) > EPS)
        normalLocal.y = (localDir.y > 0.f) ? -1.f : 1.f;
    else
        normalLocal.z = (localDir.z > 0.f) ? -1.f : 1.f;

    // Transform the local normal back to world coordinates
    result.m_impactNormal =
        normalLocal.x * obb.m_iBasisNormal +
        normalLocal.y * obb.m_jBasisNormal +
        normalLocal.z * obb.m_kBasisNormal;

    return result;
}


bool OBB3::DoOBB3sOverlap(const OBB3& a, const OBB3& b)
{
    std::vector<Vec3> aConers = a.GetCorners();
    for (Vec3& coner : aConers)
    {
        if (b.IsPointInside(coner))
        {
            return true;
        }
    }
    return false;
}

bool OBB3::DoOBB3sAndSphereOverlap(const OBB3& obb3, const Sphere& sphere)
{
    Vec3  nearestPoint = obb3.GetNearestPoint(sphere.m_position);
    float distanceSqr  = GetDistanceSquared3D(nearestPoint, sphere.m_position);
    if (distanceSqr < sphere.m_radius * sphere.m_radius)
    {
        return true;
    }
    return false;
}

bool OBB3::DoOBB3sAndPlane3Overlap(const OBB3& obb3, const Plane3& plane)
{
    int numOfFront = 0;
    int numOfBack  = 0;
    for (Vec3& corner : obb3.GetCorners())
    {
        plane.IsPointInFrontOfPlane(corner) ? numOfFront += 1 : numOfBack += 1;
    }
    if (numOfBack == 0 && numOfFront == 8)
    {
        return false;
    }
    if (numOfFront == 0 && numOfBack == 8)
    {
        return false;
    }
    return true;
}

Vec3 OBB3::GetLocalPosForWorldPos(const OBB3& obb3, const Vec3& worldPosition)
{
    // Vector from OBB center to world?space point
    Vec3 offset = worldPosition - obb3.m_center;

    // Project the offset onto each orthonormal basis vector
    float localX = DotProduct3D(offset, obb3.m_iBasisNormal.GetNormalized()); // +X axis
    float localY = DotProduct3D(offset, obb3.m_jBasisNormal.GetNormalized()); // +Y axis
    float localZ = DotProduct3D(offset, obb3.m_kBasisNormal.GetNormalized()); // +Z axis

    // Return local?space coordinates (DO NOT re?expand to world space!)
    return Vec3(localX, localY, localZ);
}

Vec3 OBB3::GetWorldPosForLocalPos(const OBB3& obb3, const Vec3& localPosition)
{
    // Expand local coordinates along the three basis vectors and translate
    Vec3 iWorld = obb3.m_iBasisNormal.GetNormalized() * localPosition.x;
    Vec3 jWorld = obb3.m_jBasisNormal.GetNormalized() * localPosition.y;
    Vec3 kWorld = obb3.m_kBasisNormal.GetNormalized() * localPosition.z;
    return obb3.m_center + iWorld + jWorld + kWorld;
}
