#pragma once
#include "Vec2.hpp"


struct LineSegment2
{
    Vec2  m_start     = Vec2(0, 0);
    Vec2  m_end       = Vec2(0, 0);
    float m_thickness = 1.0f;

    LineSegment2();
    explicit LineSegment2(const Vec2& start, const Vec2& end);
    explicit LineSegment2(const Vec2& start, const Vec2& end, float thickness);

    void Translate(const Vec2& translation);
    void SetCenter(const Vec2& newCenter);
    void RotateAboutCenter(const Vec2& rotationDeltaDegrees);
};
