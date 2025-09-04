#include "OBB2.hpp"

#include "Engine/Core/EngineCommon.hpp"

OBB2::OBB2()
{
}

OBB2::OBB2(const Vec2& center, const Vec2& iBasisNormal, const Vec2& halfDimensions) : m_center(center),
                                                                                       m_iBasisNormal(iBasisNormal), m_halfDimensions(halfDimensions)
{
}

void OBB2::GetCornerPoints(Vec2* out_fourCornerWorldPositions) const
{
    UNUSED(out_fourCornerWorldPositions);
}

const Vec2 OBB2::GetLocalPosForWorldPos(const Vec2& worldPosition) const
{
    UNUSED(worldPosition)
    return Vec2();
}

const Vec2 OBB2::GetWorldPosForLocalPos(const Vec2& localPosition) const
{
    UNUSED(localPosition)
    return Vec2();
}

void OBB2::RotateAboutCenter(float rotationDeltaDegrees)
{
    UNUSED(rotationDeltaDegrees)
}
