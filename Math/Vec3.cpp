#include "Vec3.hpp"
#include <valarray>
#include "MathUtils.hpp"
#include "Vec2.hpp"

Vec3::Vec3(float initialX, float initialY, float initialZ): x(initialX), y(initialY), z(initialZ)
{
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

Vec3 const Vec3::GetRotatedAboutZRadians(float deltaRadians) const
{
    // 计算三维向量在X-Y平面上的投影与X轴之间的角度，即相对于Z轴的旋转角度
    Vec3 vec_ptr = *this;
    TransformPositionXY3D(vec_ptr, 1.0f, ConvertRadiansToDegrees(deltaRadians), Vec2(0, 0));
    return vec_ptr;
}

Vec3 const Vec3::GetRotatedAboutZDegrees(float deltaDegrees) const
{
    Vec3 vec_ptr = *this;
    TransformPositionXY3D(vec_ptr, 1.0f, deltaDegrees, Vec2(0, 0));
    return vec_ptr;
}

Vec3 const Vec3::GetClamped(float maxLength) const
{
    float length = GetLength();
    if (length > maxLength)
    {
        Vec3 vec_normalized = GetNormalized();
        float newX = vec_normalized.x * length;
        float newY = vec_normalized.y * length;
        float newZ = vec_normalized.z * length;
        return Vec3(newX, newY, newZ);
    }
    return *this;
}

Vec3 const Vec3::GetNormalized() const
{
    float length = GetLength();
    float scale = 1.0f / length;
    return Vec3(x * scale, y * scale, z * scale);
}

bool Vec3::operator==(Vec3 const& compare) const
{
    return x == compare.x && y == compare.y && z == compare.z;
}

bool Vec3::operator!=(Vec3 const& compare) const
{
    return x != compare.x || y != compare.y || z != compare.z;
}

Vec3 const Vec3::operator+(Vec3 const& vecToAdd) const
{
    return Vec3(x + vecToAdd.x, y + vecToAdd.y, z + vecToAdd.z);
}

Vec3 const Vec3::operator-(Vec3 const& vecToSubtract) const
{
    return Vec3(x - vecToSubtract.x, y - vecToSubtract.y, z - vecToSubtract.z);
}

Vec3 const Vec3::operator*(float uniformScale) const
{
    return Vec3(x * uniformScale, y * uniformScale, z * uniformScale);
}

Vec3 const Vec3::operator/(float inverseScale) const
{
    return Vec3(x / inverseScale, y / inverseScale, z / inverseScale);
}

void Vec3::operator+=(Vec3 const& vecToAdd)
{
    x += vecToAdd.x;
    y += vecToAdd.y;
    z += vecToAdd.z;
}

void Vec3::operator-=(Vec3 const& vecToSubtract)
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

void Vec3::operator=(Vec3 const& copyFrom)
{
    this->x = copyFrom.x;
    this->y = copyFrom.y;
    this->z = copyFrom.z;
}

Vec3 const operator*(float uniformScale, Vec3 const& vecToScale)
{
    return Vec3(uniformScale * vecToScale.x, uniformScale * vecToScale.y, uniformScale * vecToScale.z);
}
