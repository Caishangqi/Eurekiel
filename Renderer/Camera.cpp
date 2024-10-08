#include "Camera.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Game/GameCommon.hpp"

void Camera::SetOrthoView(const Vec2& bottomLeft, const Vec2& topRight)
{
    m_bottomLeft = bottomLeft;
    m_topRight   = topRight;
}

Vec2 Camera::GetOrthoBottomLeft() const
{
    return m_isPostProcessing ? m_postBottomLeft : m_bottomLeft;
}

Vec2 Camera::GetOrthoTopRight() const
{
    return m_isPostProcessing ? m_postTopRight : m_topRight;
}

void Camera::Update(float deltaTime)
{
    if (m_isShaking)
    {
        ApplyShakeEffect(deltaTime);
    }
    else
    {
        m_isPostProcessing = false;
    }
}

void Camera::Translate2D(const Vec2& translation2D)
{
    Vec2 shakeOffset = GenerateRandomShakeOffset(translation2D);

    m_postBottomLeft = m_bottomLeft + shakeOffset;
    m_postTopRight   = m_topRight + shakeOffset;
}

void Camera::DoShakeEffect(const Vec2& translation2D, float duration, bool decreaseShakeOverTime)
{
    m_shakeTotalTime        = duration;
    m_shakeRemainingTime    = duration;
    m_shakeTranslation      = translation2D;
    m_isShaking             = true;
    m_isPostProcessing      = true;
    m_decreaseShakeOverTime = decreaseShakeOverTime;
}

void Camera::ApplyShakeEffect(float deltaTime)
{
    m_shakeRemainingTime -= deltaTime;
    Translate2D(m_shakeTranslation);

    if (m_shakeRemainingTime <= 0)
    {
        m_isShaking = false;
    }
}

Vec2 Camera::GenerateRandomShakeOffset(const Vec2& translation2D) const
{
    float randomRateHorizontal;
    float randomRateVertical;

    if (m_decreaseShakeOverTime && m_shakeTotalTime > 0)
    {
        float rate           = m_shakeRemainingTime / m_shakeTotalTime;
        randomRateHorizontal = g_rng->RollRandomFloatInRange(-rate, rate);
        randomRateVertical   = g_rng->RollRandomFloatInRange(-rate, rate);
    }
    else
    {
        randomRateHorizontal = g_rng->RollRandomFloatInRange(-1.0f, 1.0f);
        randomRateVertical   = g_rng->RollRandomFloatInRange(-1.0f, 1.0f);
    }

    return Vec2(randomRateHorizontal * translation2D.x, randomRateVertical * translation2D.y);
}
