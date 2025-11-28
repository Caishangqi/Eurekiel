#pragma once
#include <vector>

#include "AABB2.hpp"
#include "Vec3.hpp"
#include "Engine/Core/Rgba8.hpp"

struct Vertex_PCUTBN;
class Plane3;

struct Sphere
{
    Vec3  m_position = Vec3(0, 0, 0);
    float m_radius   = 0;

    // Verts
    Sphere      BuildVertices(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned>& outIndices, int sides = 64, const Rgba8& color = Rgba8::WHITE, const AABB2& uv = AABB2::ZERO_TO_ONE);
    static void BuildVertices(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned>& outIndices, Sphere& sphere, int sides = 64, const Rgba8& color = Rgba8::WHITE,
                              const AABB2&                uv                                                                     = AABB2::ZERO_TO_ONE);
    std::vector<Vertex_PCUTBN>        GetVertices(const Rgba8& color = Rgba8::WHITE, const AABB2& uv = AABB2::ZERO_TO_ONE, int sides = 64);
    static std::vector<Vertex_PCUTBN> GetVertices(Sphere& sphere, const Rgba8& color, const AABB2& uv, int sides = 64);
    std::vector<unsigned>             GetIndices(int sides = 64);
    static std::vector<unsigned>      GetIndices(Sphere& sphere, int sides = 64);

    Sphere();
    explicit Sphere(const Vec3& position, float radius);

    bool IsOverlapping(const Plane3& other) const;

    static bool IsOverlapping(const Sphere& sphere, const Plane3& other);
};
