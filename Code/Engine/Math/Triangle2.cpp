#include "Triangle2.hpp"

#include "Engine/Core/EngineCommon.hpp"

Triangle2::Triangle2()
{
}

Triangle2::Triangle2(const Vec2& p1, const Vec2& p2, const Vec2& p3)
{
    m_positionCounterClockwise[0] = p1;
    m_positionCounterClockwise[1] = p2;
    m_positionCounterClockwise[2] = p3;
}

Triangle2::Triangle2(const Vec2 points[3])
{
    m_positionCounterClockwise[0] = points[0];
    m_positionCounterClockwise[1] = points[1];
    m_positionCounterClockwise[2] = points[2];
}

void Triangle2::Translate(const Vec2& translation)
{
    UNUSED(translation)
}
