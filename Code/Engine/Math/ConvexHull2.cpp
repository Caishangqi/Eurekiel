#include "ConvexHull2.hpp"
#include "ConvexPoly2.hpp"
#include "MathUtils.hpp"
#include <cmath>

ConvexHull2::ConvexHull2(const std::vector<Plane2>& planes)
    : m_planes(planes)
{
}

ConvexHull2::ConvexHull2(const ConvexPoly2& poly)
{
    const auto& verts = poly.m_vertexPositionsCCW;
    int vertCount = static_cast<int>(verts.size());
    if (vertCount < 3)
        return;

    m_planes.reserve(vertCount);
    for (int i = 0; i < vertCount; ++i)
    {
        const Vec2& start = verts[i];
        const Vec2& end = verts[(i + 1) % vertCount];
        m_planes.push_back(Plane2::FromEdge(start, end));
    }
}

bool ConvexHull2::IsPointInside(const Vec2& point) const
{
    return IsPointInside(point, *this);
}

RaycastResult2D ConvexHull2::Raycast(const Vec2& start, const Vec2& direction, float maxDist) const
{
    return Raycast(start, direction, maxDist, *this);
}

bool ConvexHull2::IsPointInside(const Vec2& point, const ConvexHull2& hull)
{
    // Point is inside if it's behind all planes (negative signed distance)
    for (const Plane2& plane : hull.m_planes)
    {
        if (plane.IsPointInFrontOfPlane(point))
            return false;
    }
    return true;
}

RaycastResult2D ConvexHull2::Raycast(const Vec2& start, const Vec2& direction,
                                      float maxDist, const ConvexHull2& hull)
{
    RaycastResult2D result;
    result.m_rayStartPos = start;
    result.m_rayFwdNormal = direction;
    result.m_rayMaxLength = maxDist;

    if (!hull.IsValid())
        return result;

    // Slabs algorithm for convex hull raycast
    float tEnter = 0.0f;
    float tExit = maxDist;
    int enterPlaneIndex = -1;

    for (int i = 0; i < static_cast<int>(hull.m_planes.size()); ++i)
    {
        const Plane2& plane = hull.m_planes[i];
        float denom = DotProduct2D(direction, plane.m_normal);
        float dist = plane.GetSignedDistance(start);

        // Ray parallel to plane
        if (fabsf(denom) < 1e-6f)
        {
            if (dist > 0.0f)
                return result;  // Start outside, parallel -> miss
            continue;
        }

        float t = -dist / denom;

        if (denom < 0.0f)
        {
            // Entering plane (ray going into interior)
            if (t > tEnter)
            {
                tEnter = t;
                enterPlaneIndex = i;
            }
        }
        else
        {
            // Exiting plane (ray leaving interior)
            if (t < tExit)
            {
                tExit = t;
            }
        }
    }

    // Check for valid intersection
    if (tEnter > tExit)
        return result;  // No overlap
    if (tEnter < 0.0f || tEnter > maxDist)
        return result;  // Outside ray bounds

    // Midpoint validation for numerical stability
    float tMid = (tEnter + tExit) * 0.5f;
    Vec2 midPoint = start + direction * tMid;
    if (!IsPointInside(midPoint, hull))
        return result;  // False positive

    result.m_didImpact = true;
    result.m_impactDist = tEnter;
    result.m_impactPos = start + direction * tEnter;
    result.m_impactPlaneIndex = enterPlaneIndex;

    // Handle normal: if start is inside (enterPlaneIndex == -1), use ray direction as normal
    if (enterPlaneIndex >= 0)
    {
        result.m_impactNormal = hull.m_planes[enterPlaneIndex].m_normal;
    }
    else
    {
        // Start inside hull - use ray direction as "impact normal"
        result.m_impactNormal = direction;
    }

    return result;
}

int ConvexHull2::GetPlaneCount() const
{
    return static_cast<int>(m_planes.size());
}

const Plane2& ConvexHull2::GetPlane(int index) const
{
    return m_planes[index];
}

bool ConvexHull2::IsValid() const
{
    return m_planes.size() >= 3;
}
