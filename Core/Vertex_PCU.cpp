#include "Vertex_PCU.hpp"


Vertex_PCU::Vertex_PCU(Vec3 const& position, Rgba8 const& color, Vec2 const& uvTextCoords): m_position(position),
                                                                                            m_color(color), m_uvTextCoords(uvTextCoords)
{
}

Vertex_PCU::~Vertex_PCU()
{
}
