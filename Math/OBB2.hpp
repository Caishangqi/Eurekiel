#pragma once
#include "Vec2.hpp"

class OBB2
{
public:
    Vec2 m_center;
    Vec2 m_iBasisNormal;
    Vec2 m_halfDimensions;

    OBB2();
    explicit OBB2(const Vec2& center, const Vec2& iBasisNormal, const Vec2& halfDimensions);

    void       GetCornerPoints(Vec2* out_fourCornerWorldPositions) const; // for drawing
    const Vec2 GetLocalPosForWorldPos(const Vec2& worldPosition) const;
    const Vec2 GetWorldPosForLocalPos(const Vec2& localPosition) const;
    void       RotateAboutCenter(float rotationDeltaDegrees);
};
