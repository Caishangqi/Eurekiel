#include "PerspectiveCamera.hpp"
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

#include <stdexcept>

namespace enigma::graphic
{
    // ========================================================================
    // Lifecycle
    // ========================================================================

    PerspectiveCamera::PerspectiveCamera(const Vec3& position, const EulerAngles& orientation,
                                         float       fov, float                   aspectRatio,
                                         float       nearPlane, float             farPlane)
        : CameraBase(position, orientation, nearPlane, farPlane)
          , m_fov(fov)
          , m_aspectRatio(aspectRatio)
    {
        ValidateFOV(fov);
        ValidateAspectRatio(aspectRatio);
    }

    // ========================================================================
    // ICamera Implementation
    // ========================================================================

    Mat44 PerspectiveCamera::GetProjectionMatrix() const
    {
        // [NEW] Perspective projection using FOV and aspect ratio
        return Mat44::MakePerspectiveProjection(m_fov, m_aspectRatio, m_nearPlane, m_farPlane);
    }

    CameraType PerspectiveCamera::GetCameraType() const
    {
        return CameraType::Perspective;
    }

    void PerspectiveCamera::UpdateMatrixUniforms(MatricesUniforms& uniforms) const
    {
        // [NEW] Fill GBuffer matrices for deferred rendering
        Mat44 view          = GetViewMatrix();
        Mat44 proj          = GetProjectionMatrix();
        Mat44 cameraToWorld = GetCameraToWorldTransform();

        // GBuffer matrices (main render pass)
        uniforms.gbufferView              = view;
        uniforms.gbufferViewInverse       = cameraToWorld;
        uniforms.gbufferProjection        = proj;
        uniforms.gbufferProjectionInverse = proj.GetInverse();
        uniforms.gbufferRenderer          = m_rendererCanonicalMatrix;
        uniforms.gbufferRendererInverse   = m_rendererCanonicalMatrix.GetOrthonormalInverse();
    }

    MatricesUniforms PerspectiveCamera::GetMatrixUniforms()
    {
        MatricesUniforms uniforms;
        UpdateMatrixUniforms(uniforms);
        return uniforms;
    }

    // ========================================================================
    // Perspective-specific Setters
    // ========================================================================

    void PerspectiveCamera::SetFOV(float fov)
    {
        ValidateFOV(fov);
        m_fov = fov;
    }

    void PerspectiveCamera::SetAspectRatio(float aspectRatio)
    {
        ValidateAspectRatio(aspectRatio);
        m_aspectRatio = aspectRatio;
    }

    // ========================================================================
    // Validation
    // ========================================================================

    void PerspectiveCamera::ValidateFOV(float fov)
    {
        if (fov <= 0.0f || fov >= 180.0f)
        {
            throw InvalidCameraParameterException("FOV must be in range (0, 180) degrees");
        }
    }

    void PerspectiveCamera::ValidateAspectRatio(float aspectRatio)
    {
        if (aspectRatio <= 0.0f)
        {
            throw InvalidCameraParameterException("Aspect ratio must be positive");
        }
    }
} // namespace enigma::graphic
