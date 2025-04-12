#pragma once
#include "Vec3.hpp"

struct Sphere
{
    Vec3  m_position = Vec3(0, 0,0);
    float m_radius   = 0;

    Sphere();
    explicit Sphere(const Vec3& position, float radius);
};
