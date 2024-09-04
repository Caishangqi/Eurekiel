#pragma once
#include "Engine/Math/Vec2.hpp"

class Camera
{
public:
    void SetOrthoView(Vec2 const& bottomLeft, Vec2 const& topRight);
    Vec2 GetOrthoBottomLeft() const;
    Vec2 GetOrthoTopRight() const;
};
