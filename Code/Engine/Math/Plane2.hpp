#pragma once
#include "Vec2.hpp"

// [NEW] 2D plane representation for half-plane tests and convex hull boundaries
class Plane2
{
public:
    Vec2  m_normal   = Vec2::ZERO;  // Unit normal pointing outward
    float m_distance = 0.0f;        // Signed distance from origin along normal

    Plane2() = default;
    explicit Plane2(const Vec2& normal, float distance);
    explicit Plane2(const Vec2& pointOnPlane, const Vec2& normal);
    ~Plane2() = default;
    Plane2(const Plane2& copyFrom);

    // Core methods
    float GetSignedDistance(const Vec2& point) const;
    bool  IsPointInFrontOfPlane(const Vec2& point) const;  // Point on positive side (outside)
    bool  IsPointBehind(const Vec2& point) const;          // Point on negative side (inside)
    Vec2  GetNearestPoint(const Vec2& point) const;
    Vec2  GetCenter() const;

    // Accessors
    const Vec2& GetNormal() const { return m_normal; }
    float GetDistance() const { return m_distance; }

    // Static versions
    static float GetSignedDistance(const Vec2& point, const Plane2& plane);
    static bool  IsPointInFrontOfPlane(const Vec2& point, const Plane2& plane);
    static bool  IsPointBehind(const Vec2& point, const Plane2& plane);
    static Vec2  GetNearestPoint(const Vec2& point, const Plane2& plane);
    static Vec2  GetCenter(const Plane2& plane);

    // Factory: create plane from edge (CCW vertex order -> outward normal)
    static Plane2 FromEdge(const Vec2& startPos, const Vec2& endPos);
};
