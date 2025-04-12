#include "Camera.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"

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

void Camera::SetOrthographicView(Vec2 const& bottomLeft, Vec2 const& topRight, float near, float far)
{
    m_orthographicBottomLeft = bottomLeft;
    m_orthographicTopRight   = topRight;
    m_orthographicNear       = near;
    m_orthographicFar        = far;
}

void Camera::SetPerspectiveView(float aspect, float fov, float near, float far)
{
    m_perspectiveAspect = aspect;
    m_perspectiveNear   = near;
    m_perspectiveFar    = far;
    m_perspectiveFOV    = fov;
}

void Camera::SetPositionAndOrientation(const Vec3& position, const EulerAngles& orientation)
{
    m_position    = position;
    m_orientation = orientation;
}

void Camera::SetPosition(const Vec3& position)
{
    m_position = position;
}

Vec3 Camera::GetPosition() const
{
    return m_position;
}

void Camera::SetOrientation(const EulerAngles& orientation)
{
    m_orientation = orientation;
}

EulerAngles Camera::GetOrientation() const
{
    return m_orientation;
}

Mat44 Camera::GetCameraToWorldTransform() const
{
    Mat44 cameraToWorld = Mat44::MakeTranslation3D(m_position);
    cameraToWorld.Append(Mat44::MakeZRotationDegrees(m_orientation.m_yawDegrees));
    cameraToWorld.Append(Mat44::MakeXRotationDegrees(m_orientation.m_rollDegrees));
    cameraToWorld.Append(Mat44::MakeYRotationDegrees(m_orientation.m_pitchDegrees));
    return cameraToWorld;
}

Mat44 Camera::GetWorldToCameraTransform() const
{
    Mat44 cameraToWorld = GetCameraToWorldTransform();
    return cameraToWorld.GetOrthonormalInverse();
}

void Camera::SetCameraToRenderTransform(const Mat44& m)
{
    m_cameraToRenderTransform = m;
}

Mat44 Camera::GetCameraToRenderTransform() const
{
    return m_cameraToRenderTransform;
}

Mat44 Camera::GetRenderToClipTransform() const
{
    return GetPerspectiveMatrix();
}

Vec2 Camera::GetOrthographicBottomLeft() const
{
    return m_orthographicBottomLeft;
}

Vec2 Camera::GetOrthographicTopRight() const
{
    return m_orthographicTopRight;
}

Mat44 Camera::GetOrthographicMatrix() const
{
    return Mat44::MakeOrthoProjection(m_orthographicBottomLeft.x, m_orthographicTopRight.x, m_orthographicBottomLeft.y, m_orthographicTopRight.y, m_orthographicNear, m_orthographicFar);
}

Mat44 Camera::GetPerspectiveMatrix() const
{
    return Mat44::MakePerspectiveProjection(m_perspectiveFOV, m_perspectiveAspect, m_perspectiveNear, m_perspectiveFar);
}

Mat44 Camera::GetProjectionMatrix() const
{
    Mat44 projectMatrix;
    switch (m_mode)
    {
    case eMode_Orthographic:
        projectMatrix = GetOrthographicMatrix();
        return projectMatrix;
    case eMode_Perspective:
        projectMatrix = GetPerspectiveMatrix();
        return projectMatrix;
    case eMode_Count:
        ERROR_AND_DIE("Camera::SetPerspectiveMatrix: eMode_Count is not supported")
    }
    return projectMatrix;
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
        randomRateHorizontal = RandomNumberGenerator::__RollRandomFloatInRange(-rate, rate);
        randomRateVertical   = RandomNumberGenerator::__RollRandomFloatInRange(-rate, rate);
    }
    else
    {
        randomRateHorizontal = RandomNumberGenerator::__RollRandomFloatInRange(-1.0f, 1.0f);
        randomRateVertical   = RandomNumberGenerator::__RollRandomFloatInRange(-1.0f, 1.0f);
    }

    return Vec2(randomRateHorizontal * translation2D.x, randomRateVertical * translation2D.y);
}
