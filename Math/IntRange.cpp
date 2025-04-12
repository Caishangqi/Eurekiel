#include "IntRange.hpp"


IntRange IntRange::ONE         = IntRange(1, 1);
IntRange IntRange::ZERO        = IntRange(0, 0);
IntRange IntRange::ZERO_TO_ONE = IntRange(0, 1);

IntRange::IntRange()
{
}

IntRange::IntRange(int min, int max): m_min(min), m_max(max)
{
}

bool IntRange::operator==(const IntRange& compare) const
{
    return m_min == compare.m_min && m_max == compare.m_max;
}

bool IntRange::operator!=(const IntRange& compare) const
{
    return m_min != compare.m_min || m_max != compare.m_max;
}

void IntRange::operator=(const IntRange& copyFrom)
{
    m_min = copyFrom.m_min;
    m_max = copyFrom.m_max;
}

bool IntRange::IsOnRange(int value) const
{
    return m_min <= value && value <= m_max;
}

bool IntRange::IsOverlappingWith(const IntRange& other) const
{
    return m_min >= other.m_min && m_max <= other.m_max;
}
