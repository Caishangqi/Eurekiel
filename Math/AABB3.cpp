#include "AABB3.hpp"

#include <algorithm>

AABB3::~AABB3()
{
}

bool AABB3::IsPointInside(const Vec3& point) const
{
    return (point.x >= m_mins.x && point.x <= m_maxs.x) &&
        (point.y >= m_mins.y && point.y <= m_maxs.y) &&
        (point.z >= m_mins.z && point.z <= m_maxs.z);
}

const Vec3 AABB3::GetCenter() const
{
    return m_mins + ((m_maxs - m_mins) / 2);
}

const Vec3 AABB3::GetDimensions() const
{
    return m_maxs - m_mins;
}

void AABB3::Translate(const Vec3& translationToApply)
{
    m_mins.x += translationToApply.x;
    m_maxs.x += translationToApply.x;

    m_mins.y += translationToApply.y;
    m_maxs.y += translationToApply.y;

    m_mins.z += translationToApply.z;
    m_maxs.z += translationToApply.z;
}

void AABB3::SetCenter(const Vec3& newCenter)
{
    Vec3 displacement = newCenter - GetCenter();
    m_mins.x += displacement.x;
    m_maxs.x += displacement.x;

    m_mins.y += displacement.y;
    m_maxs.y += displacement.y;

    m_mins.z += displacement.z;
    m_maxs.z += displacement.z;
}

void AABB3::SetDimensions(const Vec3& newDimensions)
{
    Vec3 center = GetCenter();
    m_mins.x    = center.x - newDimensions.x / 2.f;
    m_mins.y    = center.y - newDimensions.y / 2.f;
    m_mins.z    = center.z - newDimensions.z / 2.f;

    m_maxs.x = center.x + newDimensions.x / 2.f;
    m_maxs.y = center.y + newDimensions.y / 2.f;
    m_maxs.z = center.z + newDimensions.z / 2.f;
}

void AABB3::StretchToIncludePoint(const Vec3& point)
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
        if (point.z > m_maxs.z)
        {
            m_maxs.z = point.z;
        }
        if (point.x < m_mins.x)
        {
            m_mins.x = point.x;
        }
        if (point.y < m_mins.y)
        {
            m_mins.y = point.y;
        }
        if (point.z < m_mins.z)
        {
            m_mins.z = point.z;
        }
    }
}

AABB3::AABB3(const AABB3& copyFrom)
{
    m_mins = copyFrom.m_mins;
    m_maxs = copyFrom.m_maxs;
}

AABB3::AABB3(float minX, float minY, float minZ, float maxX, float maxY, float maxZ): m_mins(Vec3(minX, minY, minZ)), m_maxs(Vec3(maxX, maxY, maxZ))
{
}

AABB3::AABB3(const Vec3& mins, const Vec3& maxs): m_mins(mins), m_maxs(maxs)
{
}
