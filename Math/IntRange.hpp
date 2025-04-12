#pragma once

struct IntRange
{
    int m_min = 0;
    int m_max = 0;

    IntRange();

    explicit IntRange(int min, int max);

    // operator
    bool operator==(const IntRange& compare) const;
    bool operator!=(const IntRange& compare) const;
    void operator=(const IntRange& copyFrom);
    // Methods
    bool IsOnRange(int value) const;
    bool IsOverlappingWith(const IntRange& other) const;
    // Const
    static IntRange ZERO;
    static IntRange ONE;
    static IntRange ZERO_TO_ONE;
};
