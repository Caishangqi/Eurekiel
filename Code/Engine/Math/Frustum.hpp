#pragma once

#include <array>
#include <cstddef>

#include "AABB3.hpp"
#include "Plane3.hpp"
#include "Vec2.hpp"
#include "Vec3.hpp"

class Frustum
{
public:
    static constexpr size_t PLANE_COUNT = 6;

    enum PlaneIndex : size_t
    {
        NearPlane = 0,
        FarPlane,
        LeftPlane,
        RightPlane,
        TopPlane,
        BottomPlane
    };

    Frustum() = default;
    explicit Frustum(const std::array<Plane3, PLANE_COUNT>& planes);

    static Frustum CreatePerspective(
        const Vec3& position,
        const Vec3& forward,
        const Vec3& left,
        const Vec3& up,
        float       verticalFovDegrees,
        float       aspectRatio,
        float       nearPlane,
        float       farPlane);

    static Frustum CreateOrthographic(
        const Vec3& position,
        const Vec3& forward,
        const Vec3& left,
        const Vec3& up,
        const Vec2& mins,
        const Vec2& maxs,
        float       nearPlane,
        float       farPlane);

    bool IsPointInside(const Vec3& point) const;
    bool IsOverlapping(const AABB3& bounds) const;

    const Plane3& GetPlane(PlaneIndex index) const;

private:
    std::array<Plane3, PLANE_COUNT> m_planes;
};
