#pragma once

struct Vec4
{
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    float w = 0.f;

    Vec4() = default;
    explicit Vec4(float initialX, float initialY, float initialZ, float initialW);

    // Operator (const)
    bool       operator==(const Vec4& compare) const; // vec4 == vec4
    bool       operator!=(const Vec4& compare) const; // vec4 != vec4
    const Vec4 operator+(const Vec4& vecToAdd) const; // vec4 + vec4
    const Vec4 operator-(const Vec4& vecToSubtract) const; // vec4 - vec4
    const Vec4 operator*(float uniformScale) const; // vec4 * float
    const Vec4 operator/(float inverseScale) const; // vec4 / float

    // Operators (self-mutating / non-const)
    void  operator+=(const Vec4& vecToAdd); // vec4 += vec4
    void  operator-=(const Vec4& vecToSubtract); // vec4 -= vec4
    void  operator*=(float uniformScale); // vec4 *= vec4
    void  operator/=(float uniformDivisor); // vec4 /= float
    Vec4& operator=(const Vec4& copyFrom); // vec4 = vec4

    // Standalone "friend" functions that are conceptually, but not actually, part of Vec3::
    friend const Vec4 operator*(float uniformScale, const Vec4& vecToScale); // float * vec4

    // String Operations
    void SetFromText(const char* text);
};
