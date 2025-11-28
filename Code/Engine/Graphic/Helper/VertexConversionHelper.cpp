#include "VertexConversionHelper.hpp"
#include "Engine/Math/MathUtils.hpp"

//--------------------------------------------------
// PCU -> PCUTBN conversion
//--------------------------------------------------
Vertex_PCUTBN VertexConversionHelper::ToPCUTBN(const Vertex_PCU& source,
                                                const Vec3& normal,
                                                const Vec3& tangent,
                                                const Vec3& bitangent)
{
    return Vertex_PCUTBN(source, normal, tangent, bitangent);
}

void VertexConversionHelper::ToPCUTBN(const std::vector<Vertex_PCU>& source,
                                      std::vector<Vertex_PCUTBN>& dest,
                                      const Vec3& defaultNormal,
                                      const Vec3& defaultTangent,
                                      const Vec3& defaultBitangent)
{
    dest.clear();
    dest.reserve(source.size());
    
    for (const auto& vertex : source)
    {
        dest.emplace_back(vertex, defaultNormal, defaultTangent, defaultBitangent);
    }
}

std::vector<Vertex_PCUTBN> VertexConversionHelper::ToPCUTBNVector(
    const std::vector<Vertex_PCU>& source,
    const Vec3& defaultNormal,
    const Vec3& defaultTangent,
    const Vec3& defaultBitangent)
{
    std::vector<Vertex_PCUTBN> result;
    ToPCUTBN(source, result, defaultNormal, defaultTangent, defaultBitangent);
    return result;
}

//--------------------------------------------------
// PCUTBN -> PCU conversion
//--------------------------------------------------
Vertex_PCU VertexConversionHelper::ToPCU(const Vertex_PCUTBN& source)
{
    return Vertex_PCU(source);
}

void VertexConversionHelper::ToPCU(const std::vector<Vertex_PCUTBN>& source,
                                   std::vector<Vertex_PCU>& dest)
{
    dest.clear();
    dest.reserve(source.size());
    
    for (const auto& vertex : source)
    {
        dest.emplace_back(vertex);
    }
}

std::vector<Vertex_PCU> VertexConversionHelper::ToPCUVector(
    const std::vector<Vertex_PCUTBN>& source)
{
    std::vector<Vertex_PCU> result;
    ToPCU(source, result);
    return result;
}

//--------------------------------------------------
// Convenience utility function
//--------------------------------------------------
Vec3 VertexConversionHelper::CalculateTriangleNormal(const Vec3& v0, const Vec3& v1, const Vec3& v2)
{
   // Calculate the two sides of the triangle
    Vec3 edge1 = v1 - v0;
    Vec3 edge2 = v2 - v0;
    
    // Cross product to get the normal
    Vec3 normal = CrossProduct3D(edge1, edge2);
    
    // normalize
    return normal.GetNormalized();
}

void VertexConversionHelper::ToPCUTBNWithCalculatedNormals(
    const std::vector<Vertex_PCU>& source,
    std::vector<Vertex_PCUTBN>& dest)
{
    dest.clear();
    
    // Make sure the number of vertices is a multiple of 3 (triangle)
    size_t triangleCount = source.size() / 3;
    if (source.size() % 3 != 0)
    {
        // [WARNING] The number of vertices is not a multiple of 3, the remaining vertices will use zero normals
    }
    
    dest.reserve(source.size());
    
    // Process each triangle
    for (size_t i = 0; i < triangleCount; ++i)
    {
        size_t idx0 = i * 3;
        size_t idx1 = i * 3 + 1;
        size_t idx2 = i * 3 + 2;
        
        // Calculate triangle normals
        Vec3 normal = CalculateTriangleNormal(
            source[idx0].m_position,
            source[idx1].m_position,
            source[idx2].m_position
        );
        
        //Add the same normal to the three vertices of the triangle
        dest.emplace_back(source[idx0], normal, Vec3(), Vec3());
        dest.emplace_back(source[idx1], normal, Vec3(), Vec3());
        dest.emplace_back(source[idx2], normal, Vec3(), Vec3());
    }
    
    // Process remaining vertices (if any)
    for (size_t i = triangleCount * 3; i < source.size(); ++i)
    {
        dest.emplace_back(source[i], Vec3(), Vec3(), Vec3());
    }
}
