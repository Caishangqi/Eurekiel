#pragma once
#include "Vec3.hpp"

class ZCylinder
{
public:
    ZCylinder();
    ~ZCylinder();

    ZCylinder(const ZCylinder& copyFrom);
    explicit ZCylinder(const Vec3& center, float radius, float height);
    ZCylinder(const Vec3& centerOrBase, float radius, float height, bool isBasePosition);

    Vec3  m_center = Vec3::ZERO;
    float m_height = 0.f;
    float m_radius = 0.f;
};
