#pragma once
struct EulerAngles;

struct Vec3
{
    static Vec3 ZERO;
    static Vec3 ONE;
    static Vec3 INVALID;
    
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;

    Vec3() = default;
    Vec3(const Vec3& copyFrom); // copy constructor (from another Vec3)
    explicit Vec3(float initialX, float initialY, float initialZ);
    explicit Vec3(const EulerAngles& angles);
    explicit Vec3(float length);

    // Accessors (const methods)
    float      GetLength() const;
    float      GetLengthXY() const;
    float      GetLengthSquared() const;
    float      GetLengthXYSquared() const;
    float      GetAngleAboutZRadians() const;
    float      GetAngleAboutZDegrees() const;
    const Vec3 GetRotatedAboutZRadians(float deltaRadians) const;
    const Vec3 GetRotatedAboutZDegrees(float deltaDegrees) const;
    const Vec3 GetClamped(float maxLength) const;
    const Vec3 GetNormalized() const;

    // Static methods
    const static Vec3 MakeFromPolarRadians(float pitchRadians, float yawRadians, float length = 1.0f);
    const static Vec3 MakeFromPolarDegrees(float pitchDegrees, float yawDegrees, float length = 1.0f);

    // Operators (const)
    bool        operator==(const Vec3& compare) const; // vec3 == vec3
    bool        operator!=(const Vec3& compare) const; // vec3 != vec3
    const Vec3  operator+(const Vec3& vecToAdd) const; // vec3 + vec3
    const Vec3  operator-() const;
    const Vec3  operator-(const Vec3& vecToSubtract) const; // vec3 - vec3
    const Vec3  operator*(float uniformScale) const; // vec3 * float
    const Vec3  operator/(float inverseScale) const; // vec3 / float
    friend bool operator<(const Vec3& lhs, const Vec3& rhs);
    friend bool operator<=(const Vec3& lhs, const Vec3& rhs);
    friend bool operator>(const Vec3& lhs, const Vec3& rhs);
    friend bool operator>=(const Vec3& lhs, const Vec3& rhs);

    // Operators (self-mutating / non-const)
    void operator+=(const Vec3& vecToAdd); // vec3 += vec3
    void operator-=(const Vec3& vecToSubtract); // vec3 -= vec3
    void operator*=(float uniformScale); // vec3 *= float
    void operator/=(float uniformDivisor); // vec3 /= float
    void operator=(const Vec3& copyFrom); // vec3 = vec3

    // Standalone "friend" functions that are conceptually, but not actually, part of Vec3::
    friend const Vec3 operator*(float uniformScale, const Vec3& vecToScale); // float * vec3

    // String Operations
    void SetFromText(const char* text);
};
