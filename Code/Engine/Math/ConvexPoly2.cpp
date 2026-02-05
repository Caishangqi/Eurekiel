#include "ConvexPoly2.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/Rgba8.hpp"

ConvexPoly2::ConvexPoly2(const std::vector<Vec2>& verticesCCW)
    : m_vertexPositionsCCW(verticesCCW)
{
}

void ConvexPoly2::BuildVertices(std::vector<Vertex_PCU>& outVerts, std::vector<unsigned>& outIndices, const Rgba8& color)
{
    BuildVertices(outVerts, outIndices, *this, color);
}

void ConvexPoly2::BuildVertices(std::vector<Vertex_PCU>& outVerts, std::vector<unsigned>& outIndices,
                                 const ConvexPoly2& poly, const Rgba8& color)
{
    if (!poly.IsValid())
        return;

    unsigned baseIndex = static_cast<unsigned>(outVerts.size());

    // Add vertices
    for (const Vec2& pos : poly.m_vertexPositionsCCW)
    {
        outVerts.emplace_back(Vec3(pos.x, pos.y, 0.0f), color, Vec2::ZERO);
    }

    // Triangle fan indices: 0,1,2 | 0,2,3 | 0,3,4 | ...
    int vertCount = poly.GetVertexCount();
    for (int i = 1; i < vertCount - 1; ++i)
    {
        outIndices.push_back(baseIndex);
        outIndices.push_back(baseIndex + i);
        outIndices.push_back(baseIndex + i + 1);
    }
}

std::vector<Vertex_PCU> ConvexPoly2::GetVertices(const Rgba8& color)
{
    return GetVertices(*this, color);
}

std::vector<Vertex_PCU> ConvexPoly2::GetVertices(const ConvexPoly2& poly, const Rgba8& color)
{
    std::vector<Vertex_PCU> verts;
    verts.reserve(poly.m_vertexPositionsCCW.size());

    for (const Vec2& pos : poly.m_vertexPositionsCCW)
    {
        verts.emplace_back(Vec3(pos.x, pos.y, 0.0f), color, Vec2::ZERO);
    }
    return verts;
}

std::vector<unsigned> ConvexPoly2::GetIndices() const
{
    return GetIndices(*this);
}

std::vector<unsigned> ConvexPoly2::GetIndices(const ConvexPoly2& poly)
{
    std::vector<unsigned> indices;
    int vertCount = poly.GetVertexCount();
    if (vertCount < 3)
        return indices;

    indices.reserve((vertCount - 2) * 3);
    for (int i = 1; i < vertCount - 1; ++i)
    {
        indices.push_back(0);
        indices.push_back(static_cast<unsigned>(i));
        indices.push_back(static_cast<unsigned>(i + 1));
    }
    return indices;
}

Vec2 ConvexPoly2::GetCenter() const
{
    if (m_vertexPositionsCCW.empty())
        return Vec2::ZERO;

    Vec2 sum = Vec2::ZERO;
    for (const Vec2& pos : m_vertexPositionsCCW)
    {
        sum += pos;
    }
    return sum / static_cast<float>(m_vertexPositionsCCW.size());
}

int ConvexPoly2::GetVertexCount() const
{
    return static_cast<int>(m_vertexPositionsCCW.size());
}

bool ConvexPoly2::IsValid() const
{
    return m_vertexPositionsCCW.size() >= 3;
}
