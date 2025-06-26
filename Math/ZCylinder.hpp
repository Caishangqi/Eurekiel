#pragma once
#include <vector>

#include "AABB2.hpp"
#include "Vec3.hpp"
#include "Engine/Core/Rgba8.hpp"

class AABB2;
struct Rgba8;
struct Vertex_PCUTBN;

class ZCylinder
{
public:
    ZCylinder();
    ~ZCylinder();

    // Verts
    ZCylinder   BuildVertices(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned>& outIndices, int sides = 32, const Rgba8& color = Rgba8::WHITE, const AABB2& uv = AABB2::ZERO_TO_ONE);
    static void BuildVertices(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned>& outIndices, ZCylinder& zCylinder, int sides = 32, const Rgba8& color = Rgba8::WHITE,
                              const AABB2&                uv                                                                           = AABB2::ZERO_TO_ONE);

    ZCylinder(const ZCylinder& copyFrom);
    explicit ZCylinder(const Vec3& center, float radius, float height);
    ZCylinder(const Vec3& centerOrBase, float radius, float height, bool isBasePosition);

    Vec3  m_center = Vec3::ZERO;
    float m_height = 0.f;
    float m_radius = 0.f;
};
