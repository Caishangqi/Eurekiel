#pragma once
#include <vector>

#include "RaycastUtils.hpp"
#include "Vec3.hpp"

class Plane3;
struct Sphere;

class OBB3
{
public:
    explicit OBB3(Vec3& center, Vec3 halfDimensions, Vec3& iBases, Vec3& jBases, Vec3& kBases);
    OBB3() = default;
    ~OBB3();

    OBB3(const OBB3& copyFrom);

    // Verts
    OBB3        BuildVertices(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned>& outIndices, const Rgba8& color = Rgba8::WHITE, const AABB2& uv = AABB2::ZERO_TO_ONE);
    static void BuildVertices(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned>& outIndices, OBB3& obb3, const Rgba8& color, const AABB2& uv);


    bool              IsPointInside(const Vec3& point) const;
    Vec3              GetNearestPoint(const Vec3& point) const;
    std::vector<Vec3> GetCorners() const;
    bool              IsOverlapping(const OBB3& other) const;
    bool              IsOverlapping(const Sphere& other) const;
    bool              IsOverlapping(const Plane3& other) const;
    RaycastResult3D   Raycast(Vec3 startPos, Vec3 fwdNormal, float maxDist, const OBB3& obb3) const;

    Vec3 GetLocalPosForWorldPos(const Vec3& worldPosition) const;
    Vec3 GetWorldPosForLocalPos(const Vec3& localPosition) const;

    OBB3& SetOrientation(EulerAngles angles);

    static bool            IsPointInsideOBB3(const Vec3& point, const OBB3& obb3);
    static Vec3            GetNearestPointOnOBB3(const Vec3& referencePoint, const OBB3& obb3);
    static RaycastResult3D RaycastVsOBB3D(Vec3 startPos, Vec3 fwdNormal, float maxDist, const OBB3& obb3);

    //[[deprecated("DoOBB3sOverlap is not implemented")]]
    static bool DoOBB3sOverlap(const OBB3& a, const OBB3& b); // Not implemented
    static bool DoOBB3sAndSphereOverlap(const OBB3& obb3, const Sphere& sphere);
    static bool DoOBB3sAndPlane3Overlap(const OBB3& obb3, const Plane3& plane);

    static Vec3 GetLocalPosForWorldPos(const OBB3& obb3, const Vec3& worldPosition);
    static Vec3 GetWorldPosForLocalPos(const OBB3& obb3, const Vec3& localPosition);

    Vec3 m_center         = Vec3::ZERO;
    Vec3 m_halfDimensions = Vec3::ZERO;
    Vec3 m_iBasisNormal   = Vec3(1.0f, 0.0f, 0.0f);
    Vec3 m_jBasisNormal   = Vec3(0.0f, 1.0f, 0.0f);
    Vec3 m_kBasisNormal   = Vec3(0.0f, 0.0f, 1.0f);
};
