#pragma once
#include "Rgba8.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"

/**
 * Represents a single point (vertex) of a simple 3D object intended to be rendered
 */
struct Vertex_PCU
{
    Vec3  m_position; // P
    Rgba8 m_color; // C
    Vec2  m_uvTextCoords; // U
    //Vertex_PCU();
    Vertex_PCU(const Vertex_PCU& copyFrom);
    Vertex_PCU();
    explicit Vertex_PCU(const Vec3& position, const Rgba8& color, const Vec2& uvTextCoords);
    ~Vertex_PCU();
};

struct Vertex_PCUTBN
{
public:
    Vec3 m_position;
    Rgba8 m_color;
    Vec2 m_uvTexCoords;
    Vec3 m_tangent;
    Vec3 m_bitangent;
    Vec3 m_normal;

    Vertex_PCUTBN();
    explicit Vertex_PCUTBN( Vec3 const& position, Rgba8 const& color, Vec2 const& uvTexCoords, Vec3 const normal = Vec3(), Vec3 const tangent = Vec3(), Vec3 const bitangent = Vec3() );
    ~Vertex_PCUTBN();
};