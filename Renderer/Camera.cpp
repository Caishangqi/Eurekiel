#include "Camera.hpp"


// (200,100)
void Camera::SetOrthoView(const Vec2& bottomLeft, const Vec2& topRight)
{
    this->m_bottomLeft = bottomLeft;
    this->m_topRight = topRight;
}

Vec2 Camera::GetOrthoBottomLeft() const
{
    return m_bottomLeft;
}

Vec2 Camera::GetOrthoTopRight() const
{
    return m_topRight;
}
