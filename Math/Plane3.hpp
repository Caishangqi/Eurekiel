#pragma once
#include <vector>

#include "IntVec2.hpp"
#include "RaycastUtils.hpp"
#include "Vec3.hpp"
#include "Engine/Core/Rgba8.hpp"

#define m_distToPlaneAlongNormalFromOrigin m_distance
struct Vertex_PCU;

class Plane3
{
public:
    explicit Plane3(const Vec3& normal, float distToPlaneAlongNormalFromOrigin);
    Plane3() = default;
    ~Plane3();

    Plane3(const Plane3& copyFrom);

    Vec3            GetNearestPoint(const Vec3& point) const;
    Vec3            GetCenter() const;
    bool            IsPointInFrontOfPlane(const Vec3& point) const;
    bool            IsOverlapping(const OBB3& other) const;
    bool            IsOverlapping(const Sphere& other) const;
    bool            IsOverlapping(const AABB3& other) const;
    RaycastResult3D Raycast(Vec3 startPos, Vec3 fwdNormal, float maxDist) const;


    void AddVerts(std::vector<Vertex_PCU>& verts, const IntVec2& dimensions, float thickness, const Rgba8& colorX, const Rgba8& colorY);

    static Vec3            GetNearestPoint(const Vec3& point, const Plane3& plane3);
    static Vec3            GetCenter(const Plane3& plane3);
    static bool            IsPointInFrontOfPlane(const Vec3& point, const Plane3& plane3);
    static RaycastResult3D Raycast(Vec3 startPos, Vec3 fwdNormal, float maxDist, const Plane3& plane);


    static bool IsOverlapping(const Plane3& plane, const OBB3& other);
    static bool IsOverlapping(const Plane3& plane, const Sphere& other);
    static bool IsOverlapping(const Plane3& plane, const AABB3& other);

    static void AddVerts(std::vector<Vertex_PCU>& verts, const Plane3& plane3, const IntVec2& dimensions = IntVec2(50, 50), float thickness = 0.02f, const Rgba8& colorX = Rgba8(255, 0, 0, 100),
                         const Rgba8&             colorY                                                 = Rgba8(0, 255, 0, 100));

    Vec3 m_normal                            = Vec3::ZERO;
    float m_distToPlaneAlongNormalFromOrigin = 0.0f;
};
