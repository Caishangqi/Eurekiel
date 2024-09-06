#pragma once
#include "Engine/Math/Vec2.hpp"

class Camera
{
public:
    void SetOrthoView(const Vec2& bottomLeft, const Vec2& topRight);
    Vec2 GetOrthoBottomLeft() const;
    Vec2 GetOrthoTopRight() const;

protected:
    Vec2 m_bottomLeft;
    Vec2 m_topRight;
};
