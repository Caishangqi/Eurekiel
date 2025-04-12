#include "Capsule2.hpp"

#include "Engine/Core/EngineCommon.hpp"

Capsule2::Capsule2()
{
}

Capsule2::Capsule2(const Vec2& start, const Vec2& end, float radius): m_start(start), m_end(end), m_radius(radius)
{
}

void Capsule2::Translate(const Vec2& translation)
{
    UNUSED(translation)
}

void Capsule2::SetCenter(const Vec2& newCenter)
{
    UNUSED(newCenter)
}

void Capsule2::RotateAboutCenter(const Vec2& rotationDeltaDegrees)
{
    UNUSED(rotationDeltaDegrees)
}
