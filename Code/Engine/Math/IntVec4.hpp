#pragma once
#include <string>

struct Vec4;
struct IntVec3;
// Forward declarations
struct Vec3;
struct Vec2;
struct IntVec2;

struct IntVec4
{
    static IntVec4 ZERO;
    static IntVec4 ONE;
    static IntVec4 INVALID;

    int x = 0, y = 0, z = 0, w = 0;

    ~IntVec4();

    IntVec4();

    IntVec4(const IntVec4& copyFrom);
    explicit IntVec4(int initialX, int initialY, int initialZ, int initialW);


    // Conversion constructors
    explicit IntVec4(const Vec4& vec4); // Convert from Vec4 (with rounding)
    explicit IntVec4(const IntVec3& intVec3, int initialW = 0); // Convert from IntVec3 + w
    explicit IntVec4(const Vec3& vec3, int initialW = 0); // Convert from Vec3 + w (with rounding)

    // Accessors
    float         GetLength() const;
    int           GetTaxicabLength() const;
    int           GetLengthSquared() const;
    float         GetLengthXY() const;
    int           GetLengthXYSquared() const;
    const IntVec2 GetXY() const; // Get IntVec2 from x,y components

    // Conversion methods
    Vec3    ToVec3() const; // Convert to Vec3
    IntVec2 ToIntVec2() const; // Convert to IntVec2 (drops z)

    // Mutators (non-const methods)
    void Rotate90DegreesAboutZ();
    void RotateMinus90DegreesAboutZ();

    // Operators (self-mutating / non-const)
    void operator=(const IntVec4& copyFrom); // IntVec4 = IntVec4
    void operator+=(const IntVec4& vecToAdd); // vec3 += vec3
    void operator-=(const IntVec4& vecToSubtract); // vec3 -= vec3
    void operator*=(int uniformScale); // vec3 *= int
    void operator/=(int uniformDivisor); // vec3 /= int

    // Operators (const)
    bool          operator==(const IntVec4& compare) const; // vec3 == vec3
    bool          operator!=(const IntVec4& compare) const; // vec3 != vec3
    const IntVec4 operator+(const IntVec4& vecToAdd) const; // vec3 + vec3
    const IntVec4 operator-(const IntVec4& vecToSubtract) const; // vec3 - vec3
    const IntVec4 operator-() const; // -vec3, i.e. "unary negation"
    const IntVec4 operator*(int uniformScale) const; // vec3 * int
    const IntVec4 operator*(const IntVec4& vecToMultiply) const; // vec3 * vec3
    const IntVec4 operator/(int inverseScale) const; // vec3 / int

    // Standalone "friend" functions that are conceptually, but not actually, part of IntVec4::
    friend const IntVec4 operator*(int uniformScale, const IntVec4& vecToScale); // int * vec3

    // String Operations
    void        SetFromText(const char* text);
    std::string toString() const;
};
