#pragma once
#include "Rgba8.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"

/**
 * Represents a single point (vertex) of a simple 3D object intended to be rendered
 */
struct Vertex_PCU
{
public:
 Vec3 m_position;
 Rgba8 m_color;
 Vec2 m_uvTextCoords;
public:
 Vertex_PCU();
 explicit Vertex_PCU( Vec3 const& position, Rgba8 const& color, Vec2 const& uvTextCoords);
};
