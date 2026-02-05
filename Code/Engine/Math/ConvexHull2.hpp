#pragma once
#include <vector>
#include "Plane2.hpp"
#include "RaycastUtils.hpp"

class ConvexPoly2;

// [NEW] 2D convex hull - boundary planes storage and collision detection
class ConvexHull2
{
public:
    std::vector<Plane2> m_planes;  // Boundary planes (outward-facing normals)

    ConvexHull2() = default;
    explicit ConvexHull2(const std::vector<Plane2>& planes);
    explicit ConvexHull2(const ConvexPoly2& poly);  // Build planes from vertices
    ~ConvexHull2() = default;

    // Collision detection
    bool IsPointInside(const Vec2& point) const;
    RaycastResult2D Raycast(const Vec2& start, const Vec2& direction, float maxDist) const;

    // Static versions
    static bool IsPointInside(const Vec2& point, const ConvexHull2& hull);
    static RaycastResult2D Raycast(const Vec2& start, const Vec2& direction,
                                   float maxDist, const ConvexHull2& hull);

    // Accessors
    int GetPlaneCount() const;
    const Plane2& GetPlane(int index) const;
    const std::vector<Plane2>& GetPlanes() const { return m_planes; }
    bool IsValid() const;
};
