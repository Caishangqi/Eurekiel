#pragma once
#include <vector>

#include "RaycastUtils.hpp"
#include "Vec3.hpp"

struct Rgba8;
struct Vertex_PCUTBN;
class Plane3;

class AABB3
{
public:
    Vec3 m_mins;
    Vec3 m_maxs;

    ~AABB3();

    AABB3()
    {
    }

    // Verts
    AABB3       BuildVertices(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned>& outIndices, const Rgba8& color = Rgba8::WHITE, const AABB2& uv = AABB2::ZERO_TO_ONE);
    static void BuildVertices(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned>& outIndices, AABB3& aabb3, const Rgba8& color, const AABB2& uv);

    // Accessors (const methods)
    bool              IsPointInside(const Vec3& point) const;
    std::vector<Vec3> GetCorners() const;
    const Vec3        GetCenter() const;
    const Vec3        GetDimensions() const;
    bool              IsOverlapping(const Plane3& other) const;
    RaycastResult3D   Raycast(Vec3 startPos, Vec3 fwdNormal, float maxDist) const;

    static bool            IsOverlapping(const AABB3& aabb3, const Plane3& other);
    static RaycastResult3D Raycast(Vec3 startPos, Vec3 fwdNormal, float maxDist, const AABB3& aabb3);

    // Mutators (non-const methods)
    void Translate(const Vec3& translationToApply);
    void SetCenter(const Vec3& newCenter);
    void SetDimensions(const Vec3& newDimensions);
    void StretchToIncludePoint(const Vec3& point); // does minimal stretching required (none if already on point)

    AABB3(const AABB3& copyFrom);
    explicit AABB3(float minX, float minY, float minZ, float maxX, float maxY, float maxZ);
    explicit AABB3(const Vec3& mins, const Vec3& maxs);
};
