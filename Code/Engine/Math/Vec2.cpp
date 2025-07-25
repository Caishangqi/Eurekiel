#include <valarray>

#include "IntVec2.hpp"
#include "MathUtils.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"
//#include "Engine/Math/MathUtils.hpp"
//#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec2.hpp"
Vec2 Vec2::ZERO    = Vec2(0.0f, 0.0f);
Vec2 Vec2::ONE     = Vec2(1.0f, 1.0f);
Vec2 Vec2::INVALID = Vec2(-1.0f, -1.0f);

Vec2::~Vec2()
{
}

Vec2::Vec2()
{
}

/**
 * Copy Constructor
 * @param copy target for copy
 */
Vec2::Vec2(const Vec2& copy)
{
    this->x = copy.x;
    this->y = copy.y;
}

Vec2::Vec2(const IntVec2& copyFrom)
{
    this->x = static_cast<float>(copyFrom.x);
    this->y = static_cast<float>(copyFrom.y);
}


//-----------------------------------------------------------------------------------------------
Vec2::Vec2(float initialX, float initialY)
    : x(initialX)
      , y(initialY)
{
}

const Vec2 Vec2::MakeFromPolarRadians(float orientationRadians, float length)
{
    float x = length * CosDegrees(ConvertRadiansToDegrees(orientationRadians));
    float y = length * SinDegrees(ConvertRadiansToDegrees(orientationRadians));
    return Vec2(x, y);
}

const Vec2 Vec2::MakeFromPolarDegrees(float orientationDegrees, float length)
{
    float x = length * CosDegrees(orientationDegrees);
    float y = length * SinDegrees(orientationDegrees);
    return Vec2(x, y);
}

float Vec2::GetLength() const
{
    return sqrt(x * x + y * y);
}

float Vec2::GetLengthSquared() const
{
    return x * x + y * y;
}

float Vec2::GetOrientationRadians() const
{
    return atan2(y, x);
}

float Vec2::GetOrientationDegrees() const
{
    return Atan2Degrees(y, x);
}

const Vec2 Vec2::GetRotated90Degrees() const
{
    Vec2 vec_ptr = *this;
    vec_ptr.x    = -this->y;
    vec_ptr.y    = this->x;
    return vec_ptr;
}

const Vec2 Vec2::GetRotatedMinus90Degrees() const
{
    Vec2 vec_ptr = *this;
    vec_ptr.x    = this->y;
    vec_ptr.y    = -this->x;
    return vec_ptr;
}

const Vec2 Vec2::GetRotatedRadians(float deltaRadians) const
{
    Vec2  vec_ptr      = *this;
    float deltaDegrees = ConvertRadiansToDegrees(deltaRadians);
    TransformPosition2D(vec_ptr, 1.f, deltaDegrees, Vec2(0, 0));
    return vec_ptr;
}

const Vec2 Vec2::GetRotatedDegrees(float deltaDegrees) const
{
    Vec2 vec_ptr = *this;
    TransformPosition2D(vec_ptr, 1.f, deltaDegrees, Vec2(0, 0));
    return vec_ptr;
}

const Vec2 Vec2::GetClamped(float maxLength) const
{
    Vec2 vec_ptr = *this;
    if (vec_ptr.GetLength() > maxLength)
        vec_ptr.SetLength(maxLength);
    return vec_ptr;
}

const Vec2 Vec2::GetNormalized() const
{
    Vec2  vec_ptr = *this;
    float length  = GetLength();
    if (length == 0.f)
        return *this;
    float scale = 1.f / length;
    vec_ptr.x *= scale;
    vec_ptr.y *= scale;
    return vec_ptr;
}

Vec3 Vec2::GetAsVec3(float z) const
{
    return Vec3(x, y, z);
}


const Vec2 Vec2::GetReflected(const Vec2& normalOfSurfaceToReflectOffOf) const
{
    Vec2 normalizedNormalOfSurfaceToReflectOffOf = normalOfSurfaceToReflectOffOf.GetNormalized();
    // Get Vn length composite of Vector
    float V_n        = x * normalOfSurfaceToReflectOffOf.x + y * normalOfSurfaceToReflectOffOf.y;
    Vec2  V_n_normal = normalizedNormalOfSurfaceToReflectOffOf * V_n;

    Vec2 V_t       = *this - V_n_normal;
    Vec2 reflected = V_t + (-V_n_normal);
    return reflected;
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

void Vec2::SetOrientationRadians(float newOrientationRadians)
{
    Vec2 newVec = MakeFromPolarRadians(newOrientationRadians, this->GetLength());
    this->x     = newVec.x;
    this->y     = newVec.y;
}

void Vec2::SetOrientationDegrees(float newOrientationDegrees)
{
    Vec2 newVec = MakeFromPolarDegrees(newOrientationDegrees, this->GetLength());
    this->x     = newVec.x;
    this->y     = newVec.y;
}

void Vec2::SetPolarRadians(float newOrientationRadians, float newLength)
{
    Vec2 newVec = MakeFromPolarRadians(newOrientationRadians, newLength);
    this->x     = newVec.x;
    this->y     = newVec.y;
}

void Vec2::SetPolarDegrees(float newOrientationDegrees, float newLength)
{
    Vec2 newVec = MakeFromPolarDegrees(newOrientationDegrees, newLength);
    this->x     = newVec.x;
    this->y     = newVec.y;
}

void Vec2::Rotate90Degrees()
{
    float temp_x = this->x;
    this->x      = -y;
    this->y      = temp_x;
}

void Vec2::RotateMinus90Degrees()
{
    float temp_x = this->x;
    this->x      = y;
    this->y      = -temp_x;
}

void Vec2::RotateRadians(float deltaRadians)
{
    TransformPosition2D(*this, 1.0f, ConvertRadiansToDegrees(deltaRadians), Vec2(0, 0));
}

void Vec2::RotateDegrees(float deltaDegrees)
{
    TransformPosition2D(*this, 1.0f, deltaDegrees, Vec2(0, 0));
}


void Vec2::SetLength(float newLength)
{
    Normalize();
    x *= newLength;
    y *= newLength;
}

void Vec2::ClampLength(float maxLength)
{
    if (GetLength() > maxLength)
        SetLength(maxLength);
}

void Vec2::Normalize()
{
    float length = GetLength();
    if (length == 0.f)
        return;
    float scale = 1.f / length;
    this->x *= scale;
    this->y *= scale;
}

float Vec2::NormalizeAndGetPreviousLength()
{
    float length = GetLength();
    Normalize();
    return length;
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

void Vec2::SetFromText(const char* text)
{
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
    x = static_cast<float>(atof(parts[0].c_str()));
    y = static_cast<float>(atof(parts[1].c_str()));
}

std::string Vec2::toString() const
{
    return std::to_string(x) + "," + std::to_string(y);
}

//-----------------------------------------------------------------------------------------------
const Vec2 operator*(float uniformScale, const Vec2& vecToScale)
{
    return Vec2(uniformScale * vecToScale.x, uniformScale * vecToScale.y);
}


void Vec2::Reflect(const Vec2& normalOfSurfaceToReflectOffOf)
{
    Vec2 reflectedVector = GetReflected(normalOfSurfaceToReflectOffOf);
    this->x              = reflectedVector.x;
    this->y              = reflectedVector.y;
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
