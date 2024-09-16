#include "Vertex_PCU.hpp"


Vertex_PCU::Vertex_PCU(const Vertex_PCU& copyFrom)
{
    this->m_color = copyFrom.m_color;
    this->m_position = copyFrom.m_position;
    this->m_uvTextCoords = copyFrom.m_uvTextCoords;
}

Vertex_PCU::Vertex_PCU()
{
    this->m_color = Rgba8();
    this->m_position = Vec3();
    this->m_uvTextCoords = Vec2();
}

Vertex_PCU::Vertex_PCU(const Vec3& position, const Rgba8& color, const Vec2& uvTextCoords): m_position(position),
    m_color(color), m_uvTextCoords(uvTextCoords)
{
}

Vertex_PCU::~Vertex_PCU()
{
}
