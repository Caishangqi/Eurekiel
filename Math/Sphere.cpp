#include "Sphere.hpp"

#include "MathUtils.hpp"
#include "Plane3.hpp"

Sphere::Sphere()
{
}

Sphere::Sphere(const Vec3& position, float radius): m_position(position), m_radius(radius)
{
}

bool Sphere::IsOverlapping(const Plane3& other) const
{
    return IsOverlapping(*this, other);
}

bool Sphere::IsOverlapping(const Sphere& sphere, const Plane3& other)
{
    float signedDistance = DotProduct3D(other.m_normal, sphere.m_position) - other.m_distToPlaneAlongNormalFromOrigin;
    if (fabs(signedDistance) > sphere.m_radius)
        return false;
    return true;
}
