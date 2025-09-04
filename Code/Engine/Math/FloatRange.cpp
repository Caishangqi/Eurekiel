#include "FloatRange.hpp"

FloatRange FloatRange::ONE         = FloatRange(1, 1);
FloatRange FloatRange::ZERO        = FloatRange(0, 0);
FloatRange FloatRange::ZERO_TO_ONE = FloatRange(0, 1);


FloatRange::FloatRange(float min, float max) : m_min(min), m_max(max)
{
}

FloatRange::FloatRange()
{
}

bool FloatRange::operator==(const FloatRange& compare) const
{
    return m_max == compare.m_max && m_min == compare.m_min;
}

bool FloatRange::operator!=(const FloatRange& compare) const
{
    return m_max != compare.m_max || m_min != compare.m_min;
}

void FloatRange::operator=(const FloatRange& copyFrom)
{
    m_min = copyFrom.m_min;
    m_max = copyFrom.m_max;
}

bool FloatRange::IsOnRange(float value) const
{
    return m_min <= value && value <= m_max;
}

bool FloatRange::IsOverlappingWith(const FloatRange& other) const
{
    if (m_max < other.m_min || m_min > other.m_max)
    {
        return false;
    }
    return true;
}

void FloatRange::StretchToIncludeValue(float value)
{
    if (value < m_min)
    {
        m_min = value;
    }

    if (value > m_max)
    {
        m_max = value;
    }
}

void FloatRange::SetFromText(const char* text)
{
    if (text == nullptr)
    {
        return;
    }

    // Split the input text on the comma delimiter
    Strings parts = SplitStringOnDelimiter(text, '~');

    if (parts.size() != 2)
    {
        return;
    }

    // Remove whitespace from each part
    for (std::string& part : parts)
    {
        for (size_t i = 0; i < part.length(); ++i)
        {
            if (isspace(part[i]))
            {
                part.erase(i, 1);
                --i;
            }
        }
    }
    m_min = static_cast<float>(atof(parts[0].c_str()));
    m_max = static_cast<float>(atof(parts[1].c_str()));
}
