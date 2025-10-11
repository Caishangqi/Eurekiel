#include "IntVec4.hpp"

#include <cmath>
#include "MathUtils.hpp"
#include "Vec3.hpp"
#include "Vec2.hpp"
#include "IntVec2.hpp"
#include "IntVec3.hpp"
#include "Engine/Core/StringUtils.hpp"

IntVec4 IntVec4::ZERO    = IntVec4(0, 0, 0, 0);
IntVec4 IntVec4::ONE     = IntVec4(1, 1, 1, 1);
IntVec4 IntVec4::INVALID = IntVec4(-1, -1, -1, -1);

IntVec4::~IntVec4()
{
}

IntVec4::IntVec4()
{
}

IntVec4::IntVec4(const IntVec4& copyFrom)
{
    x = copyFrom.x;
    y = copyFrom.y;
    z = copyFrom.z;
    w = copyFrom.w;
}

IntVec4::IntVec4(int initialX, int initialY, int initialZ, int initialW)
    : x(initialX), y(initialY), z(initialZ), w(initialW)
{
}

IntVec4::IntVec4(const Vec4& vec4) : x(RoundDownToInt(vec4.x)), y(RoundDownToInt(vec4.y)), z(RoundDownToInt(vec4.z)), w(RoundDownToInt(vec4.w))
{
}

IntVec4::IntVec4(const IntVec3& intVec3, int initialW) : x(intVec3.x), y(intVec3.y), z(intVec3.z), w(initialW)
{
}

// Conversion constructors

IntVec4::IntVec4(const Vec3& vec3, int initialW) : x(RoundDownToInt(vec3.x)), y(RoundDownToInt(vec3.y)), z(RoundDownToInt(vec3.z)), w(initialW)
{
}

float IntVec4::GetLength() const
{
    return static_cast<float>(sqrt(x * x + y * y + z * z + w * w));
}

int IntVec4::GetTaxicabLength() const
{
    int absX = (x < 0) ? -x : x;
    int absY = (y < 0) ? -y : y;
    int absZ = (z < 0) ? -z : z;
    int absW = (w < 0) ? -w : w;
    return absX + absY + absZ + absW;
}

int IntVec4::GetLengthSquared() const
{
    return x * x + y * y + z * z + w * w;
}

float IntVec4::GetLengthXY() const
{
    return static_cast<float>(sqrt(x * x + y * y + z * z));
}

int IntVec4::GetLengthXYSquared() const
{
    return x * x + y * y + z * z;
}

const IntVec2 IntVec4::GetXY() const
{
    return IntVec2(x, y);
}

// Conversion methods
Vec3 IntVec4::ToVec3() const
{
    return Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
}

IntVec2 IntVec4::ToIntVec2() const
{
    return IntVec2(x, y);
}

void IntVec4::Rotate90DegreesAboutZ()
{
    int temp = x;
    x        = -y;
    y        = temp;
    // z remains unchanged
}

void IntVec4::RotateMinus90DegreesAboutZ()
{
    int temp = x;
    x        = y;
    y        = -temp;
    // z remains unchanged
}

void IntVec4::operator=(const IntVec4& copyFrom)
{
    x = copyFrom.x;
    y = copyFrom.y;
    z = copyFrom.z;
}

void IntVec4::operator+=(const IntVec4& vecToAdd)
{
    x += vecToAdd.x;
    y += vecToAdd.y;
    z += vecToAdd.z;
}

void IntVec4::operator-=(const IntVec4& vecToSubtract)
{
    x -= vecToSubtract.x;
    y -= vecToSubtract.y;
    z -= vecToSubtract.z;
}

void IntVec4::operator*=(int uniformScale)
{
    x *= uniformScale;
    y *= uniformScale;
    z *= uniformScale;
}

void IntVec4::operator/=(int uniformDivisor)
{
    x /= uniformDivisor;
    y /= uniformDivisor;
    z /= uniformDivisor;
}

bool IntVec4::operator==(const IntVec4& compare) const
{
    return (x == compare.x) && (y == compare.y) && (z == compare.z);
}

bool IntVec4::operator!=(const IntVec4& compare) const
{
    return !(*this == compare);
}

const IntVec4 IntVec4::operator+(const IntVec4& vecToAdd) const
{
    return IntVec4(x + vecToAdd.x, y + vecToAdd.y, z + vecToAdd.z, w + vecToAdd.w);
}

const IntVec4 IntVec4::operator-(const IntVec4& vecToSubtract) const
{
    return IntVec4(x - vecToSubtract.x, y - vecToSubtract.y, z - vecToSubtract.z, w - vecToSubtract.w);
}

const IntVec4 IntVec4::operator-() const
{
    return IntVec4(-x, -y, -z, -w);
}

const IntVec4 IntVec4::operator*(int uniformScale) const
{
    return IntVec4(x * uniformScale, y * uniformScale, z * uniformScale, w * uniformScale);
}

const IntVec4 IntVec4::operator*(const IntVec4& vecToMultiply) const
{
    return IntVec4(x * vecToMultiply.x, y * vecToMultiply.y, z * vecToMultiply.z, w * vecToMultiply.w);
}

const IntVec4 IntVec4::operator/(int inverseScale) const
{
    return IntVec4(x / inverseScale, y / inverseScale, z / inverseScale, w / inverseScale);
}

const IntVec4 operator*(int uniformScale, const IntVec4& vecToScale)
{
    return IntVec4(uniformScale * vecToScale.x, uniformScale * vecToScale.y, uniformScale * vecToScale.z, uniformScale * vecToScale.w);
}

void IntVec4::SetFromText(const char* text)
{
    Strings splitStrings = SplitStringOnDelimiter(text, ',');
    if (splitStrings.size() >= 4)
    {
        x = atoi(splitStrings[0].c_str());
        y = atoi(splitStrings[1].c_str());
        z = atoi(splitStrings[2].c_str());
        w = atoi(splitStrings[3].c_str());
    }
}

std::string IntVec4::toString() const
{
    return Stringf("(%d,%d,%d,%d)", x, y, z, w);
}
