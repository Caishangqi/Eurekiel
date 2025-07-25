#include <valarray>

#include "EulerAngles.hpp"
#include "Mat44.hpp"
#include "MathUtils.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/StringUtils.hpp"

Vec3 Vec3::ZERO    = Vec3(0.0f, 0.0f, 0.0f);
Vec3 Vec3::ONE     = Vec3(1.0f, 1.0f, 1.0f);
Vec3 Vec3::INVALID = Vec3(-1.0f, -1.0f, -1.0f);

Vec3::Vec3(const Vec3& copyFrom)
{
    x = copyFrom.x;
    y = copyFrom.y;
    z = copyFrom.z;
}

Vec3::Vec3(float initialX, float initialY, float initialZ) : x(initialX), y(initialY), z(initialZ)
{
}

Vec3::Vec3(const EulerAngles& angles)
{
    x = angles.m_yawDegrees;
    y = angles.m_pitchDegrees;
    z = angles.m_rollDegrees;
}

Vec3::Vec3(const char* stringVec)
{
    SetFromText(stringVec);
}

Vec3::Vec3(float length)
{
    x = y = z = length;
}

float Vec3::GetLength() const
{
    return sqrt(x * x + y * y + z * z);
}

float Vec3::GetLengthXY() const
{
    return sqrt(x * x + y * y);
}

float Vec3::GetLengthSquared() const
{
    return x * x + y * y + z * z;
}

float Vec3::GetLengthXYSquared() const
{
    return x * x + y * y;
}

float Vec3::GetAngleAboutZRadians() const
{
    float projectionX = x;
    float projectionY = y;
    return ConvertDegreesToRadians(Atan2Degrees(projectionY, projectionX));
}

float Vec3::GetAngleAboutZDegrees() const
{
    float projectionX = x;
    float projectionY = y;
    return Atan2Degrees(projectionY, projectionX);
}

const Vec3 Vec3::GetRotatedAboutZRadians(float deltaRadians) const
{
    // 计算三维向量在X-Y平面上的投影与X轴之间的角度，即相对于Z轴的旋转角度
    Vec3 vec_ptr = *this;
    TransformPositionXY3D(vec_ptr, 1.0f, ConvertRadiansToDegrees(deltaRadians), Vec2(0, 0));
    return vec_ptr;
}

const Vec3 Vec3::GetRotatedAboutZDegrees(float deltaDegrees) const
{
    Vec3 vec_ptr = *this;
    TransformPositionXY3D(vec_ptr, 1.0f, deltaDegrees, Vec2(0, 0));
    return vec_ptr;
}

const Vec3 Vec3::GetClamped(float maxLength) const
{
    float length = GetLength();
    if (length > maxLength)
    {
        Vec3  vec_normalized = GetNormalized();
        float newX           = vec_normalized.x * length;
        float newY           = vec_normalized.y * length;
        float newZ           = vec_normalized.z * length;
        return Vec3(newX, newY, newZ);
    }
    return *this;
}

const Vec3 Vec3::GetNormalized() const
{
    float length = GetLength();
    float scale  = 1.0f / length;
    return Vec3(x * scale, y * scale, z * scale);
}

const Vec2 Vec3::GetXY() const
{
    return Vec2(x, y);
}

const Vec3 Vec3::MakeFromPolarRadians(float pitchRadians, float yawRadians, float length)
{
    return MakeFromPolarDegrees(ConvertRadiansToDegrees(pitchRadians), ConvertRadiansToDegrees(yawRadians), length);
}

const Vec3 Vec3::MakeFromPolarDegrees(float pitchDegrees, float yawDegrees, float length)
{
    Vec3  result(1, 0.f, 0.f);
    Mat44 mat;
    mat.AppendZRotation(yawDegrees);
    mat.AppendYRotation(pitchDegrees);
    mat.AppendScaleUniform3D(length);
    result = mat.TransformPosition3D(result);
    return result;
}


bool Vec3::operator==(const Vec3& compare) const
{
    return x == compare.x && y == compare.y && z == compare.z;
}

bool Vec3::operator!=(const Vec3& compare) const
{
    return x != compare.x || y != compare.y || z != compare.z;
}

const Vec3 Vec3::operator+(const Vec3& vecToAdd) const
{
    return Vec3(x + vecToAdd.x, y + vecToAdd.y, z + vecToAdd.z);
}

const Vec3 Vec3::operator-() const
{
    return Vec3(-x, -y, -z);
}

const Vec3 Vec3::operator-(const Vec3& vecToSubtract) const
{
    return Vec3(x - vecToSubtract.x, y - vecToSubtract.y, z - vecToSubtract.z);
}

const Vec3 Vec3::operator*(float uniformScale) const
{
    return Vec3(x * uniformScale, y * uniformScale, z * uniformScale);
}

const Vec3 Vec3::operator/(float inverseScale) const
{
    return Vec3(x / inverseScale, y / inverseScale, z / inverseScale);
}


void Vec3::operator+=(const Vec3& vecToAdd)
{
    x += vecToAdd.x;
    y += vecToAdd.y;
    z += vecToAdd.z;
}

void Vec3::operator-=(const Vec3& vecToSubtract)
{
    x -= vecToSubtract.x;
    y -= vecToSubtract.y;
    z -= vecToSubtract.z;
}

void Vec3::operator*=(float uniformScale)
{
    x *= uniformScale;
    y *= uniformScale;
    z *= uniformScale;
}

void Vec3::operator/=(float uniformDivisor)
{
    x /= uniformDivisor;
    y /= uniformDivisor;
    z /= uniformDivisor;
}

void Vec3::operator=(const Vec3& copyFrom)
{
    this->x = copyFrom.x;
    this->y = copyFrom.y;
    this->z = copyFrom.z;
}

void Vec3::SetFromText(const char* text)
{
    if (text == nullptr)
        return;
    // Split the input text on the comma delimiter
    Strings parts = SplitStringOnDelimiter(text, ',');

    if (parts.size() != 3)
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
}


bool operator<(const Vec3& lhs, const Vec3& rhs)
{
    if (lhs.x < rhs.x)
        return true;
    if (rhs.x < lhs.x)
        return false;
    if (lhs.y < rhs.y)
        return true;
    if (rhs.y < lhs.y)
        return false;
    return lhs.z < rhs.z;
}

bool operator<=(const Vec3& lhs, const Vec3& rhs)
{
    return !(rhs < lhs);
}

bool operator>(const Vec3& lhs, const Vec3& rhs)
{
    return rhs < lhs;
}

bool operator>=(const Vec3& lhs, const Vec3& rhs)
{
    return !(lhs < rhs);
}

const Vec3 operator*(float uniformScale, const Vec3& vecToScale)
{
    return Vec3(uniformScale * vecToScale.x, uniformScale * vecToScale.y, uniformScale * vecToScale.z);
}
