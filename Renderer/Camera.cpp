#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/gl.h>
#include "Camera.hpp"


// (200,100)
void Camera::SetOrthoView(Vec2 const& bottomLeft, Vec2 const& topRight)
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
