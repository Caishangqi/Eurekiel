#include "IntVec3.hpp"

#include <cmath>
#include "MathUtils.hpp"
#include "Vec3.hpp"
#include "Vec2.hpp"
#include "IntVec2.hpp"
#include "Engine/Core/StringUtils.hpp"

IntVec3 IntVec3::ZERO    = IntVec3(0, 0, 0);
IntVec3 IntVec3::ONE     = IntVec3(1, 1, 1);
IntVec3 IntVec3::INVALID = IntVec3(-1, -1, -1);

IntVec3::~IntVec3()
{
}

IntVec3::IntVec3()
{
}

IntVec3::IntVec3(const IntVec3& copyFrom)
{
    x = copyFrom.x;
    y = copyFrom.y;
    z = copyFrom.z;
}

IntVec3::IntVec3(int initialX, int initialY, int initialZ)
    : x(initialX), y(initialY), z(initialZ)
{
}

// Conversion constructors
IntVec3::IntVec3(const Vec3& vec3)
    : x(RoundDownToInt(vec3.x)), y(RoundDownToInt(vec3.y)), z(RoundDownToInt(vec3.z))
{
}

IntVec3::IntVec3(const IntVec2& intVec2, int initialZ)
    : x(intVec2.x), y(intVec2.y), z(initialZ)
{
}

IntVec3::IntVec3(const Vec2& vec2, int initialZ)
    : x(RoundDownToInt(vec2.x)), y(RoundDownToInt(vec2.y)), z(initialZ)
{
}

float IntVec3::GetLength() const
{
    return static_cast<float>(sqrt(x * x + y * y + z * z));
}

int IntVec3::GetTaxicabLength() const
{
    int absX = (x < 0) ? -x : x;
    int absY = (y < 0) ? -y : y;
    int absZ = (z < 0) ? -z : z;
    return absX + absY + absZ;
}

int IntVec3::GetLengthSquared() const
{
    return x * x + y * y + z * z;
}

float IntVec3::GetLengthXY() const
{
    return static_cast<float>(sqrt(x * x + y * y));
}

int IntVec3::GetLengthXYSquared() const
{
    return x * x + y * y;
}

const IntVec3 IntVec3::GetRotated90DegreesAboutZ() const
{
    return IntVec3(-y, x, z);
}

const IntVec3 IntVec3::GetRotatedMinus90DegreesAboutZ() const
{
    return IntVec3(y, -x, z);
}

const IntVec2 IntVec3::GetXY() const
{
    return IntVec2(x, y);
}

// Conversion methods
Vec3 IntVec3::ToVec3() const
{
    return Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
}

IntVec2 IntVec3::ToIntVec2() const
{
    return IntVec2(x, y);
}

void IntVec3::Rotate90DegreesAboutZ()
{
    int temp = x;
    x        = -y;
    y        = temp;
    // z remains unchanged
}

void IntVec3::RotateMinus90DegreesAboutZ()
{
    int temp = x;
    x        = y;
    y        = -temp;
    // z remains unchanged
}

void IntVec3::operator=(const IntVec3& copyFrom)
{
    x = copyFrom.x;
    y = copyFrom.y;
    z = copyFrom.z;
}

void IntVec3::operator+=(const IntVec3& vecToAdd)
{
    x += vecToAdd.x;
    y += vecToAdd.y;
    z += vecToAdd.z;
}

void IntVec3::operator-=(const IntVec3& vecToSubtract)
{
    x -= vecToSubtract.x;
    y -= vecToSubtract.y;
    z -= vecToSubtract.z;
}

void IntVec3::operator*=(int uniformScale)
{
    x *= uniformScale;
    y *= uniformScale;
    z *= uniformScale;
}

void IntVec3::operator/=(int uniformDivisor)
{
    x /= uniformDivisor;
    y /= uniformDivisor;
    z /= uniformDivisor;
}

bool IntVec3::operator==(const IntVec3& compare) const
{
    return (x == compare.x) && (y == compare.y) && (z == compare.z);
}

bool IntVec3::operator!=(const IntVec3& compare) const
{
    return !(*this == compare);
}

const IntVec3 IntVec3::operator+(const IntVec3& vecToAdd) const
{
    return IntVec3(x + vecToAdd.x, y + vecToAdd.y, z + vecToAdd.z);
}

const IntVec3 IntVec3::operator-(const IntVec3& vecToSubtract) const
{
    return IntVec3(x - vecToSubtract.x, y - vecToSubtract.y, z - vecToSubtract.z);
}

const IntVec3 IntVec3::operator-() const
{
    return IntVec3(-x, -y, -z);
}

const IntVec3 IntVec3::operator*(int uniformScale) const
{
    return IntVec3(x * uniformScale, y * uniformScale, z * uniformScale);
}

const IntVec3 IntVec3::operator*(const IntVec3& vecToMultiply) const
{
    return IntVec3(x * vecToMultiply.x, y * vecToMultiply.y, z * vecToMultiply.z);
}

const IntVec3 IntVec3::operator/(int inverseScale) const
{
    return IntVec3(x / inverseScale, y / inverseScale, z / inverseScale);
}

const IntVec3 operator*(int uniformScale, const IntVec3& vecToScale)
{
    return IntVec3(uniformScale * vecToScale.x, uniformScale * vecToScale.y, uniformScale * vecToScale.z);
}

void IntVec3::SetFromText(const char* text)
{
    Strings splitStrings = SplitStringOnDelimiter(text, ',');
    if (splitStrings.size() >= 3)
    {
        x = atoi(splitStrings[0].c_str());
        y = atoi(splitStrings[1].c_str());
        z = atoi(splitStrings[2].c_str());
    }
}

std::string IntVec3::toString() const
{
    return Stringf("(%d,%d,%d)", x, y, z);
}
