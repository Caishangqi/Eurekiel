#include "Vec2.hpp"
//#include "Engine/Math/MathUtils.hpp"
//#include "Engine/Core/EngineCommon.hpp"

/**
 * Copy Constructor
 * @param copy target for copy
 */
Vec2::Vec2(const Vec2& copy)
{
    this->x = copy.x;
    this->y = copy.y;
}


//-----------------------------------------------------------------------------------------------
Vec2::Vec2(float initialX, float initialY)
    : x(initialX)
      , y(initialY)
{
}


//-----------------------------------------------------------------------------------------------
const Vec2 Vec2::operator +(const Vec2& vecToAdd) const
{
    return Vec2(this->x + vecToAdd.x, this->y + vecToAdd.y);
}


//-----------------------------------------------------------------------------------------------
const Vec2 Vec2::operator-(const Vec2& vecToSubtract) const
{
    return Vec2(this->x - vecToSubtract.x, this->y - vecToSubtract.y);
}


//------------------------------------------------------------------------------------------------
const Vec2 Vec2::operator-() const
{
    return Vec2(-this->x, -this->y);
}


//-----------------------------------------------------------------------------------------------
const Vec2 Vec2::operator*(float uniformScale) const
{
    return Vec2(this->x * uniformScale, this->y * uniformScale);
}


//------------------------------------------------------------------------------------------------
const Vec2 Vec2::operator*(const Vec2& vecToMultiply) const
{
    return Vec2(this->x * vecToMultiply.x, this->y * vecToMultiply.y);
}


//-----------------------------------------------------------------------------------------------
const Vec2 Vec2::operator/(float inverseScale) const
{
    return Vec2(this->x / inverseScale, this->y / inverseScale);
}


//-----------------------------------------------------------------------------------------------
void Vec2::operator+=(const Vec2& vecToAdd)
{
    this->x += vecToAdd.x;
    this->y += vecToAdd.y;
}


//-----------------------------------------------------------------------------------------------
void Vec2::operator-=(const Vec2& vecToSubtract)
{
    this->x -= vecToSubtract.x;
    this->y -= vecToSubtract.y;
}


//-----------------------------------------------------------------------------------------------
void Vec2::operator*=(const float uniformScale)
{
    this->x *= uniformScale;
    this->y *= uniformScale;
}


//-----------------------------------------------------------------------------------------------
void Vec2::operator/=(const float uniformDivisor)
{
    this->x /= uniformDivisor;
    this->y /= uniformDivisor;
}


//-----------------------------------------------------------------------------------------------
void Vec2::operator=(const Vec2& copyFrom)
{
    this->x = copyFrom.x;
    this->y = copyFrom.y;
}


//-----------------------------------------------------------------------------------------------
const Vec2 operator*(float uniformScale, const Vec2& vecToScale)
{
    return Vec2(uniformScale * vecToScale.x, uniformScale * vecToScale.y);
}


//-----------------------------------------------------------------------------------------------
bool Vec2::operator==(const Vec2& compare) const
{
    return this->x == compare.x && this->y == compare.y;
}


//-----------------------------------------------------------------------------------------------
bool Vec2::operator!=(const Vec2& compare) const
{
    return this->x != compare.x || this->y != compare.y;
}
