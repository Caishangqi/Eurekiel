#include "VertexUtils.hpp"

#include "EngineCommon.hpp"
#include "ErrorWarningAssert.hpp"
#include "Vertex_PCU.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/Capsule2.hpp"
#include "Engine/Math/Disc2.hpp"
#include "Engine/Math/LineSegment2.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/OBB2.hpp"
#include "Engine/Math/Triangle2.hpp"
#include "Engine/Math/ZCylinder.hpp"

void TransformVertexArrayXY3D(int         numVerts, Vertex_PCU* verts, float uniformScaleXY, float rotationDegreesAboutZ,
                              const Vec2& translationXY)
{
    for (int vertIndex = 0; vertIndex < numVerts; vertIndex++)
    {
        Vec3& pos = verts[vertIndex].m_position;
        TransformPositionXY3D(pos, uniformScaleXY, rotationDegreesAboutZ, translationXY);
    }
}

void TransformVertexArrayXY3D(std::vector<Vertex_PCU>& verts, float uniformScaleXY, float rotationDegreesAboutZ,
                              const Vec2&              translationXY)
{
    for (int vertIndex = 0; vertIndex < static_cast<int>(verts.size()); vertIndex++)
    {
        Vec3& pos = verts[vertIndex].m_position;
        TransformPositionXY3D(pos, uniformScaleXY, rotationDegreesAboutZ, translationXY);
    }
}

void AddVertsForDisc2D(std::vector<Vertex_PCU>& verts, const Disc2& disc, const Rgba8& color)
{
    int   NUM_SIDES        = 32;
    float DEGREES_PER_SIDE = 360.f / static_cast<float>(NUM_SIDES);
    for (int sideNum = 0; sideNum < NUM_SIDES; ++sideNum)
    {
        float startDegrees = DEGREES_PER_SIDE * static_cast<float>(sideNum);
        float endDegrees   = DEGREES_PER_SIDE * static_cast<float>(sideNum + 1);

        float cosStart = CosDegrees(startDegrees);
        float sinStart = SinDegrees(startDegrees);
        float cosEnd   = CosDegrees(endDegrees);
        float sinEnd   = SinDegrees(endDegrees);

        auto centerPos = Vec3(disc.m_position.x, disc.m_position.y, 0.0f);
        // use unit circle calculate length
        auto startPos = Vec3(disc.m_position.x + cosStart * disc.m_radius, disc.m_position.y + sinStart * disc.m_radius,
                             0.0f);
        auto endPos = Vec3(disc.m_position.x + cosEnd * disc.m_radius, disc.m_position.y + sinEnd * disc.m_radius,
                           0.0f);
        verts.push_back(Vertex_PCU(centerPos, color, Vec2()));
        verts.push_back(Vertex_PCU(startPos, color, Vec2()));
        verts.push_back(Vertex_PCU(endPos, color, Vec2()));
    }
}

void AddVertsForAABB2D(std::vector<Vertex_PCU>& verts, const AABB2& aabb2, const Rgba8& color)
{
    float halfX  = aabb2.GetDimensions().x / 2.0f;
    float halfY  = aabb2.GetDimensions().y / 2.0f;
    Vec2  center = aabb2.GetCenter();
    // Four conner points
    Vec2 RightUp   = center + Vec2(halfX, halfY);
    Vec2 RightDown = center + Vec2(halfX, -halfY);
    Vec2 LeftUp    = center + Vec2(-halfX, halfY);
    Vec2 LeftDown  = center + Vec2(-halfX, -halfY);

    // left side of triangle
    verts.push_back(Vertex_PCU(Vec3(LeftDown.x, LeftDown.y, 0.0f), color, Vec2(0, 0)));
    verts.push_back(Vertex_PCU(Vec3(RightUp.x, RightUp.y, 0.0f), color, Vec2(1, 1)));
    verts.push_back(Vertex_PCU(Vec3(LeftUp.x, LeftUp.y, 0.0f), color, Vec2(0, 1)));

    // right side of triangle
    verts.push_back(Vertex_PCU(Vec3(LeftDown.x, LeftDown.y, 0.0f), color, Vec2(0, 0)));
    verts.push_back(Vertex_PCU(Vec3(RightDown.x, RightDown.y, 0.0f), color, Vec2(1, 0)));
    verts.push_back(Vertex_PCU(Vec3(RightUp.x, RightUp.y, 0.0f), color, Vec2(1, 1)));
}

void AddVertsForCurve2D(std::vector<Vertex_PCU>& verts, const std::vector<Vec2>& points, const Rgba8& color, float thickness)
{
    if (points.size() < 2)
    {
        return;
    }
    float numPoints = static_cast<float>(points.size());
    for (int i = 0; (int)i < numPoints; ++i)
    {
        if (i + 1 < (int)points.size()) AddVertsForLineSegment2D(verts, LineSegment2(points[i], points[(i + 1)], thickness), color);
    }
}


void AddVertsForAABB2D(std::vector<Vertex_PCU>& verts, const AABB2& aabb2, const Rgba8& color,
                       const Vec2&              uvMin = Vec2::ZERO,
                       const Vec2&              uvMax = Vec2::ONE)
{
    float halfX  = aabb2.GetDimensions().x / 2.0f;
    float halfY  = aabb2.GetDimensions().y / 2.0f;
    Vec2  center = aabb2.GetCenter();
    // Four conner points
    Vec2 RightUp   = center + Vec2(halfX, halfY);
    Vec2 RightDown = center + Vec2(halfX, -halfY);
    Vec2 LeftUp    = center + Vec2(-halfX, halfY);
    Vec2 LeftDown  = center + Vec2(-halfX, -halfY);

    // left side of triangle
    verts.push_back(Vertex_PCU(Vec3(LeftDown.x, LeftDown.y, 0.0f), color, uvMin));
    verts.push_back(Vertex_PCU(Vec3(RightUp.x, RightUp.y, 0.0f), color, uvMax));
    verts.push_back(Vertex_PCU(Vec3(LeftUp.x, LeftUp.y, 0.0f), color, Vec2(uvMin.x, uvMax.y)));

    // right side of triangle
    verts.push_back(Vertex_PCU(Vec3(LeftDown.x, LeftDown.y, 0.0f), color, uvMin));
    verts.push_back(Vertex_PCU(Vec3(RightDown.x, RightDown.y, 0.0f), color, Vec2(uvMax.x, uvMin.y)));
    verts.push_back(Vertex_PCU(Vec3(RightUp.x, RightUp.y, 0.0f), color, uvMax));
}

void AddVertsForOBB2D(std::vector<Vertex_PCU>& verts, const OBB2& obb2, const Rgba8& color)
{
    Vec2 localCorners[4] = {
        Vec2(-obb2.m_halfDimensions.x, -obb2.m_halfDimensions.y),
        Vec2(obb2.m_halfDimensions.x, -obb2.m_halfDimensions.y),
        Vec2(obb2.m_halfDimensions.x, obb2.m_halfDimensions.y),
        Vec2(-obb2.m_halfDimensions.x, obb2.m_halfDimensions.y)
    };

    Vec2 worldCorners[4];
    for (int i = 0; i < 4; ++i)
    {
        worldCorners[i] = obb2.m_center + (localCorners[i].x * obb2.m_iBasisNormal) + (localCorners[i].y * obb2.
                                                                                                           m_iBasisNormal.GetRotated90Degrees());
    }

    // Create two triangles to represent the OBB
    verts.push_back(Vertex_PCU(Vec3(worldCorners[0].x, worldCorners[0].y, 0.0f), color, Vec2()));
    verts.push_back(Vertex_PCU(Vec3(worldCorners[1].x, worldCorners[1].y, 0.0f), color, Vec2()));
    verts.push_back(Vertex_PCU(Vec3(worldCorners[2].x, worldCorners[2].y, 0.0f), color, Vec2()));

    verts.push_back(Vertex_PCU(Vec3(worldCorners[0].x, worldCorners[0].y, 0.0f), color, Vec2()));
    verts.push_back(Vertex_PCU(Vec3(worldCorners[2].x, worldCorners[2].y, 0.0f), color, Vec2()));
    verts.push_back(Vertex_PCU(Vec3(worldCorners[3].x, worldCorners[3].y, 0.0f), color, Vec2()));
}

void AddVertsForCapsule2D(std::vector<Vertex_PCU>& verts, const Capsule2& capsule, const Rgba8& color)
{
    // Add vertices for the rectangular middle part of the capsule
    Vec2 forwardDir  = (capsule.m_end - capsule.m_start).GetNormalized(); // SE
    Vec2 backwardDir = (capsule.m_start - capsule.m_end).GetNormalized(); // ES
    Vec2 leftDir     = forwardDir.GetRotated90Degrees(); //EL
    // End part half disc
    float orientedDegreesEnd = forwardDir.GetRotatedMinus90Degrees().GetOrientationDegrees();
    int   NUM_SIDES          = 16;
    float DEGREES_PER_SIDE   = 180.f / static_cast<float>(NUM_SIDES);
    for (int sideNum = 0; sideNum < NUM_SIDES; ++sideNum)
    {
        float startDegrees = orientedDegreesEnd + DEGREES_PER_SIDE * static_cast<float>(sideNum);
        float endDegrees   = orientedDegreesEnd + DEGREES_PER_SIDE * static_cast<float>(sideNum + 1);

        float cosStart = CosDegrees(startDegrees);
        float sinStart = SinDegrees(startDegrees);
        float cosEnd   = CosDegrees(endDegrees);
        float sinEnd   = SinDegrees(endDegrees);

        auto centerPos = Vec3(capsule.m_end.x, capsule.m_end.y, 0.0f);
        auto startPos  = Vec3(centerPos.x + cosStart * capsule.m_radius, centerPos.y + sinStart * capsule.m_radius,
                              0.0f);
        auto endPos = Vec3(centerPos.x + cosEnd * capsule.m_radius, centerPos.y + sinEnd * capsule.m_radius,
                           0.0f);
        verts.push_back(Vertex_PCU(centerPos, color, Vec2()));
        verts.push_back(Vertex_PCU(startPos, color, Vec2()));
        verts.push_back(Vertex_PCU(endPos, color, Vec2()));
    }
    // Start part half disc
    float orientedDegreesStart = backwardDir.GetRotatedMinus90Degrees().GetOrientationDegrees();
    for (int sideNum = 0; sideNum < NUM_SIDES; ++sideNum)
    {
        float startDegrees = orientedDegreesStart + DEGREES_PER_SIDE * static_cast<float>(sideNum);
        float endDegrees   = orientedDegreesStart + DEGREES_PER_SIDE * static_cast<float>(sideNum + 1);

        float cosStart = CosDegrees(startDegrees);
        float sinStart = SinDegrees(startDegrees);
        float cosEnd   = CosDegrees(endDegrees);
        float sinEnd   = SinDegrees(endDegrees);

        auto centerPos = Vec3(capsule.m_start.x, capsule.m_start.y, 0.0f);
        auto startPos  = Vec3(centerPos.x + cosStart * capsule.m_radius, centerPos.y + sinStart * capsule.m_radius,
                              0.0f);
        auto endPos = Vec3(centerPos.x + cosEnd * capsule.m_radius, centerPos.y + sinEnd * capsule.m_radius,
                           0.0f);
        verts.push_back(Vertex_PCU(centerPos, color, Vec2()));
        verts.push_back(Vertex_PCU(startPos, color, Vec2()));
        verts.push_back(Vertex_PCU(endPos, color, Vec2()));
    }
    // Box
    Vec2 p1 = capsule.m_start + leftDir * capsule.m_radius;
    Vec2 p2 = capsule.m_start - leftDir * capsule.m_radius;
    Vec2 p3 = capsule.m_end + leftDir * capsule.m_radius;
    Vec2 p4 = capsule.m_end - leftDir * capsule.m_radius;

    verts.push_back(Vertex_PCU(Vec3(p2.x, p2.y, 0.0f), color, Vec2()));
    verts.push_back(Vertex_PCU(Vec3(p4.x, p4.y, 0.0f), color, Vec2()));
    verts.push_back(Vertex_PCU(Vec3(p3.x, p3.y, 0.0f), color, Vec2()));

    verts.push_back(Vertex_PCU(Vec3(p2.x, p2.y, 0.0f), color, Vec2()));
    verts.push_back(Vertex_PCU(Vec3(p3.x, p3.y, 0.0f), color, Vec2()));
    verts.push_back(Vertex_PCU(Vec3(p1.x, p1.y, 0.0f), color, Vec2()));
}

void AddVertsForTriangle2D(std::vector<Vertex_PCU>& verts, const Triangle2& triangle, const Rgba8& color)
{
    for (Vec2 m_position_counter_clockwise : triangle.m_positionCounterClockwise)
    {
        verts.push_back(Vertex_PCU(Vec3(m_position_counter_clockwise.x, m_position_counter_clockwise.y, 0.0f), color,
                                   Vec2()));
    }
}

void AddVertsForLineSegment2D(std::vector<Vertex_PCU>& verts, const LineSegment2& lineSegment, const Rgba8& color)
{
    Vec2  dx            = lineSegment.m_end - lineSegment.m_start;
    float halfThickness = lineSegment.m_thickness * 0.5f;

    Vec2 stepForward = halfThickness * dx.GetNormalized();
    Vec2 stepLeft    = stepForward.GetRotated90Degrees();

    Vec2 EL = lineSegment.m_end + stepForward + stepLeft;
    Vec2 ER = lineSegment.m_end + stepForward - stepLeft;
    Vec2 SL = lineSegment.m_start - stepForward + stepLeft;
    Vec2 SR = lineSegment.m_start - stepForward - stepLeft;

    verts.push_back(Vertex_PCU(Vec3(SR.x, SR.y, 0.f), color, Vec2()));
    verts.push_back(Vertex_PCU(Vec3(EL.x, EL.y, 0.f), color, Vec2()));
    verts.push_back(Vertex_PCU(Vec3(SL.x, SL.y, 0.f), color, Vec2()));
    verts.push_back(Vertex_PCU(Vec3(SR.x, SR.y, 0.f), color, Vec2()));
    verts.push_back(Vertex_PCU(Vec3(ER.x, ER.y, 0.f), color, Vec2()));
    verts.push_back(Vertex_PCU(Vec3(EL.x, EL.y, 0.f), color, Vec2()));
}

void AddVertsForArrow2D(std::vector<Vertex_PCU>& verts, Vec2 tailPos, Vec2 tipPos, float arrowSize, float lineThickness,
                        const Rgba8&             color)
{
    LineSegment2 lineSegment;
    lineSegment.m_start     = tailPos;
    lineSegment.m_thickness = lineThickness;
    lineSegment.m_end       = tipPos;
    AddVertsForLineSegment2D(verts, lineSegment, color);

    Vec2 p_ES = (tailPos - tipPos).GetNormalized();
    Vec2 p_EA = p_ES.GetRotatedDegrees(45);
    Vec2 p_EB = p_ES.GetRotatedDegrees(-45);

    Vec2 pointA = tipPos + p_EA * arrowSize;
    Vec2 pointB = tipPos + p_EB * arrowSize;

    LineSegment2 lineSegmentEA;
    lineSegmentEA.m_start     = tipPos;
    lineSegmentEA.m_thickness = lineThickness;
    lineSegmentEA.m_end       = pointA;
    AddVertsForLineSegment2D(verts, lineSegmentEA, color);

    LineSegment2 lineSegmentEB;
    lineSegmentEB.m_start     = tipPos;
    lineSegmentEB.m_thickness = lineThickness;
    lineSegmentEB.m_end       = pointB;
    AddVertsForLineSegment2D(verts, lineSegmentEB, color);
}

void AddVertsForRoundedQuad3D(std::vector<Vertex_PCUTBN>& vertexes, const Vec3& bottomLeft, const Vec3& bottomRight, const Vec3& topRight, const Vec3& topLeft, const Rgba8& color, const AABB2& UVs)
{
    Vec3 u = bottomRight - bottomLeft;
    Vec3 v = topLeft - bottomLeft;
    Vec3 n = CrossProduct3D(u, v).GetNormalized();

    Vec3 topMiddlePoint    = Interpolate(topLeft, topRight, 0.5f);
    Vec3 bottomMiddlePoint = Interpolate(bottomLeft, bottomRight, 0.5f);

    /// Left part
    vertexes.push_back(Vertex_PCUTBN(bottomLeft, color, UVs.m_mins, -n));
    vertexes.push_back(Vertex_PCUTBN(bottomMiddlePoint, color, Vec2((UVs.m_maxs.x + UVs.m_mins.x) * 0.5f, UVs.m_mins.y), n));
    vertexes.push_back(Vertex_PCUTBN(topMiddlePoint, color, Vec2((UVs.m_maxs.x + UVs.m_mins.x) * 0.5f, UVs.m_maxs.y), n));

    vertexes.push_back(Vertex_PCUTBN(bottomLeft, color, UVs.m_mins, -n));
    vertexes.push_back(Vertex_PCUTBN(topMiddlePoint, color, Vec2((UVs.m_maxs.x + UVs.m_mins.x) * 0.5f, UVs.m_maxs.y), n));
    vertexes.push_back(Vertex_PCUTBN(topLeft, color, Vec2(UVs.m_mins.x, UVs.m_maxs.y), -n));
    /// Right part
    vertexes.push_back(Vertex_PCUTBN(bottomMiddlePoint, color, Vec2((UVs.m_maxs.x + UVs.m_mins.x) * 0.5f, UVs.m_mins.y), n));
    vertexes.push_back(Vertex_PCUTBN(bottomRight, color, Vec2(UVs.m_maxs.x, UVs.m_mins.y), n));
    vertexes.push_back(Vertex_PCUTBN(topRight, color, UVs.m_maxs, n));

    vertexes.push_back(Vertex_PCUTBN(bottomMiddlePoint, color, Vec2((UVs.m_maxs.x + UVs.m_mins.x) * 0.5f, UVs.m_mins.y), n));
    vertexes.push_back(Vertex_PCUTBN(topRight, color, UVs.m_maxs, n));
    vertexes.push_back(Vertex_PCUTBN(topMiddlePoint, color, Vec2((UVs.m_maxs.x + UVs.m_mins.x) * 0.5f, UVs.m_maxs.y), n));
}

void AddVertsForQuad3D(std::vector<Vertex_PCU>& verts, const Vec3& bottomLeft, const Vec3& bottomRight, const Vec3& topRight, const Vec3& topLeft, const Rgba8& color, const AABB2& UVs)
{
    verts.push_back(Vertex_PCU(bottomLeft, color, UVs.m_mins));
    verts.push_back(Vertex_PCU(bottomRight, color, Vec2(UVs.m_maxs.x, UVs.m_mins.y)));
    verts.push_back(Vertex_PCU(topRight, color, UVs.m_maxs));

    verts.push_back(Vertex_PCU(bottomLeft, color, UVs.m_mins));
    verts.push_back(Vertex_PCU(topRight, color, UVs.m_maxs));
    verts.push_back(Vertex_PCU(topLeft, color, Vec2(UVs.m_mins.x, UVs.m_maxs.y)));
}

void AddVertsForQuad3D(std::vector<Vertex_PCUTBN>& verts, const Vec3& bottomLeft, const Vec3& bottomRight, const Vec3& topRight, const Vec3& topLeft, const Rgba8& color, const AABB2& UVs)
{
    Vec3 u = bottomRight - bottomLeft;
    Vec3 v = topLeft - bottomLeft;
    Vec3 n = CrossProduct3D(u, v).GetNormalized();

    verts.push_back(Vertex_PCUTBN(bottomLeft, color, UVs.m_mins, n));
    verts.push_back(Vertex_PCUTBN(bottomRight, color, Vec2(UVs.m_maxs.x, UVs.m_mins.y)));
    verts.push_back(Vertex_PCUTBN(topRight, color, UVs.m_maxs, n));

    verts.push_back(Vertex_PCUTBN(bottomLeft, color, UVs.m_mins, n));
    verts.push_back(Vertex_PCUTBN(topRight, color, UVs.m_maxs, n));
    verts.push_back(Vertex_PCUTBN(topLeft, color, Vec2(UVs.m_mins.x, UVs.m_maxs.y), n));
}

void AddVertsForQuad3D(std::vector<Vertex_PCU>& outVerts, std::vector<unsigned>& outIndices, const Vec3& bottomLeft, const Vec3& bottomRight, const Vec3& topRight, const Vec3& topLeft,
                       const Rgba8&             color, const AABB2&              uv)
{
    unsigned int startIndex = static_cast<unsigned int>(outVerts.size());

    outVerts.push_back(Vertex_PCU(bottomLeft, color, Vec2(uv.m_mins.x, uv.m_mins.y)));
    outVerts.push_back(Vertex_PCU(bottomRight, color, Vec2(uv.m_maxs.x, uv.m_mins.y)));
    outVerts.push_back(Vertex_PCU(topRight, color, Vec2(uv.m_maxs.x, uv.m_maxs.y)));
    outVerts.push_back(Vertex_PCU(topLeft, color, Vec2(uv.m_mins.x, uv.m_maxs.y)));

    outIndices.push_back(startIndex + 0);
    outIndices.push_back(startIndex + 1);
    outIndices.push_back(startIndex + 2);

    outIndices.push_back(startIndex + 0);
    outIndices.push_back(startIndex + 2);
    outIndices.push_back(startIndex + 3);
}

void AddVertsForQuad3D(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned>& outIndices, const Vec3& bottomLeft, const Vec3& bottomRight, const Vec3& topRight, const Vec3& topLeft,
                       const Rgba8&                color, const AABB2&              uv)
{
    unsigned int startIndex = static_cast<unsigned int>(outVerts.size());

    Vec3 u = bottomRight - bottomLeft;
    Vec3 v = topLeft - bottomLeft;
    Vec3 n = CrossProduct3D(u, v).GetNormalized();

    outVerts.push_back(Vertex_PCUTBN(bottomLeft, color, Vec2(uv.m_mins.x, uv.m_mins.y), n));
    outVerts.push_back(Vertex_PCUTBN(bottomRight, color, Vec2(uv.m_maxs.x, uv.m_mins.y), n));
    outVerts.push_back(Vertex_PCUTBN(topRight, color, Vec2(uv.m_maxs.x, uv.m_maxs.y), n));
    outVerts.push_back(Vertex_PCUTBN(topLeft, color, Vec2(uv.m_mins.x, uv.m_maxs.y), n));

    outIndices.push_back(startIndex + 0);
    outIndices.push_back(startIndex + 1);
    outIndices.push_back(startIndex + 2);

    outIndices.push_back(startIndex + 0);
    outIndices.push_back(startIndex + 2);
    outIndices.push_back(startIndex + 3);
}

void TransformVertexArray3D(std::vector<Vertex_PCU>& verts, const Mat44& transform)
{
    for (Vertex_PCU& vert : verts)
    {
        vert.m_position = transform.TransformPosition3D(vert.m_position);
    }
}

AABB2 GetVertexBounds2D(const std::vector<Vertex_PCU>& verts)
{
    AABB2 bounds;
    if ((int)verts.size() <= 0)
    {
        return AABB2();
    }
    bounds.m_mins = Vec2(verts[0].m_position.x, verts[0].m_position.y);
    bounds.m_maxs = bounds.m_mins;
    for (const Vertex_PCU& vert : verts)
    {
        if (vert.m_position.x > bounds.m_maxs.x)
            bounds.m_maxs.x = vert.m_position.x;
        if (vert.m_position.y > bounds.m_maxs.y)
            bounds.m_maxs.y = vert.m_position.y;
        if (vert.m_position.x < bounds.m_mins.x)
            bounds.m_mins.x = vert.m_position.x;
        if (vert.m_position.y < bounds.m_mins.y)
            bounds.m_mins.y = vert.m_position.y;
    }
    return bounds;
}

void AddVertsForCylinder3D(std::vector<Vertex_PCU>& verts, const Vec3& apex, const Vec3& baseCenter, float radius, const Rgba8& color, const AABB2& UVs, int numSlices)
{
    UNUSED(UVs)
    Vec3  axis           = baseCenter - apex;
    float cylinderLength = axis.GetLength();
    if (cylinderLength <= 0.f)
    {
        return;
    }
    Vec3 forward = axis.GetNormalized();

    Vec3 worldUp(0.f, 0.f, 1.f);
    if (fabsf(DotProduct3D(forward, worldUp)) > 0.99f)
    {
        worldUp = Vec3(1.f, 0.f, 0.f);
    }

    // right = forward × worldUp, up = right × forward
    Vec3 right = CrossProduct3D(forward, worldUp).GetNormalized();
    Vec3 up    = CrossProduct3D(right, forward).GetNormalized();

    float angleStep = 360.f / static_cast<float>(numSlices);

    for (int i = 0; i < numSlices; ++i)
    {
        float angleA = angleStep * (float)i;
        float angleB = angleStep * (float)(i + 1);

        float cosA = CosDegrees(angleA);
        float sinA = SinDegrees(angleA);
        float cosB = CosDegrees(angleB);
        float sinB = SinDegrees(angleB);

        Vec3 offsetA = (right * cosA + up * sinA) * radius;
        Vec3 offsetB = (right * cosB + up * sinB) * radius;

        Vec3 p0 = baseCenter + offsetA;
        Vec3 p1 = baseCenter + offsetB;

        Vec3 p2 = apex + offsetA;
        Vec3 p3 = apex + offsetB;

        AddVertsForQuad3D(verts, p1, p0, baseCenter, baseCenter, color, UVs);
        AddVertsForQuad3D(verts, p2, p3, apex, apex, color, UVs);

        AddVertsForQuad3D(verts, p0, p1, p3, p2, color, UVs);
    }
}

void AddVertsForCone3D(std::vector<Vertex_PCU>& verts, const Vec3& apex, const Vec3& baseCenter, float radius, const Rgba8& color, const AABB2& UVs, int numSlices)
{
    Vec3  axis       = baseCenter - apex;
    float coneLength = axis.GetLength();
    if (coneLength <= 0.f)
    {
        return;
    }

    Vec3 forward = axis.GetNormalized();

    Vec3 worldUp(0.f, 0.f, 1.f);
    if (fabsf(DotProduct3D(forward, worldUp)) > 0.99f)
    {
        worldUp = Vec3(1.f, 0.f, 0.f);
    }

    // right = forward × worldUp, up = right × forward
    Vec3 right = CrossProduct3D(forward, worldUp).GetNormalized();
    Vec3 up    = CrossProduct3D(right, forward).GetNormalized();

    float angleStep = 360.f / static_cast<float>(numSlices);

    for (int i = 0; i < numSlices; ++i)
    {
        float angleA = angleStep * (float)i;
        float angleB = angleStep * (float)(i + 1);

        float cosA = CosDegrees(angleA);
        float sinA = SinDegrees(angleA);
        float cosB = CosDegrees(angleB);
        float sinB = SinDegrees(angleB);

        Vec3 offsetA = (right * cosA + up * sinA) * radius;
        Vec3 offsetB = (right * cosB + up * sinB) * radius;

        Vec3 p0 = baseCenter + offsetA;
        Vec3 p1 = baseCenter + offsetB;

        AddVertsForQuad3D(verts, p1, p0, baseCenter, baseCenter, color, UVs);

        AddVertsForQuad3D(verts, p0, p1, apex, apex, color, UVs);
    }
}

void AddVertsForArrow3D(std::vector<Vertex_PCU>& verts, const Vec3& start, const Vec3& end, float radius, float arrowPercentage, const Rgba8& color, int numSlices)
{
    Vec3  axis        = end - start;
    float arrowLength = axis.GetLength() * arrowPercentage;
    Vec3  arrowEnd    = start + axis.GetNormalized() * arrowLength;

    /// Cylinder but do not have cap
    float cylinderLength = axis.GetLength();
    if (cylinderLength <= 0.f)
    {
        return;
    }
    Vec3 forward = axis.GetNormalized();

    Vec3 worldUp(0.f, 0.f, 1.f);
    if (fabsf(DotProduct3D(forward, worldUp)) > 0.99f)
    {
        worldUp = Vec3(1.f, 0.f, 0.f);
    }

    // right = forward × worldUp, up = right × forward
    Vec3 right = CrossProduct3D(forward, worldUp).GetNormalized();
    Vec3 up    = CrossProduct3D(right, forward).GetNormalized();

    float angleStep = 360.f / static_cast<float>(numSlices);

    for (int i = 0; i < numSlices; ++i)
    {
        float angleA = angleStep * (float)i;
        float angleB = angleStep * (float)(i + 1);

        float cosA = CosDegrees(angleA);
        float sinA = SinDegrees(angleA);
        float cosB = CosDegrees(angleB);
        float sinB = SinDegrees(angleB);

        Vec3 offsetA = (right * cosA + up * sinA) * radius;
        Vec3 offsetB = (right * cosB + up * sinB) * radius;

        Vec3 p0 = end + offsetA;
        Vec3 p1 = end + offsetB;

        Vec3 p2 = arrowEnd + offsetA;
        Vec3 p3 = arrowEnd + offsetB;

        AddVertsForQuad3D(verts, p1, p0, end, end, color, AABB2::ZERO_TO_ONE);

        AddVertsForQuad3D(verts, p0, p1, p3, p2, color, AABB2::ZERO_TO_ONE);
    }

    /// 

    AddVertsForCone3D(verts, start, arrowEnd, radius * (1 + arrowPercentage), color, AABB2::ZERO_TO_ONE, numSlices);
}

void AddVertsForArrow3DFixArrowSize(std::vector<Vertex_PCU>& verts, const Vec3& start, const Vec3& end, float radius, float arrowSize, const Rgba8& color, int numSlices)
{
    Vec3  axis        = end - start;
    float arrowLength = arrowSize;
    Vec3  arrowEnd    = start + axis.GetNormalized() * arrowLength;

    /// Cylinder but do not have cap
    float cylinderLength = axis.GetLength();
    if (cylinderLength <= 0.f)
    {
        return;
    }
    Vec3 forward = axis.GetNormalized();

    Vec3 worldUp(0.f, 0.f, 1.f);
    if (fabsf(DotProduct3D(forward, worldUp)) > 0.99f)
    {
        worldUp = Vec3(1.f, 0.f, 0.f);
    }

    // right = forward × worldUp, up = right × forward
    Vec3 right = CrossProduct3D(forward, worldUp).GetNormalized();
    Vec3 up    = CrossProduct3D(right, forward).GetNormalized();

    float angleStep = 360.f / static_cast<float>(numSlices);

    for (int i = 0; i < numSlices; ++i)
    {
        float angleA = angleStep * (float)i;
        float angleB = angleStep * (float)(i + 1);

        // 在 right-up 平面上使用极坐标生成两点，用于形成一段扇形
        float cosA = CosDegrees(angleA);
        float sinA = SinDegrees(angleA);
        float cosB = CosDegrees(angleB);
        float sinB = SinDegrees(angleB);

        // 将这两个局部向量偏移到世界中
        Vec3 offsetA = (right * cosA + up * sinA) * radius;
        Vec3 offsetB = (right * cosB + up * sinB) * radius;

        // 计算底面周边的两个顶点 p0 / p1
        Vec3 p0 = end + offsetA;
        Vec3 p1 = end + offsetB;

        Vec3 p2 = arrowEnd + offsetA;
        Vec3 p3 = arrowEnd + offsetB;

        // 1) 添加底面三角形（使用一个 degenerate quad，即把第四点重复写成 baseCenter）
        AddVertsForQuad3D(verts, p1, p0, end, end, color, AABB2::ZERO_TO_ONE);

        // 2) 添加侧面三角形，同样可用 degenerate quad：p0 / p1 / apex
        AddVertsForQuad3D(verts, p0, p1, p3, p2, color, AABB2::ZERO_TO_ONE);
    }

    /// 

    AddVertsForCone3D(verts, start, arrowEnd, radius * (1 + arrowSize), color, AABB2::ZERO_TO_ONE, numSlices);
}

/**
 * @brief Generates vertices for a sphere in 3D space without using an index buffer.
 *
 * This function creates a sphere by subdividing it into a specified number of slices (longitudinal divisions)
 * and stacks (latitudinal divisions). It computes the position, normal, texture coordinates, and color for
 * each vertex in model space, and transforms them into world space. In this non-indexed version, every triangle
 * is constructed by explicitly listing its vertices—even if adjacent triangles share common vertices, they are duplicated.
 *
 * @param verts         The vector into which the generated Vertex_PCU vertices will be appended.
 * @param center        The center position of the sphere in world space.
 * @param radius        The radius of the sphere.
 * @param color         The color to be applied to all vertices (default is white).
 * @param UVs           The UV coordinate bounds for texture mapping (default is AABB2::ZERO_TO_ONE).
 * @param numSlices     The number of longitudinal slices (default is 32).
 * @param numStacks     The number of latitudinal stacks (default is 16).
 */
void AddVertsForSphere3D(std::vector<Vertex_PCU>& verts, const Vec3&  center, float radius,
                         const Rgba8&             color, const AABB2& UVs, int      numSlices, int numStacks)
{
    // TODO: Update the UVs parameter to allow user to customize sphere uv mapping range
    UNUSED(UVs)
    float unit_pitch = 180.f / static_cast<float>(numStacks);
    float unit_yaw   = 360.f / static_cast<float>(numSlices);


    // bottom cap
    Vec3 bottomPole = Vec3::MakeFromPolarDegrees(90.f, 0.f, radius) + center;

    float ringAngleBottom = 90.f - unit_pitch;

    for (int i = 0; i < numSlices; ++i)
    {
        float yawA = (float)i * unit_yaw;
        float yawB = (float)((i + 1) % numSlices) * unit_yaw;

        Vec3 ringPointA = Vec3::MakeFromPolarDegrees(ringAngleBottom, yawA, radius) + center;
        Vec3 ringPointB = Vec3::MakeFromPolarDegrees(ringAngleBottom, yawB, radius) + center;

        float uA = static_cast<float>(i) / (float)numSlices;
        float uB = static_cast<float>(i + 1) / (float)numSlices;
        AABB2 subUVs(Vec2(uA, 0.f), Vec2(uB, 1.0f / (float)numStacks));

        AddVertsForQuad3D(verts, bottomPole, bottomPole, ringPointB, ringPointA, color, subUVs);
    }

    for (int stack = 1; stack < numStacks - 1; ++stack)
    {
        float pitch0 = 90.f - (float)stack * unit_pitch;
        float pitch1 = 90.f - (float)(stack + 1) * unit_pitch;

        float v0 = static_cast<float>(stack) / (float)numStacks;
        float v1 = static_cast<float>(stack + 1) / (float)numStacks;

        for (int slice = 0; slice < numSlices; ++slice)
        {
            float yaw0 = (float)slice * unit_yaw;
            float yaw1 = (float)((slice + 1) % numSlices) * unit_yaw;

            Vec3 v0Pos = Vec3::MakeFromPolarDegrees(pitch0, yaw0, radius) + center;
            Vec3 v1Pos = Vec3::MakeFromPolarDegrees(pitch1, yaw0, radius) + center;
            Vec3 v2Pos = Vec3::MakeFromPolarDegrees(pitch1, yaw1, radius) + center;
            Vec3 v3Pos = Vec3::MakeFromPolarDegrees(pitch0, yaw1, radius) + center;

            float u0 = static_cast<float>(slice) / (float)numSlices;
            float u1 = static_cast<float>(slice + 1) / (float)numSlices;

            AABB2 subUVs(Vec2(u0, v0), Vec2(u1, v1));
            AddVertsForQuad3D(verts, v0Pos, v3Pos, v2Pos, v1Pos, color, subUVs);
        }
    }

    Vec3  topPole      = Vec3::MakeFromPolarDegrees(-90.f, 0.f, radius) + center;
    float ringAngleTop = -90.f + unit_pitch;

    for (int i = 0; i < numSlices; ++i)
    {
        float yawA = (float)i * unit_yaw;
        float yawB = (float)((i + 1) % numSlices) * unit_yaw;

        Vec3 ringPointA = Vec3::MakeFromPolarDegrees(ringAngleTop, yawA, radius) + center;
        Vec3 ringPointB = Vec3::MakeFromPolarDegrees(ringAngleTop, yawB, radius) + center;

        float uA = static_cast<float>(i) / (float)numSlices;
        float uB = static_cast<float>(i + 1) / (float)numSlices;
        AABB2 subUVs(Vec2(uA, 1.0f - 1.0f / (float)numStacks), Vec2(uB, 1.0f));

        AddVertsForQuad3D(verts, ringPointA, ringPointB, topPole, topPole, color, subUVs);
    }
}

void AddVertsForWireFrameSphere3D(std::vector<Vertex_PCU>& verts, const Vec3& center, float radius, const Rgba8& color, const AABB2& UVs, int numSlices, int numStacks)
{
    // TODO: Update the UVs parameter to allow user to customize sphere uv mapping range
    UNUSED(UVs)
    float unit_pitch = 180.f / static_cast<float>(numStacks);
    float unit_yaw   = 360.f / static_cast<float>(numSlices);


    // bottom cap
    Vec3 bottomPole = Vec3::MakeFromPolarDegrees(90.f, 0.f, radius) + center;

    float ringAngleBottom = 90.f - unit_pitch;

    for (int i = 0; i < numSlices; ++i)
    {
        float yawA = (float)i * unit_yaw;
        float yawB = (float)((i + 1) % numSlices) * unit_yaw;

        Vec3 ringPointA = Vec3::MakeFromPolarDegrees(ringAngleBottom, yawA, radius) + center;
        Vec3 ringPointB = Vec3::MakeFromPolarDegrees(ringAngleBottom, yawB, radius) + center;

        float uA = static_cast<float>(i) / (float)numSlices;
        float uB = static_cast<float>(i + 1) / (float)numSlices;
        AABB2 subUVs(Vec2(uA, 0.f), Vec2(uB, 1.0f / (float)numStacks));

        AddVertsForCylinder3D(verts, bottomPole, ringPointB, 0.006f, color, subUVs, 3);
        AddVertsForCylinder3D(verts, bottomPole, ringPointA, 0.006f, color, subUVs, 3);
    }

    for (int stack = 1; stack < numStacks - 1; ++stack)
    {
        float pitch0 = 90.f - (float)stack * unit_pitch;
        float pitch1 = 90.f - (float)(stack + 1) * unit_pitch;

        float v0 = static_cast<float>(stack) / (float)numStacks;
        float v1 = static_cast<float>(stack + 1) / (float)numStacks;

        for (int slice = 0; slice < numSlices; ++slice)
        {
            float yaw0 = (float)slice * unit_yaw;
            float yaw1 = (float)((slice + 1) % numSlices) * unit_yaw;

            Vec3 v0Pos = Vec3::MakeFromPolarDegrees(pitch0, yaw0, radius) + center;
            Vec3 v1Pos = Vec3::MakeFromPolarDegrees(pitch1, yaw0, radius) + center;
            Vec3 v2Pos = Vec3::MakeFromPolarDegrees(pitch1, yaw1, radius) + center;
            Vec3 v3Pos = Vec3::MakeFromPolarDegrees(pitch0, yaw1, radius) + center;

            float u0 = static_cast<float>(slice) / (float)numSlices;
            float u1 = static_cast<float>(slice + 1) / (float)numSlices;

            AABB2 subUVs(Vec2(u0, v0), Vec2(u1, v1));

            AddVertsForCylinder3D(verts, v0Pos, v3Pos, 0.006f, color, subUVs, 3);
            AddVertsForCylinder3D(verts, v3Pos, v2Pos, 0.006f, color, subUVs, 3);
            AddVertsForCylinder3D(verts, v2Pos, v1Pos, 0.006f, color, subUVs, 3);
            AddVertsForCylinder3D(verts, v1Pos, v0Pos, 0.006f, color, subUVs, 3);
            //AddVertsForQuad3D(verts, v0Pos, v3Pos, v2Pos, v1Pos, color, subUVs);
        }
    }

    Vec3  topPole      = Vec3::MakeFromPolarDegrees(-90.f, 0.f, radius) + center;
    float ringAngleTop = -90.f + unit_pitch;

    for (int i = 0; i < numSlices; ++i)
    {
        float yawA = (float)i * unit_yaw;
        float yawB = (float)((i + 1) % numSlices) * unit_yaw;

        Vec3 ringPointA = Vec3::MakeFromPolarDegrees(ringAngleTop, yawA, radius) + center;
        Vec3 ringPointB = Vec3::MakeFromPolarDegrees(ringAngleTop, yawB, radius) + center;

        float uA = static_cast<float>(i) / (float)numSlices;
        float uB = static_cast<float>(i + 1) / (float)numSlices;
        AABB2 subUVs(Vec2(uA, 1.0f - 1.0f / (float)numStacks), Vec2(uB, 1.0f));

        AddVertsForCylinder3D(verts, topPole, ringPointA, 0.006f, color, subUVs, 3);
        AddVertsForCylinder3D(verts, topPole, ringPointB, 0.006f, color, subUVs, 3);
    }
}

/**
 * @brief Generates vertices for a sphere in 3D space using an indexed approach.
 *
 * This function calculates a unique set of vertices for a sphere subdivided into the specified number
 * of slices (longitudinal divisions) and stacks (latitudinal divisions). The unique vertices can then be
 * combined with an index buffer to form triangles, reducing redundant vertex data and improving GPU vertex
 * cache utilization. Texture coordinates and vertex colors are computed based on the provided parameters.
 *
 * @param verts         The vector into which the unique Vertex_PCU vertices will be appended.
 * @param center        The center position of the sphere in world space.
 * @param radius        The radius of the sphere.
 * @param color         The color to be applied to all vertices (default is white).
 * @param UVs           The UV coordinate bounds for texture mapping (default is AABB2::ZERO_TO_ONE).
 * @param numSlices     The number of longitudinal slices (default is 32).
 * @param numStacks     The number of latitudinal stacks (default is 16).
 */
void AddVertsForIndexedSphere3D(std::vector<Vertex_PCU>& verts, const Vec3& center, float radius, const Rgba8& color, const AABB2& UVs, int numSlices, int numStacks)
{
    UNUSED(verts)
    UNUSED(center)
    UNUSED(radius)
    UNUSED(color)
    UNUSED(UVs)
    UNUSED(numSlices)
    UNUSED(numStacks)
}

void AddVertsForCube3D(std::vector<Vertex_PCU>& verts, const AABB3& box, const Rgba8& color, const AABB2& UVs)
{
    std::vector<Vertex_PCU> localVerts;
    localVerts.reserve(36);

    // +X面 (右侧)
    AddVertsForQuad3D(localVerts,
                      Vec3(0.5f, -0.5f, -0.5f),
                      Vec3(0.5f, 0.5f, -0.5f),
                      Vec3(0.5f, 0.5f, 0.5f),
                      Vec3(0.5f, -0.5f, 0.5f),
                      color,
                      UVs);

    // -X面 (左侧)
    AddVertsForQuad3D(localVerts,
                      Vec3(-0.5f, 0.5f, -0.5f),
                      Vec3(-0.5f, -0.5f, -0.5f),
                      Vec3(-0.5f, -0.5f, 0.5f),
                      Vec3(-0.5f, 0.5f, 0.5f),
                      color,
                      UVs);

    // +Y面 (前方)
    AddVertsForQuad3D(localVerts,
                      Vec3(0.5f, 0.5f, -0.5f),
                      Vec3(-0.5f, 0.5f, -0.5f),
                      Vec3(-0.5f, 0.5f, 0.5f),
                      Vec3(0.5f, 0.5f, 0.5f),
                      color,
                      UVs);

    // -Y面 (后方)
    AddVertsForQuad3D(localVerts,
                      Vec3(-0.5f, -0.5f, -0.5f),
                      Vec3(0.5f, -0.5f, -0.5f),
                      Vec3(0.5f, -0.5f, 0.5f),
                      Vec3(-0.5f, -0.5f, 0.5f),
                      color,
                      UVs);

    // +Z面 (上方)
    AddVertsForQuad3D(localVerts, Vec3(0.5f, 0.5f, 0.5f),
                      Vec3(-0.5f, 0.5f, 0.5f),
                      Vec3(-0.5f, -0.5f, 0.5f),
                      Vec3(0.5f, -0.5f, 0.5f),
                      color,
                      UVs);

    // -Z面 (下方)
    AddVertsForQuad3D(localVerts,
                      Vec3(0.5f, -0.5f, -0.5f),
                      Vec3(-0.5f, -0.5f, -0.5f),
                      Vec3(-0.5f, 0.5f, -0.5f),
                      Vec3(0.5f, 0.5f, -0.5f),
                      color,
                      UVs);

    Vec3 boxCenter = (box.m_mins + box.m_maxs) * 0.5f;
    Vec3 boxExtent = (box.m_maxs - box.m_mins); // 长宽高

    for (Vertex_PCU& v : localVerts)
    {
        v.m_position.x *= boxExtent.x;
        v.m_position.y *= boxExtent.y;
        v.m_position.z *= boxExtent.z;

        v.m_position += boxCenter;
    }

    verts.insert(verts.end(), localVerts.begin(), localVerts.end());
}

void AddVertsForCube3D(std::vector<Vertex_PCU>& verts, std::vector<unsigned>& indexes, const AABB3& box, const Rgba8& color, const AABB2& UVs)
{
    UNUSED(verts)
    UNUSED(indexes)
    UNUSED(box)
    UNUSED(color)
    UNUSED(UVs)
}


void AddVertsForCube3DWireFrame(std::vector<Vertex_PCU>& verts, const AABB3& box, const Rgba8& color)
{
    float length, width, height;
    length = box.m_maxs.x - box.m_mins.x;
    width  = box.m_maxs.y - box.m_mins.y;
    height = box.m_maxs.z - box.m_mins.z;

    // facing
    Vec3 facingBottomLeft  = box.m_mins;
    Vec3 facingBottomRight = facingBottomLeft + Vec3(length, 0, 0);
    Vec3 facingTopLeft     = facingBottomLeft + Vec3(0, 0, height);
    Vec3 facingTopRight    = facingBottomRight + Vec3(0, 0, height);

    Vec3 backBottomLeft  = facingBottomLeft + Vec3(0, width, 0);
    Vec3 backBottomRight = facingBottomRight + Vec3(0, length, 0);
    Vec3 backTopLeft     = backBottomLeft + Vec3(0, 0, height);
    Vec3 backTopRight    = backBottomRight + Vec3(0, 0, height);

    AddVertsForCylinder3D(verts, facingBottomLeft, facingBottomRight, 0.006f, color, AABB2::ZERO_TO_ONE, 3);
    AddVertsForCylinder3D(verts, facingBottomRight, facingTopRight, 0.006f, color, AABB2::ZERO_TO_ONE, 3);
    AddVertsForCylinder3D(verts, facingTopRight, facingTopLeft, 0.006f, color, AABB2::ZERO_TO_ONE, 3);
    AddVertsForCylinder3D(verts, facingTopLeft, facingBottomLeft, 0.006f, color, AABB2::ZERO_TO_ONE, 3);

    AddVertsForCylinder3D(verts, backBottomLeft, backBottomRight, 0.006f, color, AABB2::ZERO_TO_ONE, 3);
    AddVertsForCylinder3D(verts, backBottomRight, backTopRight, 0.006f, color, AABB2::ZERO_TO_ONE, 3);
    AddVertsForCylinder3D(verts, backTopRight, backTopLeft, 0.006f, color, AABB2::ZERO_TO_ONE, 3);
    AddVertsForCylinder3D(verts, backTopLeft, backBottomLeft, 0.006f, color, AABB2::ZERO_TO_ONE, 3);

    AddVertsForCylinder3D(verts, facingBottomRight, backBottomRight, 0.006f, color, AABB2::ZERO_TO_ONE, 3);
    AddVertsForCylinder3D(verts, facingTopRight, backTopRight, 0.006f, color, AABB2::ZERO_TO_ONE, 3);
    AddVertsForCylinder3D(verts, facingBottomLeft, backBottomLeft, 0.006f, color, AABB2::ZERO_TO_ONE, 3);
    AddVertsForCylinder3D(verts, facingTopLeft, backTopLeft, 0.006f, color, AABB2::ZERO_TO_ONE, 3);
}

void AddVertsForCylinderZ3DWireFrame(std::vector<Vertex_PCU>& verts, const ZCylinder& cylinder, const Rgba8& color, int numSlices)
{
    Vec3  axis           = Vec3(0, 0, 1);
    float cylinderLength = cylinder.m_height;
    float radius         = cylinder.m_radius;
    Vec3  baseCenter     = cylinder.m_center - Vec3(0, 0, cylinderLength / 2.0f);
    Vec3  apex           = cylinder.m_center + Vec3(0, 0, cylinderLength / 2.0f);
    if (cylinderLength <= 0.f)
    {
        return;
    }
    Vec3 forward = axis.GetNormalized();

    Vec3 worldUp(0.f, 0.f, 1.f);
    if (fabsf(DotProduct3D(forward, worldUp)) > 0.99f)
    {
        worldUp = Vec3(1.f, 0.f, 0.f);
    }

    Vec3 right = CrossProduct3D(forward, worldUp).GetNormalized();
    Vec3 up    = CrossProduct3D(right, forward).GetNormalized();

    float angleStep = 360.f / static_cast<float>(numSlices);

    for (int i = 0; i < numSlices; ++i)
    {
        float angleA = angleStep * (float)i;
        float angleB = angleStep * (float)(i + 1);

        float cosA = CosDegrees(angleA);
        float sinA = SinDegrees(angleA);
        float cosB = CosDegrees(angleB);
        float sinB = SinDegrees(angleB);

        Vec3 offsetA = (right * cosA + up * sinA) * radius;
        Vec3 offsetB = (right * cosB + up * sinB) * radius;

        Vec3 p0 = baseCenter + offsetA;
        Vec3 p1 = baseCenter + offsetB;

        Vec3 p2 = apex + offsetA;
        Vec3 p3 = apex + offsetB;

        AddVertsForCylinder3D(verts, p1, p0, 0.006f, color, AABB2::ZERO_TO_ONE, 3);
        AddVertsForCylinder3D(verts, p2, p3, 0.006f, color, AABB2::ZERO_TO_ONE, 3);

        AddVertsForCylinder3D(verts, p0, p2, 0.006f, color, AABB2::ZERO_TO_ONE, 3);
    }
}

void AddVertsForCylinderZ3D(std::vector<Vertex_PCU>& verts, const ZCylinder cylinder, const Rgba8& color, const AABB2& UVs, int numSlices)
{
    if (numSlices < 3 || cylinder.m_radius <= 0.f || cylinder.m_height <= 0.f)
    {
        return;
    }

    float halfHeight   = cylinder.m_height * 0.5f;
    Vec3  bottomCenter = cylinder.m_center;
    bottomCenter.z -= halfHeight; // 圆柱底面
    Vec3 topCenter = cylinder.m_center;
    topCenter.z += halfHeight; // 圆柱顶面

    float uRange          = UVs.m_maxs.x - UVs.m_mins.x;
    float degreesPerSlice = 360.f / (float)numSlices;

    for (int i = 0; i < numSlices; i++)
    {
        float angleA = degreesPerSlice * (float)i;
        float angleB = degreesPerSlice * (float)(i + 1);

        float cosA = CosDegrees(angleA);
        float sinA = SinDegrees(angleA);
        float cosB = CosDegrees(angleB);
        float sinB = SinDegrees(angleB);

        Vec3 p0 = bottomCenter + Vec3(cosA * cylinder.m_radius, sinA * cylinder.m_radius, 0.f);
        Vec3 p1 = bottomCenter + Vec3(cosB * cylinder.m_radius, sinB * cylinder.m_radius, 0.f);

        Vec3 p3 = topCenter + Vec3(cosA * cylinder.m_radius, sinA * cylinder.m_radius, 0.f);
        Vec3 p2 = topCenter + Vec3(cosB * cylinder.m_radius, sinB * cylinder.m_radius, 0.f);

        float uA = angleA / 360.f;
        float uB = angleB / 360.f;

        float p0u = UVs.m_mins.x + uA * uRange;
        float p0v = UVs.m_mins.y;
        float p1u = UVs.m_mins.x + uB * uRange;
        float p1v = UVs.m_mins.y;

        float p2u = UVs.m_mins.x + uB * uRange;
        float p2v = UVs.m_maxs.y;
        float p3u = UVs.m_mins.x + uA * uRange;
        float p3v = UVs.m_maxs.y;

        verts.push_back(Vertex_PCU(p0, color, Vec2(p0u, p0v)));
        verts.push_back(Vertex_PCU(p1, color, Vec2(p1u, p1v)));
        verts.push_back(Vertex_PCU(p2, color, Vec2(p2u, p2v)));

        verts.push_back(Vertex_PCU(p0, color, Vec2(p0u, p0v)));
        verts.push_back(Vertex_PCU(p2, color, Vec2(p2u, p2v)));
        verts.push_back(Vertex_PCU(p3, color, Vec2(p3u, p3v)));
    }

    Vec3 cT = topCenter;
    Vec2 uvCenter(0.5f * (UVs.m_mins.x + UVs.m_maxs.x),
                  0.5f * (UVs.m_mins.y + UVs.m_maxs.y));
    float uvSizeX  = UVs.m_maxs.x - UVs.m_mins.x;
    float uvSizeY  = UVs.m_maxs.y - UVs.m_mins.y;
    float uvRadius = 0.5f * ((uvSizeX < uvSizeY) ? uvSizeX : uvSizeY);

    for (int i = 0; i < numSlices; i++)
    {
        float angleA = degreesPerSlice * (float)i;
        float angleB = degreesPerSlice * (float)(i + 1);

        Vec3 t0 = cT + Vec3(CosDegrees(angleA) * cylinder.m_radius,
                            SinDegrees(angleA) * cylinder.m_radius, 0.f);
        Vec3 t1 = cT + Vec3(CosDegrees(angleB) * cylinder.m_radius,
                            SinDegrees(angleB) * cylinder.m_radius, 0.f);

        Vec2 uvC(uvCenter);

        Vec2 uvT0 = CalcRadialUVForCircle(t0, cT, cylinder.m_radius, uvCenter, uvRadius, 0.f, false);
        Vec2 uvT1 = CalcRadialUVForCircle(t1, cT, cylinder.m_radius, uvCenter, uvRadius, 0.f, false);

        verts.push_back(Vertex_PCU(cT, color, uvC));
        verts.push_back(Vertex_PCU(t0, color, uvT0));
        verts.push_back(Vertex_PCU(t1, color, uvT1));
    }

    Vec3 cB = bottomCenter;
    for (int i = 0; i < numSlices; i++)
    {
        float angleA = degreesPerSlice * (float)i;
        float angleB = degreesPerSlice * (float)(i + 1);

        Vec3 b0 = cB + Vec3(CosDegrees(angleA) * cylinder.m_radius,
                            SinDegrees(angleA) * cylinder.m_radius, 0.f);
        Vec3 b1 = cB + Vec3(CosDegrees(angleB) * cylinder.m_radius,
                            SinDegrees(angleB) * cylinder.m_radius, 0.f);

        Vec2 uvC(uvCenter);
        Vec2 uvB0 = CalcRadialUVForCircle(b0, cB, cylinder.m_radius, uvCenter, uvRadius, 180.f, true);
        Vec2 uvB1 = CalcRadialUVForCircle(b1, cB, cylinder.m_radius, uvCenter, uvRadius, 180.f, true);

        verts.push_back(Vertex_PCU(cB, color, uvC));
        verts.push_back(Vertex_PCU(b1, color, uvB1));
        verts.push_back(Vertex_PCU(b0, color, uvB0));
    }
}

static Vec2 CalcRadialUVForCircle(const Vec3& pos, const Vec3& center, float radius, const Vec2& uvCenter, float uvRadius, float rotateDegrees, bool flipAboutY) // 新增
{
    float dx           = pos.x - center.x;
    float dy           = pos.y - center.y;
    float r            = sqrtf(dx * dx + dy * dy) / radius;
    float thetaDegrees = Atan2Degrees(dy, dx);

    if (flipAboutY)
    {
        thetaDegrees = 180.f - thetaDegrees;
    }

    thetaDegrees += rotateDegrees;

    float u = uvCenter.x + uvRadius * r * CosDegrees(thetaDegrees);
    float v = uvCenter.y + uvRadius * r * SinDegrees(thetaDegrees);

    return Vec2(u, v);
}
