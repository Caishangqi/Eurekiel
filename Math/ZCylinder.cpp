#include "ZCylinder.hpp"
#include "AABB2.hpp"
#include "MathUtils.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/Vertex_PCU.hpp"

ZCylinder::ZCylinder()
{
}

ZCylinder::~ZCylinder()
{
}

ZCylinder ZCylinder::BuildVertices(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned int>& outIndices, int sides, const Rgba8& color, const AABB2& uv)
{
    const float angleStep  = 360.f / static_cast<float>(sides);
    const float halfHeight = m_height * 0.5f;

    Vec3 topCenter    = m_center + Vec3(0, 0, halfHeight);
    Vec3 bottomCenter = m_center - Vec3(0, 0, halfHeight);

    // Side Faces (Quads) with smooth shading
    for (int i = 0; i < sides; ++i)
    {
        float currAngle = static_cast<float>(i) * angleStep;
        float nextAngle = static_cast<float>(i + 1) * angleStep;

        Vec3 offsetCurr = Vec3(CosDegrees(currAngle), SinDegrees(currAngle), 0.f) * m_radius;
        Vec3 offsetNext = Vec3(CosDegrees(nextAngle), SinDegrees(nextAngle), 0.f) * m_radius;

        Vec3 p0 = bottomCenter + offsetCurr;
        Vec3 p1 = bottomCenter + offsetNext;
        Vec3 p2 = topCenter + offsetNext;
        Vec3 p3 = topCenter + offsetCurr;

        Vec3 n0 = offsetCurr.GetNormalized();
        Vec3 n1 = offsetNext.GetNormalized();
        Vec3 t0 = Vec3(-n0.y, n0.x, 0.f).GetNormalized(); // tangent → U
        Vec3 t1 = Vec3(-n1.y, n1.x, 0.f).GetNormalized();
        auto b  = Vec3(0, 0, 1); // bitangent → +Z (V)

        auto uv0 = Vec2(static_cast<float>(i) / static_cast<float>(sides), uv.m_mins.y);
        auto uv1 = Vec2(static_cast<float>(i + 1) / static_cast<float>(sides), uv.m_mins.y);
        auto uv2 = Vec2(static_cast<float>(i + 1) / static_cast<float>(sides), uv.m_maxs.y);
        auto uv3 = Vec2(static_cast<float>(i) / static_cast<float>(sides), uv.m_maxs.y);

        unsigned int base = static_cast<unsigned int>(outVerts.size());

        outVerts.emplace_back(p0, color, uv0, n0, t0, b); // bottom-left
        outVerts.emplace_back(p1, color, uv1, n1, t1, b); // bottom-right
        outVerts.emplace_back(p2, color, uv2, n1, t1, b); // top-right
        outVerts.emplace_back(p3, color, uv3, n0, t0, b); // top-left

        outIndices.push_back(base + 0);
        outIndices.push_back(base + 1);
        outIndices.push_back(base + 2);
        outIndices.push_back(base + 0);
        outIndices.push_back(base + 2);
        outIndices.push_back(base + 3);
    }
    Vec3 capTangent(1, 0, 0);
    Vec3 capBitangent(0, 1, 0);

    // Top and Bottom Caps
    unsigned int topCenterIndex = static_cast<unsigned int>(outVerts.size());
    outVerts.emplace_back(topCenter, color,
                          (uv.m_mins + uv.m_maxs) * 0.5f,
                          Vec3(0, 0, 1), capTangent, capBitangent);

    unsigned int bottomCenterIndex = static_cast<unsigned int>(outVerts.size());
    outVerts.emplace_back(bottomCenter, color,
                          (uv.m_mins + uv.m_maxs) * 0.5f,
                          Vec3(0, 0, -1), capTangent, -capBitangent);

    for (int i = 0; i < sides; ++i)
    {
        float currAngle = static_cast<float>(i) * angleStep;
        float nextAngle = static_cast<float>(i + 1) * angleStep;

        float x0 = CosDegrees(currAngle) * m_radius;
        float y0 = SinDegrees(currAngle) * m_radius;
        float x1 = CosDegrees(nextAngle) * m_radius;
        float y1 = SinDegrees(nextAngle) * m_radius;

        Vec3 topRim0    = topCenter + Vec3(x0, y0, 0);
        Vec3 topRim1    = topCenter + Vec3(x1, y1, 0);
        Vec3 bottomRim0 = bottomCenter + Vec3(x0, y0, 0);
        Vec3 bottomRim1 = bottomCenter + Vec3(x1, y1, 0);

        auto uvTop0 = Vec2(RangeMap(x0, -m_radius, m_radius, uv.m_mins.x, uv.m_maxs.x),
                           RangeMap(y0, -m_radius, m_radius, uv.m_mins.y, uv.m_maxs.y));
        auto uvTop1 = Vec2(RangeMap(x1, -m_radius, m_radius, uv.m_mins.x, uv.m_maxs.x),
                           RangeMap(y1, -m_radius, m_radius, uv.m_mins.y, uv.m_maxs.y));

        // Top cap (counter-clockwise)
        unsigned int top0 = static_cast<unsigned int>(outVerts.size());
        outVerts.emplace_back(topRim0, color, uvTop0, Vec3(0, 0, 1));
        unsigned int top1 = static_cast<unsigned int>(outVerts.size());
        outVerts.emplace_back(topRim1, color, uvTop1, Vec3(0, 0, 1));

        outIndices.push_back(topCenterIndex);
        outIndices.push_back(top0);
        outIndices.push_back(top1);

        // Bottom cap (clockwise)
        unsigned int bottom0 = static_cast<unsigned int>(outVerts.size());
        outVerts.emplace_back(bottomRim0, color, uvTop0, Vec3(0, 0, -1));
        unsigned int bottom1 = static_cast<unsigned int>(outVerts.size());
        outVerts.emplace_back(bottomRim1, color, uvTop1, Vec3(0, 0, -1));

        outIndices.push_back(bottomCenterIndex);
        outIndices.push_back(bottom1);
        outIndices.push_back(bottom0);
    }

    return *this;
}

void ZCylinder::BuildVertices(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned int>& outIndices, ZCylinder& zCylinder, int sides, const Rgba8& color, const AABB2& uv)
{
    zCylinder.BuildVertices(outVerts, outIndices, sides, color, uv);
}


ZCylinder::ZCylinder(const ZCylinder& copyFrom)
{
    m_center = copyFrom.m_center;
    m_height = copyFrom.m_height;
    m_radius = copyFrom.m_radius;
}

ZCylinder::ZCylinder(const Vec3& center, float radius, float height): m_center(center), m_height(height), m_radius(radius)
{
}

ZCylinder::ZCylinder(const Vec3& centerOrBase, float radius, float height, bool isBasePosition)
    : m_height(height), m_radius(radius)
{
    if (isBasePosition)
    {
        m_center = centerOrBase + Vec3(0.f, 0.f, height / 2.0f);
    }
    else
    {
        m_center = centerOrBase;
    }
}
