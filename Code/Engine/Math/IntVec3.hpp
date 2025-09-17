#pragma once
#include <string>

// Forward declarations
struct Vec3;
struct Vec2;
struct IntVec2;

struct IntVec3
{
    static IntVec3 ZERO;
    static IntVec3 ONE;
    static IntVec3 INVALID;

    int x = 0, y = 0, z = 0;

    ~IntVec3();

    IntVec3();

    IntVec3(const IntVec3& copyFrom);
    explicit IntVec3(int initialX, int initialY, int initialZ);

    // Conversion constructors
    explicit IntVec3(const Vec3& vec3); // Convert from Vec3 (with rounding)
    explicit IntVec3(const IntVec2& intVec2, int initialZ = 0); // Convert from IntVec2 + z
    explicit IntVec3(const Vec2& vec2, int initialZ = 0); // Convert from Vec2 + z (with rounding)

    // Accessors
    float         GetLength() const;
    int           GetTaxicabLength() const;
    int           GetLengthSquared() const;
    float         GetLengthXY() const;
    int           GetLengthXYSquared() const;
    const IntVec3 GetRotated90DegreesAboutZ() const;
    const IntVec3 GetRotatedMinus90DegreesAboutZ() const;
    const IntVec2 GetXY() const; // Get IntVec2 from x,y components

    // Conversion methods
    Vec3    ToVec3() const; // Convert to Vec3
    IntVec2 ToIntVec2() const; // Convert to IntVec2 (drops z)

    // Mutators (non-const methods)
    void Rotate90DegreesAboutZ();
    void RotateMinus90DegreesAboutZ();

    // Operators (self-mutating / non-const)
    void operator=(const IntVec3& copyFrom); // IntVec3 = IntVec3
    void operator+=(const IntVec3& vecToAdd); // vec3 += vec3
    void operator-=(const IntVec3& vecToSubtract); // vec3 -= vec3
    void operator*=(int uniformScale); // vec3 *= int
    void operator/=(int uniformDivisor); // vec3 /= int

    // Operators (const)
    bool          operator==(const IntVec3& compare) const; // vec3 == vec3
    bool          operator!=(const IntVec3& compare) const; // vec3 != vec3
    const IntVec3 operator+(const IntVec3& vecToAdd) const; // vec3 + vec3
    const IntVec3 operator-(const IntVec3& vecToSubtract) const; // vec3 - vec3
    const IntVec3 operator-() const; // -vec3, i.e. "unary negation"
    const IntVec3 operator*(int uniformScale) const; // vec3 * int
    const IntVec3 operator*(const IntVec3& vecToMultiply) const; // vec3 * vec3
    const IntVec3 operator/(int inverseScale) const; // vec3 / int

    // Standalone "friend" functions that are conceptually, but not actually, part of IntVec3::
    friend const IntVec3 operator*(int uniformScale, const IntVec3& vecToScale); // int * vec3

    // String Operations
    void        SetFromText(const char* text);
    std::string toString() const;
};
