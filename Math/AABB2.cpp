#include "AABB2.hpp"

#include "MathUtils.hpp"

AABB2::~AABB2()
{
}

AABB2::AABB2()
{
    m_mins = Vec2(0.0f, 0.0f);
    m_maxs = Vec2(0.0f, 0.0f);
}

AABB2::AABB2(AABB2 const& copyFrom)
{
    m_mins = copyFrom.m_mins;
    m_maxs = copyFrom.m_maxs;
}

AABB2::AABB2(float minX, float minY, float maxX, float maxY)
{
    m_mins = Vec2(minX, minY);
    m_maxs = Vec2(maxX, maxY);
}

AABB2::AABB2(Vec2 const& mins, Vec2 const& maxs)
{
    m_mins = mins;
    m_maxs = maxs;
}

bool AABB2::IsPointInside(Vec2 const& point) const
{
    if (point.x < m_mins.x || point.x > m_maxs.x || point.y < m_mins.y || point.y > m_maxs.y)
        return false;
    else
    {
        return true;
    }
}

Vec2 const AABB2::GetCenter() const
{
    return m_mins + ((m_maxs - m_mins) / 2);
}

Vec2 const AABB2::GetDimensions() const
{
    return m_maxs - m_mins;
}

Vec2 const AABB2::GetNearestPoint(Vec2 const& referencePosition) const
{
    float x = GetClamped(referencePosition.x, m_mins.x, m_maxs.x);
    float y = GetClamped(referencePosition.y, m_mins.y, m_maxs.y);
    return Vec2(x, y);
}

Vec2 const AABB2::GetPointAtUV(Vec2 const& uv) const
{
    float x = Interpolate(m_mins.x, m_maxs.x, uv.x);
    float y = Interpolate(m_mins.y, m_maxs.y, uv.y);
    return Vec2(x, y);
}


Vec2 const AABB2::GetUVForPoint(Vec2 const& point) const
{
    float x = GetFractionWithinRange(point.x, m_mins.x, m_maxs.x);
    float y = GetFractionWithinRange(point.y, m_mins.y, m_maxs.y);
    return Vec2(x, y);
}

void AABB2::Translate(Vec2 const& translationToApply)
{
    m_mins.x += translationToApply.x;
    m_maxs.x += translationToApply.x;

    m_mins.y += translationToApply.y;
    m_maxs.y += translationToApply.y;
}

void AABB2::SetCenter(Vec2 const& newCenter)
{
    Vec2 displacement = newCenter - GetCenter();
    m_mins.x += displacement.x;
    m_maxs.x += displacement.x;

    m_mins.y += displacement.y;
    m_maxs.y += displacement.y;
}

void AABB2::SetDimensions(Vec2 const& newDimensions)
{
    Vec2 center = GetCenter();
    m_mins.x = center.x - newDimensions.x / 2.f;
    m_mins.y = center.y - newDimensions.y / 2.f;

    m_maxs.x = center.x + newDimensions.x / 2.f;
    m_maxs.y = center.y + newDimensions.y / 2.f;
}

void AABB2::StretchToIncludePoint(Vec2 const& point)
{
    if (!IsPointInside(point))
    {
        if (point.x > m_maxs.x)
        {
            m_maxs.x = point.x;
        }
        if (point.y > m_maxs.y)
        {
            m_maxs.y = point.y;
        }
        if (point.x < m_mins.x)
        {
            m_mins.x = point.x;
        }
        if (point.y < m_mins.y)
        {
            m_mins.y = point.y;
        }
    }
}
