#include "Vec3.hpp"

Vec3::Vec3(float initialX, float initialY, float initialZ): x(initialX), y(initialY), z(initialZ)
{
}

Vec3::Vec3(const Vec3& copyFrom)
{
    this->x = copyFrom.x;
    this->y = copyFrom.y;
}
