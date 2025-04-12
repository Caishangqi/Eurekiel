#pragma once
#include "Vec2.hpp"

struct Capsule2
{
    Vec2  m_start  = Vec2(0, 0);
    Vec2  m_end    = Vec2(0, 0);
    float m_radius = 0.0f;

    Capsule2();
    explicit Capsule2(const Vec2& start, const Vec2& end, float radius);

    void Translate(const Vec2& translation);
    void SetCenter(const Vec2& newCenter);
    void RotateAboutCenter(const Vec2& rotationDeltaDegrees);
};
