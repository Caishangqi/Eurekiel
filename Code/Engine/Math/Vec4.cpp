#include "Vec4.hpp"

#include "Engine/Core/StringUtils.hpp"

Vec4::Vec4(float initialX, float initialY, float initialZ, float initialW) : x(initialX), y(initialY), z(initialZ),
                                                                             w(initialW)
{
}

bool Vec4::operator==(const Vec4& compare) const
{
    return x == compare.x && y == compare.y && z == compare.z && w == compare.w;
}

bool Vec4::operator!=(const Vec4& compare) const
{
    return !(*this == compare);
}

const Vec4 Vec4::operator+(const Vec4& vecToAdd) const
{
    return Vec4(x + vecToAdd.x, y + vecToAdd.y, z + vecToAdd.z, w + vecToAdd.w);
}

const Vec4 Vec4::operator-(const Vec4& vecToSubtract) const
{
    return Vec4(x - vecToSubtract.x, y - vecToSubtract.y, z - vecToSubtract.z, w - vecToSubtract.w);
}

const Vec4 Vec4::operator*(float uniformScale) const
{
    return Vec4(x * uniformScale, y * uniformScale, z * uniformScale, w * uniformScale);
}

const Vec4 Vec4::operator/(float inverseScale) const
{
    return Vec4(x / inverseScale, y / inverseScale, z / inverseScale, w / inverseScale);
}

void Vec4::operator+=(const Vec4& vecToAdd)
{
    x += vecToAdd.x;
    y += vecToAdd.y;
    z += vecToAdd.z;
    w += vecToAdd.w;
}

void Vec4::operator-=(const Vec4& vecToSubtract)
{
    x -= vecToSubtract.x;
    y -= vecToSubtract.y;
    z -= vecToSubtract.z;
    w -= vecToSubtract.w;
}

void Vec4::operator*=(float uniformScale)
{
    x *= uniformScale;
    y *= uniformScale;
    z *= uniformScale;
    w *= uniformScale;
}

void Vec4::operator/=(float uniformDivisor)
{
    x /= uniformDivisor;
    y /= uniformDivisor;
    z /= uniformDivisor;
    w /= uniformDivisor;
}

Vec4& Vec4::operator=(const Vec4& copyFrom)
{
    this->x = copyFrom.x;
    this->y = copyFrom.y;
    this->z = copyFrom.z;
    this->w = copyFrom.w;
    return *this;
}

void Vec4::SetFromText(const char* text)
{
    // Split the input text on the comma delimiter
    Strings parts = SplitStringOnDelimiter(text, ',');

    if (parts.size() != 4)
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
    x = static_cast<float>(atof(parts[0].c_str()));
    y = static_cast<float>(atof(parts[1].c_str()));
    z = static_cast<float>(atof(parts[2].c_str()));
    w = static_cast<float>(atof(parts[3].c_str()));
}
