#pragma once
#include "Rgba8.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"

struct Vertex_PCUTBN;
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
    
    // [NEW] Constructor converted from Vertex_PCUTBN (discards TBN data)
    explicit Vertex_PCU(const Vertex_PCUTBN& source);
    
    ~Vertex_PCU();
};

struct Vertex_PCUTBN
{
    Vec3  m_position;
    Rgba8 m_color;
    Vec2  m_uvTexCoords;
    Vec3  m_tangent;
    Vec3  m_bitangent;
    Vec3  m_normal;

    Vertex_PCUTBN();
    explicit Vertex_PCUTBN(const Vec3& position, const Rgba8& color, const Vec2& uvTexCoords, Vec3 normal = Vec3(), Vec3 tangent = Vec3(), Vec3 bitangent = Vec3());
    
    // [NEW] Constructor converted from Vertex_PCU
    explicit Vertex_PCUTBN(const Vertex_PCU& source,    const Vec3& normal = Vec3(), const Vec3& tangent = Vec3(), const Vec3& bitangent = Vec3());
    
    ~Vertex_PCUTBN();
};
