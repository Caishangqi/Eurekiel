#pragma once
#include "Vec2.hpp"

struct Disc2
{
    Vec2  m_position = Vec2(0, 0);
    float m_radius   = 0;

    Disc2();
    explicit Disc2(const Vec2& position, float radius);
};
