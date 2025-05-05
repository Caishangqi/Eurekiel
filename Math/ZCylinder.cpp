#include "ZCylinder.hpp"

ZCylinder::ZCylinder()
{
}

ZCylinder::~ZCylinder()
{
}

ZCylinder::ZCylinder(const ZCylinder& copyFrom)
{
    m_center = copyFrom.m_center;
    m_height = copyFrom.m_height;
    m_radius = copyFrom.m_radius;
}

ZCylinder::ZCylinder(const Vec3& center, float radius, float height): m_center(center), m_height(height), m_radius(radius)
{
}

ZCylinder::ZCylinder(const Vec3& centerOrBase, float radius, float height, bool isBasePosition)
    : m_height(height), m_radius(radius)
{
    if (isBasePosition)
    {
        m_center = centerOrBase + Vec3(0.f, 0.f, height / 2.0f);
    }
    else
    {
        // 否则直接认为传入的就是中心位置
        m_center = centerOrBase;
    }
}
