#pragma once
#include "Vec3.hpp"

class Plane3;

struct Sphere
{
    Vec3  m_position = Vec3(0, 0, 0);
    float m_radius   = 0;

    Sphere();
    explicit Sphere(const Vec3& position, float radius);

    bool IsOverlapping(const Plane3& other) const;

    static bool IsOverlapping(const Sphere& sphere, const Plane3& other);
};
