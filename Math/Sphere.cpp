#include "Sphere.hpp"

#include "MathUtils.hpp"
#include "Plane3.hpp"
#include "Engine/Core/Vertex_PCU.hpp"

Sphere Sphere::BuildVertices(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned int>& outIndices, int sides, const Rgba8& color, const AABB2& uv)
{
    int   numSlices  = sides;
    int   numStacks  = sides / 2;
    float unit_pitch = 180.f / static_cast<float>(numStacks);
    float unit_yaw   = 360.f / static_cast<float>(numSlices);

    // top polar
    Vec3         topPole      = Vec3::MakeFromPolarDegrees(-90.f, 0.f, m_radius) + m_position;
    Vec3         normalTop    = (topPole - m_position).GetNormalized();
    unsigned int topPoleIndex = (unsigned int)outVerts.size();
    outVerts.emplace_back(topPole, color, Vec2(uv.GetCenter().x, uv.m_mins.y), normalTop);

    for (int i = 0; i < numSlices; ++i)
    {
        float yawA = (float)i * unit_yaw;
        float yawB = (float)(i + 1) * unit_yaw;

        Vec3 ringA = Vec3::MakeFromPolarDegrees(-90.f + unit_pitch, yawA, m_radius) + m_position;
        Vec3 ringB = Vec3::MakeFromPolarDegrees(-90.f + unit_pitch, yawB, m_radius) + m_position;

        Vec3 normA = (ringA - m_position).GetNormalized();
        Vec3 normB = (ringB - m_position).GetNormalized();

        Vec2 uvA = Vec2(RangeMap((float)i, 0.f, (float)numSlices, uv.m_mins.x, uv.m_maxs.x),
                        RangeMap(1.f, 0.f, 1.f, uv.m_mins.y, uv.m_maxs.y));
        Vec2 uvB = Vec2(RangeMap((float)(i + 1), 0.f, (float)numSlices, uv.m_mins.x, uv.m_maxs.x),
                        RangeMap(1.f, 0.f, 1.f, uv.m_mins.y, uv.m_maxs.y));

        unsigned int i0 = (unsigned int)outVerts.size();
        outVerts.emplace_back(ringB, color, uvB, normB);
        outVerts.emplace_back(ringA, color, uvA, normA);

        outIndices.push_back(topPoleIndex);
        outIndices.push_back(i0 + 1);
        outIndices.push_back(i0 + 0);
    }

    // middle
    for (int stack = 1; stack < numStacks - 1; ++stack)
    {
        float pitch0 = -90.f + stack * unit_pitch;
        float pitch1 = -90.f + (stack + 1) * unit_pitch;
        for (int slice = 0; slice < numSlices; ++slice)
        {
            float yaw0 = slice * unit_yaw;
            float yaw1 = (slice + 1) * unit_yaw;

            Vec3 p0 = Vec3::MakeFromPolarDegrees(pitch0, yaw0, m_radius) + m_position;
            Vec3 p1 = Vec3::MakeFromPolarDegrees(pitch0, yaw1, m_radius) + m_position;
            Vec3 p2 = Vec3::MakeFromPolarDegrees(pitch1, yaw1, m_radius) + m_position;
            Vec3 p3 = Vec3::MakeFromPolarDegrees(pitch1, yaw0, m_radius) + m_position;

            Vec3 n0 = (p0 - m_position).GetNormalized();
            Vec3 n1 = (p1 - m_position).GetNormalized();
            Vec3 n2 = (p2 - m_position).GetNormalized();
            Vec3 n3 = (p3 - m_position).GetNormalized();

            Vec2 uv0 = Vec2(RangeMap((float)slice, 0.f, (float)numSlices, uv.m_mins.x, uv.m_maxs.x),
                            RangeMap((float)(stack), 0.f, (float)numStacks, uv.m_mins.y, uv.m_maxs.y));
            Vec2 uv1 = Vec2(RangeMap((float)(slice + 1), 0.f, (float)numSlices, uv.m_mins.x, uv.m_maxs.x),
                            uv0.y);
            Vec2 uv2 = Vec2(uv1.x,
                            RangeMap((float)(stack + 1), 0.f, (float)numStacks, uv.m_mins.y, uv.m_maxs.y));
            Vec2 uv3 = Vec2(uv0.x, uv2.y);

            unsigned int baseIndex = (unsigned int)outVerts.size();

            outVerts.emplace_back(p3, color, uv3, n3);
            outVerts.emplace_back(p2, color, uv2, n2);
            outVerts.emplace_back(p1, color, uv1, n1);
            outVerts.emplace_back(p0, color, uv0, n0);

            outIndices.push_back(baseIndex + 0);
            outIndices.push_back(baseIndex + 1);
            outIndices.push_back(baseIndex + 2);

            outIndices.push_back(baseIndex + 0);
            outIndices.push_back(baseIndex + 2);
            outIndices.push_back(baseIndex + 3);
        }
    }

    // bottom polar
    Vec3         bottomPole      = Vec3::MakeFromPolarDegrees(90.f, 0.f, m_radius) + m_position;
    unsigned int bottomPoleIndex = (unsigned int)outVerts.size();
    Vec3         normalBottom    = (bottomPole - m_position).GetNormalized();
    outVerts.emplace_back(bottomPole, color, Vec2(uv.GetCenter().x, uv.m_maxs.y), normalBottom);

    for (int i = 0; i < numSlices; ++i)
    {
        float yawA = i * unit_yaw;
        float yawB = (i + 1) * unit_yaw;

        Vec3 ringA = Vec3::MakeFromPolarDegrees(90.f - unit_pitch, yawA, m_radius) + m_position;
        Vec3 ringB = Vec3::MakeFromPolarDegrees(90.f - unit_pitch, yawB, m_radius) + m_position;

        Vec3 normA = (ringA - m_position).GetNormalized();
        Vec3 normB = (ringB - m_position).GetNormalized();

        Vec2 uvA = Vec2(RangeMap((float)i, 0.f, (float)numSlices, uv.m_mins.x, uv.m_maxs.x),
                        RangeMap((float)(numStacks - 2), 0.f, (float)numStacks, uv.m_mins.y, uv.m_maxs.y));
        Vec2 uvB = Vec2(RangeMap((float)(i + 1), 0.f, (float)numSlices, uv.m_mins.x, uv.m_maxs.x),
                        uvA.y);

        unsigned int i0 = (unsigned int)outVerts.size();
        outVerts.emplace_back(ringB, color, uvB, normB);
        outVerts.emplace_back(ringA, color, uvA, normA);

        outIndices.push_back(bottomPoleIndex);
        outIndices.push_back(i0 + 0);
        outIndices.push_back(i0 + 1);
    }

    return *this;
}

void Sphere::BuildVertices(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned int>& outIndices, Sphere& sphere, int sides, const Rgba8& color, const AABB2& uv)
{
    sphere.BuildVertices(outVerts, outIndices, sides, color, uv);
}

Sphere::Sphere()
{
}

Sphere::Sphere(const Vec3& position, float radius): m_position(position), m_radius(radius)
{
}

bool Sphere::IsOverlapping(const Plane3& other) const
{
    return IsOverlapping(*this, other);
}

bool Sphere::IsOverlapping(const Sphere& sphere, const Plane3& other)
{
    float signedDistance = DotProduct3D(other.m_normal, sphere.m_position) - other.m_distToPlaneAlongNormalFromOrigin;
    if (fabs(signedDistance) > sphere.m_radius)
        return false;
    return true;
}
