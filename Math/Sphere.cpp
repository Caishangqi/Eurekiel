#include "Sphere.hpp"

#include "MathUtils.hpp"
#include "Plane3.hpp"
#include "Engine/Core/Vertex_PCU.hpp"

Sphere Sphere::BuildVertices(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned int>& outIndices, int sides, const Rgba8& color, const AABB2& uv)
{
    // Grid Segments
    const int   numSlices = std::max(3, sides); // longitude
    const int   numStacks = std::max(2, sides / 2); // latitude (+ poles)
    const float dPitchDeg = 180.f / static_cast<float>(numStacks); // delta latitude
    const float dYawDeg   = 360.f / static_cast<float>(numSlices); // delta longitude
    const auto  worldUp   = Vec3(0.f, 0.f, 1.f);

    // vertex
    const unsigned baseV       = static_cast<unsigned>(outVerts.size()); // Base index
    const int      vertsPerRow = numSlices + 1; // One more column of closed seams

    for (int stack = 0; stack <= numStacks; ++stack) // Include both poles
    {
        float pitchDeg = -90.f + dPitchDeg * static_cast<float>(stack); // -90° to +90°
        float v        = RangeMap(static_cast<float>(stack), 0.f, static_cast<float>(numStacks),
                           uv.m_mins.y, uv.m_maxs.y); // [0,1]→V

        for (int slice = 0; slice <= numSlices; ++slice) //1 more column
        {
            float yawDeg = dYawDeg * static_cast<float>(slice);
            float u      = RangeMap(static_cast<float>(slice), 0.f, static_cast<float>(numSlices),
                               uv.m_mins.x, uv.m_maxs.x); // [0,1]→U

            Vec3 pos = Vec3::MakeFromPolarDegrees(pitchDeg, yawDeg, m_radius) + m_position;
            Vec3 n   = (pos - m_position).GetNormalized();

            // tangent (U direction)
            Vec3 t = CrossProduct3D(worldUp, n); // worldUp × n
            if (t.GetLengthSquared() < 1e-6f) // Pole degeneration
                t = Vec3(1.f, 0.f, 0.f);
            else
                t = t.GetNormalized();

            // bitangent (V direction)
            Vec3 b = CrossProduct3D(n, t); // n × t

            outVerts.emplace_back(pos, color, Vec2(u, v), n, t, b);
        }
    }

    // Indexing (two triangles form a quad)
    for (int stack = 0; stack < numStacks; ++stack)
    {
        for (int slice = 0; slice < numSlices; ++slice)
        {
            unsigned i0 = baseV + stack * vertsPerRow + slice;
            unsigned i1 = i0 + 1;
            unsigned i2 = i0 + vertsPerRow;
            unsigned i3 = i2 + 1;

            // @Note: Counterclockwise
            outIndices.push_back(i0);
            outIndices.push_back(i2);
            outIndices.push_back(i1);
            outIndices.push_back(i1);
            outIndices.push_back(i2);
            outIndices.push_back(i3);
        }
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
