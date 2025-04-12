#include "IntVec2.hpp"

#include <corecrt_math.h>

#include "MathUtils.hpp"
#include "Engine/Core/StringUtils.hpp"

IntVec2 IntVec2::ZERO    = IntVec2(0, 0);
IntVec2 IntVec2::ONE     = IntVec2(1, 1);
IntVec2 IntVec2::INVALID = IntVec2(-1, -1);

IntVec2::~IntVec2()
{
}

IntVec2::IntVec2()
{
}

IntVec2::IntVec2(const IntVec2& copyFrom)
{
    x = copyFrom.x;
    y = copyFrom.y;
}

IntVec2::IntVec2(int initialX, int initialY): x(initialX), y(initialY)
{
}

float IntVec2::GetLength() const
{
    return static_cast<float>(sqrt(x * x + y * y));
}

int IntVec2::GetTaxicabLength() const
{
    int absX;
    int absY;
    if (x < 0)
        absX = -x;
    else
        absX = x;
    if (y < 0)
        absY = -y;
    else
        absY = y;
    return absX + absY;
}

int IntVec2::GetLengthSquared() const
{
    return (x * x + y * y);
}

float IntVec2::GetOrientationRadians() const
{
    return static_cast<float>(atan2(y, x));
}

float IntVec2::GetOrientationDegrees() const
{
    return ConvertRadiansToDegrees(static_cast<float>(atan2(y, x)));
}

const IntVec2 IntVec2::GetRotated90Degrees() const
{
    IntVec2 p = *this;
    p.x       = -y;
    p.y       = x;
    return p;
}

const IntVec2 IntVec2::GetRotatedMinus90Degrees() const
{
    IntVec2 p = *this;
    p.x       = y;
    p.y       = -x;
    return p;
}

void IntVec2::Rotate90Degrees()
{
    int temp_x = x;
    x          = -y;
    y          = temp_x;
}

void IntVec2::RotateMinus90Degrees()
{
    int temp_x = x;
    this->x    = y;
    this->y    = -temp_x;
}

void IntVec2::operator=(const IntVec2& copyFrom)
{
    x = copyFrom.x;
    y = copyFrom.y;
}

void IntVec2::operator+=(const IntVec2& vecToAdd)
{
    x += vecToAdd.x;
    y += vecToAdd.y;
}

void IntVec2::operator-=(const IntVec2& vecToSubtract)
{
    x -= vecToSubtract.x;
    y -= vecToSubtract.y;
}

void IntVec2::operator*=(int uniformScale)
{
    x *= uniformScale;
    y *= uniformScale;
}

void IntVec2::operator/=(int uniformDivisor)
{
    x /= uniformDivisor;
    y /= uniformDivisor;
}

bool IntVec2::operator==(const IntVec2& compare) const
{
    return x == compare.x && y == compare.y;
}

bool IntVec2::operator!=(const IntVec2& compare) const
{
    return x != compare.x || y != compare.y;
}

const IntVec2 IntVec2::operator+(const IntVec2& vecToAdd) const
{
    return IntVec2(x + vecToAdd.x, y + vecToAdd.y);
}

const IntVec2 IntVec2::operator-(const IntVec2& vecToSubtract) const
{
    return IntVec2(x - vecToSubtract.x, y - vecToSubtract.y);
}

const IntVec2 IntVec2::operator-() const
{
    return IntVec2(-x, -y);
}

const IntVec2 IntVec2::operator*(int uniformScale) const
{
    return IntVec2(x * uniformScale, y * uniformScale);
}

const IntVec2 IntVec2::operator*(const IntVec2& vecToMultiply) const
{
    return IntVec2(x * vecToMultiply.x, y * vecToMultiply.y);
}

const IntVec2 IntVec2::operator/(int inverseScale) const
{
    return IntVec2(x / inverseScale, y / inverseScale);
}

void IntVec2::SetFromText(const char* text)
{
    if (text == nullptr)
    {
        return;
    }

    // Split the input text on the comma delimiter
    Strings parts = SplitStringOnDelimiter(text, ',');

    if (parts.size() != 2)
    {
        return;
    }

    // Remove whitespace from each part
    for (std::string& part : parts)
    {
        for (size_t i = 0; i < part.length(); ++i)
        {
            if (isspace(part[i]))
            {
                part.erase(i, 1);
                --i;
            }
        }
    }

    // Convert the strings to floats and set x and y
    x = atoi(parts[0].c_str());
    y = atoi(parts[1].c_str());
}

std::string IntVec2::toString() const
{
    return std::to_string(x) + "," + std::to_string(y);
}
