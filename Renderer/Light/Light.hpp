#pragma once
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec4.hpp"

struct Rgba8;


class Light
{
public:
    Light& SetPosition(Vec3 position);
    Light& SetDirection(Vec3 direction);
    Light& SetColor(Vec4 color);
    Light& SetColor(Rgba8 color);
    Light& SetInnerRadius(float radius);
    Light& SetOuterRadius(float radius);
    Light& SetInnerAngle(float angle);
    Light& SetOuterAngle(float angle);

private:
    Vec3 m_position = Vec3::ZERO;
    [[maybe_unused]]
    float m_pad0      = 0.0f;
    Vec3  m_direction = Vec3::ZERO;
    [[maybe_unused]]
    float m_pad1 = 0.0f;
    Vec4  m_color;
    float m_innerPenumbra{0.f};
    float m_outerPenumbra{0.f};
    float m_innerRadius{0.f};
    float m_outerRadius{0.f};
};
