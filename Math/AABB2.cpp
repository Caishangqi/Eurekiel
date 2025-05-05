#include "AABB2.hpp"

#include "MathUtils.hpp"

AABB2 AABB2::ZERO_TO_ONE = AABB2(Vec2(0, 0), Vec2(1, 1));

AABB2::~AABB2()
{
}

AABB2::AABB2()
{
    m_mins = Vec2(0.0f, 0.0f);
    m_maxs = Vec2(0.0f, 0.0f);
}

AABB2::AABB2(const AABB2& copyFrom)
{
    m_mins = copyFrom.m_mins;
    m_maxs = copyFrom.m_maxs;
}

AABB2::AABB2(float minX, float minY, float maxX, float maxY)
{
    m_mins = Vec2(minX, minY);
    m_maxs = Vec2(maxX, maxY);
}

AABB2::AABB2(const Vec2& mins, const Vec2& maxs)
{
    m_mins = mins;
    m_maxs = maxs;
}

bool AABB2::IsPointInside(const Vec2& point) const
{
    if (point.x < m_mins.x || point.x > m_maxs.x || point.y < m_mins.y || point.y > m_maxs.y)
        return false;
    return true;
}

const Vec2 AABB2::GetCenter() const
{
    return m_mins + ((m_maxs - m_mins) / 2);
}

const Vec2 AABB2::GetDimensions() const
{
    return m_maxs - m_mins;
}

const Vec2 AABB2::GetNearestPoint(const Vec2& referencePosition) const
{
    float x = GetClamped(referencePosition.x, m_mins.x, m_maxs.x);
    float y = GetClamped(referencePosition.y, m_mins.y, m_maxs.y);
    return Vec2(x, y);
}

const Vec2 AABB2::GetPointAtUV(const Vec2& uv) const
{
    float x = Interpolate(m_mins.x, m_maxs.x, uv.x);
    float y = Interpolate(m_mins.y, m_maxs.y, uv.y);
    return Vec2(x, y);
}


const Vec2 AABB2::GetUVForPoint(const Vec2& point) const
{
    float x = GetFractionWithinRange(point.x, m_mins.x, m_maxs.x);
    float y = GetFractionWithinRange(point.y, m_mins.y, m_maxs.y);
    return Vec2(x, y);
}

AABB2 AABB2::StretchTowards(AABB2Anchor anchor, Vec2 stretchAmount)
{
    // Make a copy so we don't mutate the original AABB2
    AABB2 result = *this;

    switch (anchor)
    {
    case AABB2Anchor::BOTTOM_LEFT:
        // Keep bottom-left fixed, move right/up edges
        result.m_maxs.x += stretchAmount.x; // extend/shrink to the right
        result.m_maxs.y += stretchAmount.y; // extend/shrink upward
        break;

    case AABB2Anchor::BOTTOM_RIGHT:
        // Keep bottom-right fixed
        result.m_mins.x -= stretchAmount.x; // extend/shrink to the left
        result.m_maxs.y += stretchAmount.y; // extend/shrink upward
        break;

    case AABB2Anchor::TOP_LEFT:
        // Keep top-left fixed
        result.m_maxs.x += stretchAmount.x; // extend/shrink to the right
        result.m_mins.y -= stretchAmount.y; // extend/shrink downward
        break;

    case AABB2Anchor::TOP_RIGHT:
        // Keep top-right fixed
        result.m_mins.x -= stretchAmount.x; // extend/shrink to the left
        result.m_mins.y -= stretchAmount.y; // extend/shrink downward
        break;

    default: /* INVALID */
        break;
    }

    return result;
}

/**
 * @brief Return a new AABB2 with extra padding.
 *        padding = (padLeft, padBottom, padRight, padTop)
 *
 *  - Positive values enlarge the box.
 *  - Negative values shrink the box (clamped responsibility left to caller).
 */
AABB2 AABB2::GetPadded(const Vec4& padding) const
{
    // padding.x = left , padding.y = bottom
    // padding.z = right, padding.w = top
    Vec2 newMins = m_mins - Vec2(padding.x, padding.y); // move mins outward
    Vec2 newMaxs = m_maxs + Vec2(padding.z, padding.w); // move maxs outward
    return AABB2(newMins, newMaxs);
}

void AABB2::Translate(const Vec2& translationToApply)
{
    m_mins.x += translationToApply.x;
    m_maxs.x += translationToApply.x;

    m_mins.y += translationToApply.y;
    m_maxs.y += translationToApply.y;
}

void AABB2::SetCenter(const Vec2& newCenter)
{
    Vec2 displacement = newCenter - GetCenter();
    m_mins.x += displacement.x;
    m_maxs.x += displacement.x;

    m_mins.y += displacement.y;
    m_maxs.y += displacement.y;
}

void AABB2::SetDimensions(const Vec2& newDimensions)
{
    Vec2 center = GetCenter();
    m_mins.x    = center.x - newDimensions.x / 2.f;
    m_mins.y    = center.y - newDimensions.y / 2.f;

    m_maxs.x = center.x + newDimensions.x / 2.f;
    m_maxs.y = center.y + newDimensions.y / 2.f;
}

void AABB2::StretchToIncludePoint(const Vec2& point)
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

bool operator==(const AABB2& lhs, const AABB2& rhs)
{
    return lhs.m_mins == rhs.m_mins
        && lhs.m_maxs == rhs.m_maxs;
}

bool operator!=(const AABB2& lhs, const AABB2& rhs)
{
    return !(lhs == rhs);
}
