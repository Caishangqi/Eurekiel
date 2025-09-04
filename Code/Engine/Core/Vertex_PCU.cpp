#include "Vertex_PCU.hpp"


Vertex_PCU::Vertex_PCU(const Vertex_PCU& copyFrom)
{
    this->m_color        = copyFrom.m_color;
    this->m_position     = copyFrom.m_position;
    this->m_uvTextCoords = copyFrom.m_uvTextCoords;
}

Vertex_PCU::Vertex_PCU()
{
    this->m_color        = Rgba8();
    this->m_position     = Vec3();
    this->m_uvTextCoords = Vec2();
}

Vertex_PCU::Vertex_PCU(const Vec3& position, const Rgba8& color, const Vec2& uvTextCoords) : m_position(position),
                                                                                             m_color(color), m_uvTextCoords(uvTextCoords)
{
}

Vertex_PCU::~Vertex_PCU()
{
}

Vertex_PCUTBN::Vertex_PCUTBN() : m_position(Vec3::ZERO), m_color(Rgba8::WHITE), m_uvTexCoords(Vec2(0, 1)), m_tangent(Vec3::ZERO), m_bitangent(Vec3::ZERO), m_normal(Vec3::ZERO)
{
}

Vertex_PCUTBN::Vertex_PCUTBN(const Vec3& position, const Rgba8& color, const Vec2& uvTexCoords, const Vec3 normal, const Vec3 tangent, const Vec3 bitangent) : m_position(position), m_color(color),
    m_uvTexCoords(uvTexCoords), m_tangent(tangent), m_bitangent(bitangent), m_normal(normal)
{
}

Vertex_PCUTBN::~Vertex_PCUTBN()
{
}
