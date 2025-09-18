#pragma once
#include <string>

// Forward declaration
struct IntVec3;


struct IntVec2
{
    static IntVec2 ZERO;
    static IntVec2 ONE;
    static IntVec2 INVALID;

    int x = 0, y = 0;

    ~IntVec2();

    IntVec2();

    IntVec2(const IntVec2& copyFrom);
    explicit IntVec2(int initialX, int initialY);
    explicit IntVec2(const IntVec3& intVec3); // Convert from IntVec3 (drops z)

    // Accessors
    float         GetLength() const;
    int           GetTaxicabLength() const;
    int           GetLengthSquared() const;
    float         GetOrientationRadians() const;
    float         GetOrientationDegrees() const;
    const IntVec2 GetRotated90Degrees() const;
    const IntVec2 GetRotatedMinus90Degrees() const;

    // Mutators (non-const methods)
    void Rotate90Degrees();
    void RotateMinus90Degrees();

    // Operators (self-mutating / non-const)
    void operator=(const IntVec2& copyFrom); // IntVec2 = IntVec2
    void operator+=(const IntVec2& vecToAdd); // vec2 += vec2
    void operator-=(const IntVec2& vecToSubtract); // vec2 -= vec2
    void operator*=(int uniformScale); // vec2 *= float
    void operator/=(int uniformDivisor); // vec2 /= float

    // Operators (const)
    bool          operator==(const IntVec2& compare) const; // vec2 == vec2
    bool          operator!=(const IntVec2& compare) const; // vec2 != vec2
    bool          operator<(const IntVec2& compare) const; // vec2 < vec2 (for sorting)
    const IntVec2 operator+(const IntVec2& vecToAdd) const; // vec2 + vec2
    const IntVec2 operator-(const IntVec2& vecToSubtract) const; // vec2 - vec2
    const IntVec2 operator-() const; // -vec2, i.e. "unary negation"
    const IntVec2 operator*(int uniformScale) const; // vec2 * float
    const IntVec2 operator*(const IntVec2& vecToMultiply) const; // vec2 * vec2
    const IntVec2 operator/(int inverseScale) const; // vec2 / float

    // Standalone "friend" functions that are conceptually, but not actually, part of Vec2::
    friend const IntVec2 operator*(float uniformScale, const IntVec2& vecToScale); // float * vec2

    // String Operations
    void        SetFromText(const char* text);
    std::string toString() const;
};

// Hash function for IntVec2 (required for unordered containers)
#include <functional>

namespace std
{
    template <>
    struct hash<IntVec2>
    {
        size_t operator()(const IntVec2& pos) const
        {
            // Use a simple but effective hash combining x and y
            uint64_t ux = static_cast<uint64_t>(static_cast<uint32_t>(pos.x));
            uint64_t uy = static_cast<uint64_t>(static_cast<uint32_t>(pos.y));
            return static_cast<size_t>((uy << 32) | ux);
        }
    };
}
