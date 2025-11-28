#pragma once
#include "Engine/Core/Vertex_PCU.hpp"
#include <vector>

/**
 * [HELPER] Vertex format conversion tool class
 * Responsibility: Provide bidirectional conversion function of Vertex_PCU and Vertex_PCUTBN
 * Features: Purely static, stateless, single responsibility
 */
class VertexConversionHelper {
public:
    // [REQUIRED] prohibit instantiation
    VertexConversionHelper() = delete;
    VertexConversionHelper(const VertexConversionHelper&) = delete;
    VertexConversionHelper& operator=(const VertexConversionHelper&) = delete;
    
    //--------------------------------------------------
    // PCU -> PCUTBN conversion
    //--------------------------------------------------
    
    //Single vertex transformation
    static Vertex_PCUTBN ToPCUTBN(const Vertex_PCU& source,
                                  const Vec3& normal = Vec3(),
                                  const Vec3& tangent = Vec3(),
                                  const Vec3& bitangent = Vec3());
    
    // Batch conversion (output parameter version)
    static void ToPCUTBN(const std::vector<Vertex_PCU>& source,
                        std::vector<Vertex_PCUTBN>& dest,
                        const Vec3& defaultNormal = Vec3(),
                        const Vec3& defaultTangent = Vec3(),
                        const Vec3& defaultBitangent = Vec3());
    
    // Batch conversion (return value version)
    static std::vector<Vertex_PCUTBN> ToPCUTBNVector(
        const std::vector<Vertex_PCU>& source,
        const Vec3& defaultNormal = Vec3(),
        const Vec3& defaultTangent = Vec3(),
        const Vec3& defaultBitangent = Vec3());
    
    //--------------------------------------------------
    // PCUTBN -> PCU conversion
    //--------------------------------------------------
    
    // Single vertex transformation (discard TBN data)
    static Vertex_PCU ToPCU(const Vertex_PCUTBN& source);
    
    // Batch conversion (output parameter version)
    static void ToPCU(const std::vector<Vertex_PCUTBN>& source,std::vector<Vertex_PCU>& dest);
    
    // Batch conversion (return value version)
    static std::vector<Vertex_PCU> ToPCUVector(const std::vector<Vertex_PCUTBN>& source);
    
    //--------------------------------------------------
    // Convenience utility function
    //--------------------------------------------------
    
    // Calculate triangle normals (used to automatically calculate normals during conversion)
    static Vec3 CalculateTriangleNormal(const Vec3& v0, const Vec3& v1, const Vec3& v2);
    
    // Batch conversion and automatic calculation of normals (every 3 vertices is a triangle)
    static void ToPCUTBNWithCalculatedNormals( const std::vector<Vertex_PCU>& source,std::vector<Vertex_PCUTBN>& dest);
};
