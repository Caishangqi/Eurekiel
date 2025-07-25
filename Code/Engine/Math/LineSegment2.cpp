#include "LineSegment2.hpp"

#include "Engine/Core/EngineCommon.hpp"

LineSegment2::LineSegment2()
{
}

LineSegment2::LineSegment2(const Vec2& start, const Vec2& end): m_start(start), m_end(end)
{
}

LineSegment2::LineSegment2(const Vec2& start, const Vec2& end, float thickness): m_start(start), m_end(end),
                                                                                 m_thickness(thickness)
{
}

void LineSegment2::Translate(const Vec2& translation)
{
    m_start += translation;
    m_end += translation;
}

void LineSegment2::SetCenter(const Vec2& newCenter)
{
    m_start = newCenter;
    m_end   = newCenter;
}

void LineSegment2::RotateAboutCenter(const Vec2& rotationDeltaDegrees)
{
    UNUSED(rotationDeltaDegrees)
}
