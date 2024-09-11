#include "Vec3.hpp"

Vec3::Vec3(float initialX, float initialY, float initialZ): x(initialX), y(initialY), z(initialZ)
{
}

void Vec3::operator=(Vec3 const& copyFrom)
{
    this->x = copyFrom.x;
    this->y = copyFrom.y;
    this->z = copyFrom.z;
}