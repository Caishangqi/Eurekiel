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
    Sphere      BuildVertices(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned>& outIndices, int sides = 32, const Rgba8& color = Rgba8::WHITE, const AABB2& uv = AABB2::ZERO_TO_ONE);
    static void BuildVertices(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned>& outIndices, Sphere& sphere, int sides = 32, const Rgba8& color = Rgba8::WHITE,
                              const AABB2&                uv                                                                     = AABB2::ZERO_TO_ONE);


    Sphere();
    explicit Sphere(const Vec3& position, float radius);

    bool IsOverlapping(const Plane3& other) const;

    static bool IsOverlapping(const Sphere& sphere, const Plane3& other);
};
