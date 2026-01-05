#include "CameraBase.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // Lifecycle
    // ========================================================================

    CameraBase::CameraBase(const Vec3& position, const EulerAngles& orientation,
                           float       nearPlane, float             farPlane)
        : m_position(position)
          , m_orientation(orientation)
          , m_nearPlane(nearPlane)
          , m_farPlane(farPlane)

    {
        Mat44 gbufferRenderer;
        gbufferRenderer.SetIJK3D(Vec3(0, 0, 1), Vec3(-1, 0, 0), Vec3(0, 1, 0));
        m_rendererCanonicalMatrix = gbufferRenderer;
    }

    // ========================================================================
    // ICamera Implementation
    // ========================================================================

    Mat44 CameraBase::GetViewMatrix() const
    {
        return CalculateViewMatrix();
    }

    Mat44 CameraBase::GetRendererCanonicalMatrix() const
    {
        return m_rendererCanonicalMatrix;
    }

    // ========================================================================
    // [NEW] Public Setters
    // ========================================================================

    void CameraBase::SetPosition(const Vec3& position)
    {
        m_position = position;
    }

    void CameraBase::SetOrientation(const EulerAngles& orientation)
    {
        m_orientation = orientation;
    }

    void CameraBase::SetPositionAndOrientation(const Vec3& position, const EulerAngles& orientation)
    {
        m_position    = position;
        m_orientation = orientation;
    }

    void CameraBase::SetNearFar(float nearPlane, float farPlane)
    {
        m_nearPlane = nearPlane;
        m_farPlane  = farPlane;
    }

    // ========================================================================
    // [REFACTOR] Protected Helpers - Extracted from EnigmaCamera.cpp
    // ========================================================================

    Mat44 CameraBase::GetCameraToWorldTransform() const
    {
        // [REFACTOR] Rotation order: Translation * Z(yaw) * X(roll) * Y(pitch)
        // Source: EnigmaCamera.cpp lines 27-35
        Mat44 result = Mat44::MakeTranslation3D(m_position);
        result.Append(Mat44::MakeZRotationDegrees(m_orientation.m_yawDegrees));
        result.Append(Mat44::MakeXRotationDegrees(m_orientation.m_rollDegrees));
        result.Append(Mat44::MakeYRotationDegrees(m_orientation.m_pitchDegrees));
        return result;
    }

    Mat44 CameraBase::CalculateViewMatrix() const
    {
        // View matrix = inverse of camera-to-world transform
        return GetCameraToWorldTransform().GetInverse();
    }
} // namespace enigma::graphic
