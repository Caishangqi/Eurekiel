// ============================================================================
// ShadowCamera.cpp - [NEW] Shadow camera implementation
// ============================================================================

#include "ShadowCamera.hpp"

#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // [NEW] Constructor
    // ========================================================================

    ShadowCamera::ShadowCamera(const Vec3& lightPosition, const EulerAngles& lightDirection,
                               const Vec2& shadowBoundsMin, const Vec2&      shadowBoundsMax,
                               float       nearPlane, float                  farPlane)
        : OrthographicCamera(lightPosition, lightDirection, shadowBoundsMin, shadowBoundsMax, nearPlane, farPlane)
          , m_cascadeIndex(0)
    {
    }

    // ========================================================================
    // [NEW] ICamera Implementation
    // ========================================================================

    CameraType ShadowCamera::GetCameraType() const
    {
        return CameraType::Shadow;
    }

    void ShadowCamera::UpdateMatrixUniforms(MatricesUniforms& uniforms) const
    {
        // [NEW] Calculate shadow matrices
        Mat44 view          = GetViewMatrix();
        Mat44 proj          = GetProjectionMatrix();
        Mat44 cameraToWorld = GetCameraToWorldTransform();

        // [NEW] Fill ONLY shadow matrices (not gbuffer matrices)
        uniforms.shadowModelView         = view;
        uniforms.shadowModelViewInverse  = cameraToWorld;
        uniforms.shadowProjection        = proj;
        uniforms.shadowProjectionInverse = proj.GetOrthonormalInverse();
        uniforms.gbufferRenderer         = m_rendererCanonicalMatrix;
    }

    MatricesUniforms ShadowCamera::GetMatrixUniforms()
    {
        MatricesUniforms uniforms;
        UpdateMatrixUniforms(uniforms);
        return uniforms;
    }
} // namespace enigma::graphic
