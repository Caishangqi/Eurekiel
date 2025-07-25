#pragma once
#include <ostream>
#include <string>

struct IntVec2;
struct Vec3;

//-----------------------------------------------------------------------------------------------
struct Vec2
{
    static Vec2 ZERO;
    static Vec2 ONE;
    static Vec2 INVALID;
    // NOTE: this is one of the few cases where we break both the "m_" naming rule AND the avoid-public-members rule
    float x = 0.f;
    float y = 0.f;
    // Construction/Destruction
    ~Vec2(); // destructor (do nothing)
    Vec2(); // default constructor (do nothing)
    Vec2(const Vec2& copyFrom); // copy constructor (from another vec2)
    Vec2(const IntVec2& copyFrom); // copy constructor (from another vec2)
    explicit Vec2(float initialX, float initialY); // explicit constructor (from x, y)

    // Static methods (e.g. creation functions)
    static const Vec2 MakeFromPolarRadians(float orientationRadians, float length = 1.f);
    static const Vec2 MakeFromPolarDegrees(float orientationDegrees, float length = 1.f);

    // Accessors (const methods)
    float      GetLength() const; // Gives R for X,Y
    float      GetLengthSquared() const;
    float      GetOrientationRadians() const;
    float      GetOrientationDegrees() const; // Gives Theta for X, Y
    const Vec2 GetRotated90Degrees() const;
    const Vec2 GetRotatedMinus90Degrees() const;
    const Vec2 GetRotatedRadians(float deltaRadians) const;
    const Vec2 GetRotatedDegrees(float deltaDegrees) const;
    const Vec2 GetClamped(float maxLength) const; // Get the clamped version of the vector
    const Vec2 GetNormalized() const;
    Vec3       GetAsVec3(float z = 0.f) const;

    const Vec2 GetReflected(const Vec2& normalOfSurfaceToReflectOffOf) const;
    void       Reflect(const Vec2& normalOfSurfaceToReflectOffOf);


    // Operators (const)
    bool       operator==(const Vec2& compare) const; // vec2 == vec2
    bool       operator!=(const Vec2& compare) const; // vec2 != vec2
    const Vec2 operator+(const Vec2& vecToAdd) const; // vec2 + vec2
    const Vec2 operator-(const Vec2& vecToSubtract) const; // vec2 - vec2
    const Vec2 operator-() const; // -vec2, i.e. "unary negation"
    const Vec2 operator*(float uniformScale) const; // vec2 * float
    const Vec2 operator*(const Vec2& vecToMultiply) const; // vec2 * vec2
    const Vec2 operator/(float inverseScale) const; // vec2 / float

    // Mutators (non-const methods)
    void  SetOrientationRadians(float newOrientationRadians);
    void  SetOrientationDegrees(float newOrientationDegrees);
    void  SetPolarRadians(float newOrientationRadians, float newLength);
    void  SetPolarDegrees(float newOrientationDegrees, float newLength);
    void  Rotate90Degrees();
    void  RotateMinus90Degrees();
    void  RotateRadians(float deltaRadians);
    void  RotateDegrees(float deltaDegrees);
    void  SetLength(float newLength);
    void  ClampLength(float maxLength); // Like speed limited
    void  Normalize();
    float NormalizeAndGetPreviousLength();


    // Operators (self-mutating / non-const)
    void operator+=(const Vec2& vecToAdd); // vec2 += vec2
    void operator-=(const Vec2& vecToSubtract); // vec2 -= vec2
    void operator*=(float uniformScale); // vec2 *= float
    void operator/=(float uniformDivisor); // vec2 /= float
    void operator=(const Vec2& copyFrom); // vec2 = vec2

    // Standalone "friend" functions that are conceptually, but not actually, part of Vec2::
    friend const Vec2 operator*(float uniformScale, const Vec2& vecToScale); // float * vec2

    // String Operations
    void SetFromText(const char* text); // Parses "6,4" or "-.3, 0.05" to (x,y)

    std::string toString() const;
};
