#pragma once
#include "Engine/Core/XmlUtils.hpp"

struct FloatRange
{
    float m_min = 0.f;
    float m_max = 0.f;

    FloatRange();

    explicit FloatRange(float min, float max);

    // operator
    bool operator==(const FloatRange& compare) const;
    bool operator!=(const FloatRange& compare) const;
    void operator=(const FloatRange& copyFrom);
    // Methods
    bool IsOnRange(float value) const;
    bool IsOverlappingWith(const FloatRange& other) const;
    void StretchToIncludeValue(float value);
    void SetFromText(const char* text);
    // Const
    static FloatRange ZERO;
    static FloatRange ONE;
    static FloatRange ZERO_TO_ONE;
};
