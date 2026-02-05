#pragma once
#include <vector>
#include "Vec2.hpp"

struct Rgba8;
struct Vertex_PCU;

// [NEW] 2D convex polygon - vertex storage and rendering data generation
class ConvexPoly2
{
public:
    std::vector<Vec2> m_vertexPositionsCCW;  // Counter-clockwise vertex list

    ConvexPoly2() = default;
    explicit ConvexPoly2(const std::vector<Vec2>& verticesCCW);
    ~ConvexPoly2() = default;

    // Vertex building interface (follows AABB3 pattern)
    void BuildVertices(std::vector<Vertex_PCU>& outVerts, std::vector<unsigned>& outIndices, const Rgba8& color);
    static void BuildVertices(std::vector<Vertex_PCU>& outVerts, std::vector<unsigned>& outIndices,
                              const ConvexPoly2& poly, const Rgba8& color);

    std::vector<Vertex_PCU> GetVertices(const Rgba8& color);
    static std::vector<Vertex_PCU> GetVertices(const ConvexPoly2& poly, const Rgba8& color);

    std::vector<unsigned> GetIndices() const;
    static std::vector<unsigned> GetIndices(const ConvexPoly2& poly);

    // Geometry queries
    Vec2 GetCenter() const;
    int  GetVertexCount() const;
    bool IsValid() const;  // At least 3 vertices
};
