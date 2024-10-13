#pragma once
#include "Engine/Math/Vec2.hpp"

class Camera
{
public:
    void SetOrthoView(const Vec2& bottomLeft, const Vec2& topRight);
    Vec2 GetOrthoBottomLeft() const;
    Vec2 GetOrthoTopRight() const;

    void Update(float deltaTime);
    void Translate2D(const Vec2& translation2D);
    void DoShakeEffect(const Vec2& translation2D, float duration, bool decreaseShakeOverTime = true);

private:
    void ApplyShakeEffect(float deltaTime);
    Vec2 GenerateRandomShakeOffset(const Vec2& translation2D) const;

    Vec2 m_bottomLeft;
    Vec2 m_topRight;

    // Post-processing
    Vec2 m_postBottomLeft;
    Vec2 m_postTopRight;
    bool m_isPostProcessing = false;

    // Shake effect
    float m_shakeTotalTime     = 0.f;
    float m_shakeRemainingTime = 0.f;
    bool  m_isShaking          = false;
    Vec2  m_shakeTranslation;
    bool  m_decreaseShakeOverTime = false;
};
