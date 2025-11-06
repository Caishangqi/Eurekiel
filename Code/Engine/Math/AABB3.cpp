#include "AABB3.hpp"

#include <algorithm>

#include "AABB2.hpp"
#include "MathUtils.hpp"
#include "Plane3.hpp"
#include "Engine/Core/Vertex_PCU.hpp"

AABB3::~AABB3()
{
}

AABB3 AABB3::BuildVertices(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned int>& outIndices, const Rgba8& color, const AABB2& uv)
{
    std::vector<Vec3> corners = GetCorners();

    static const int faces[6][4] = {
        {4, 5, 6, 7}, // +X
        {1, 0, 3, 2}, // -X
        {7, 6, 2, 3}, // +Y
        {1, 5, 4, 0}, // -Y
        {0, 4, 7, 3}, // +Z
        {5, 1, 2, 6} // -Z
    };

    static const Vec2 quadUVs[4] = {
        Vec2(uv.m_mins.x, uv.m_mins.y), // bottomLeft
        Vec2(uv.m_maxs.x, uv.m_mins.y), // bottomRight
        Vec2(uv.m_maxs.x, uv.m_maxs.y), // topRight
        Vec2(uv.m_mins.x, uv.m_maxs.y) // topLeft
    };

    unsigned int baseIndex = static_cast<unsigned int>(outVerts.size());

    for (int face = 0; face < 6; ++face)
    {
        Vec3 p0 = corners[faces[face][0]];
        Vec3 p1 = corners[faces[face][1]];
        Vec3 p2 = corners[faces[face][2]];
        Vec3 p3 = corners[faces[face][3]];

        Vec2 uv0 = quadUVs[0];
        Vec2 uv1 = quadUVs[1];
        Vec2 uv2 = quadUVs[2];
        Vec2 uv3 = quadUVs[3];

        Vec3 edge1    = p1 - p0;
        Vec3 edge2    = p2 - p0;
        Vec2 deltaUV1 = uv1 - uv0;
        Vec2 deltaUV2 = uv2 - uv0;

        float f         = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
        Vec3  tangent   = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
        Vec3  bitangent = f * (-deltaUV2.x * edge1 + deltaUV1.x * edge2);
        Vec3  normal    = CrossProduct3D(tangent, bitangent).GetNormalized();

        tangent   = tangent.GetNormalized();
        bitangent = bitangent.GetNormalized();

        outVerts.emplace_back(p0, color, uv0, normal, tangent, bitangent);
        outVerts.emplace_back(p1, color, uv1, normal, tangent, bitangent);
        outVerts.emplace_back(p2, color, uv2, normal, tangent, bitangent);
        outVerts.emplace_back(p3, color, uv3, normal, tangent, bitangent);

        outIndices.push_back(baseIndex + 0);
        outIndices.push_back(baseIndex + 1);
        outIndices.push_back(baseIndex + 2);

        outIndices.push_back(baseIndex + 0);
        outIndices.push_back(baseIndex + 2);
        outIndices.push_back(baseIndex + 3);

        baseIndex += 4;
    }

    return *this;
}

void AABB3::BuildVertices(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned int>& outIndices, AABB3& aabb3, const Rgba8& color, const AABB2& uv)
{
    aabb3.BuildVertices(outVerts, outIndices, color, uv);
}

std::vector<Vertex_PCUTBN> AABB3::GetVertices(const Rgba8& color, const AABB2& uv)
{
    std::vector<Vertex_PCUTBN> vertices;
    std::vector<unsigned int>  indices;
    BuildVertices(vertices, indices, color, uv);
    return vertices;
}

std::vector<Vertex_PCUTBN> AABB3::GetVertices(AABB3& aabb3, const Rgba8& color, const AABB2& uv)
{
    return aabb3.GetVertices(color, uv);
}

std::vector<unsigned int> AABB3::GetIndices()
{
    std::vector<Vertex_PCUTBN> vertices;
    std::vector<unsigned int>  indices;
    BuildVertices(vertices, indices);
    return indices;
}

std::vector<unsigned int> AABB3::GetIndices(AABB3& aabb3)
{
    return aabb3.GetIndices();
}

bool AABB3::IsPointInside(const Vec3& point) const
{
    return (point.x >= m_mins.x && point.x <= m_maxs.x) &&
        (point.y >= m_mins.y && point.y <= m_maxs.y) &&
        (point.z >= m_mins.z && point.z <= m_maxs.z);
}

std::vector<Vec3> AABB3::GetCorners() const
{
    std::vector<Vec3> corners;
    Vec3              dimensions = m_maxs - m_mins;
    corners.reserve(8);
    corners.emplace_back(m_mins.x, m_mins.y, m_mins.z + dimensions.z);
    corners.emplace_back(m_mins.x, m_mins.y, m_mins.z);
    corners.emplace_back(m_mins.x, m_mins.y + dimensions.y, m_mins.z);
    corners.emplace_back(m_mins.x, m_mins.y + dimensions.y, m_mins.z + dimensions.z);

    corners.emplace_back(m_mins.x + dimensions.x, m_mins.y, m_mins.z + dimensions.z);
    corners.emplace_back(m_mins.x + dimensions.x, m_mins.y, m_mins.z);
    corners.emplace_back(m_mins.x + dimensions.x, m_mins.y + dimensions.y, m_mins.z);
    corners.emplace_back(m_mins.x + dimensions.x, m_mins.y + dimensions.y, m_mins.z + dimensions.z);
    return corners;
}

const Vec3 AABB3::GetCenter() const
{
    return m_mins + ((m_maxs - m_mins) / 2);
}

const Vec3 AABB3::GetDimensions() const
{
    return m_maxs - m_mins;
}

bool AABB3::IsOverlapping(const Plane3& other) const
{
    return IsOverlapping(*this, other);
}

RaycastResult3D AABB3::Raycast(Vec3 startPos, Vec3 fwdNormal, float maxDist) const
{
    return Raycast(startPos, fwdNormal, maxDist, *this);
}

bool AABB3::IsOverlapping(const AABB3& aabb3, const Plane3& other)
{
    int numOfFront = 0;
    int numOfBack  = 0;
    for (Vec3& corner : aabb3.GetCorners())
    {
        other.IsPointInFrontOfPlane(corner) ? numOfFront += 1 : numOfBack += 1;
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

RaycastResult3D AABB3::Raycast(Vec3 startPos, Vec3 fwdNormal, float maxDist, const AABB3& aabb3)
{
    RaycastResult3D result;
    result.m_didImpact    = false;
    result.m_rayStartPos  = startPos;
    result.m_rayFwdNormal = fwdNormal;
    result.m_rayMaxLength = maxDist;

    // If the starting point is inside the AABB3, return collision immediately
    if (startPos.x >= aabb3.m_mins.x && startPos.x <= aabb3.m_maxs.x &&
        startPos.y >= aabb3.m_mins.y && startPos.y <= aabb3.m_maxs.y &&
        startPos.z >= aabb3.m_mins.z && startPos.z <= aabb3.m_maxs.z)
    {
        result.m_didImpact    = true;
        result.m_impactDist   = 0.f;
        result.m_impactPos    = startPos;
        result.m_impactNormal = -fwdNormal;
        return result;
    }

    // Perform slab test in world space
    constexpr float EPS  = 1e-6f;
    float           tMin = 0.f;
    float           tMax = maxDist;

    float tEntry[3] = {0.f, 0.f, 0.f};
    // Perform slab intersection test on each axis
    float minB[3] = {aabb3.m_mins.x, aabb3.m_mins.y, aabb3.m_mins.z};
    float maxB[3] = {aabb3.m_maxs.x, aabb3.m_maxs.y, aabb3.m_maxs.z};
    float orig[3] = {startPos.x, startPos.y, startPos.z};
    float dir[3]  = {fwdNormal.x, fwdNormal.y, fwdNormal.z};

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
    if (fabsf(tEntry[0] - tMin) < EPS_T && fabsf(fwdNormal.x) > EPS)
        normalLocal.x = (fwdNormal.x > 0.f) ? -1.f : 1.f;
    else if (fabsf(tEntry[1] - tMin) < EPS_T && fabsf(fwdNormal.y) > EPS)
        normalLocal.y = (fwdNormal.y > 0.f) ? -1.f : 1.f;
    else
        normalLocal.z = (fwdNormal.z > 0.f) ? -1.f : 1.f;

    result.m_impactNormal = normalLocal;

    return result;
}

void AABB3::Translate(const Vec3& translationToApply)
{
    m_mins.x += translationToApply.x;
    m_maxs.x += translationToApply.x;

    m_mins.y += translationToApply.y;
    m_maxs.y += translationToApply.y;

    m_mins.z += translationToApply.z;
    m_maxs.z += translationToApply.z;
}

void AABB3::SetCenter(const Vec3& newCenter)
{
    Vec3 displacement = newCenter - GetCenter();
    m_mins.x += displacement.x;
    m_maxs.x += displacement.x;

    m_mins.y += displacement.y;
    m_maxs.y += displacement.y;

    m_mins.z += displacement.z;
    m_maxs.z += displacement.z;
}

void AABB3::SetDimensions(const Vec3& newDimensions)
{
    Vec3 center = GetCenter();
    m_mins.x    = center.x - newDimensions.x / 2.f;
    m_mins.y    = center.y - newDimensions.y / 2.f;
    m_mins.z    = center.z - newDimensions.z / 2.f;

    m_maxs.x = center.x + newDimensions.x / 2.f;
    m_maxs.y = center.y + newDimensions.y / 2.f;
    m_maxs.z = center.z + newDimensions.z / 2.f;
}

void AABB3::StretchToIncludePoint(const Vec3& point)
{
    if (!IsPointInside(point))
    {
        if (point.x > m_maxs.x)
        {
            m_maxs.x = point.x;
        }
        if (point.y > m_maxs.y)
        {
            m_maxs.y = point.y;
        }
        if (point.z > m_maxs.z)
        {
            m_maxs.z = point.z;
        }
        if (point.x < m_mins.x)
        {
            m_mins.x = point.x;
        }
        if (point.y < m_mins.y)
        {
            m_mins.y = point.y;
        }
        if (point.z < m_mins.z)
        {
            m_mins.z = point.z;
        }
    }
}

AABB3::AABB3(const AABB3& copyFrom)
{
    m_mins = copyFrom.m_mins;
    m_maxs = copyFrom.m_maxs;
}

AABB3::AABB3(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) : m_mins(Vec3(minX, minY, minZ)), m_maxs(Vec3(maxX, maxY, maxZ))
{
}

AABB3::AABB3(const Vec3& mins, const Vec3& maxs) : m_mins(mins), m_maxs(maxs)
{
}
