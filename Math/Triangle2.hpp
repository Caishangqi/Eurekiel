#pragma once
#include "Vec2.hpp"

struct Triangle2
{
    Vec2 m_positionCounterClockwise[3];

    Triangle2();
    explicit Triangle2(const Vec2& p1, const Vec2& p2, const Vec2& p3);
    explicit Triangle2(const Vec2 points[3]);

    void Translate(const Vec2& translation);
};
