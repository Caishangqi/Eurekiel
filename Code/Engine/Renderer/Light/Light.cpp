#include "Light.hpp"

#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/MathUtils.hpp"

Light& Light::SetPosition(Vec3 position)
{
    m_position = position;
    return *this;
}

Light& Light::SetDirection(Vec3 direction)
{
    m_direction = direction.GetNormalized();
    return *this;
}

Light& Light::SetColor(Vec4 color)
{
    m_color = color;
    return *this;
}

Light& Light::SetColor(Rgba8 color)
{
    float colorInFloat[4];
    color.GetAsFloats(colorInFloat);
    m_color = Vec4(colorInFloat[0], colorInFloat[1], colorInFloat[2], colorInFloat[3]);
    return *this;
}

Light& Light::SetInnerRadius(float radius)
{
    m_innerRadius = radius;
    return *this;
}

Light& Light::SetOuterRadius(float radius)
{
    m_outerRadius = radius;
    return *this;
}

Light& Light::SetInnerAngle(float angle)
{
    m_innerPenumbra = CosDegrees(angle);
    return *this;
}

Light& Light::SetOuterAngle(float angle)
{
    m_outerPenumbra = CosDegrees(angle);
    return *this;
}
