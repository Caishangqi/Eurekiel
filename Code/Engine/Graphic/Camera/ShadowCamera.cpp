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
        Mat44 gbufferRenderer;
        gbufferRenderer.SetIJK3D(Vec3(0, 0, 1), Vec3(-1, 0, 0), Vec3(0, 1, 0));
        m_rendererCanonicalMatrix = gbufferRenderer;
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
        uniforms.shadowView              = view;
        uniforms.shadowViewInverse       = cameraToWorld;
        uniforms.shadowProjection        = proj;
        uniforms.shadowProjectionInverse = proj.GetInverse();
        uniforms.gbufferRenderer         = m_rendererCanonicalMatrix;
        uniforms.gbufferRendererInverse  = m_rendererCanonicalMatrix.GetOrthonormalInverse();
    }

    MatricesUniforms ShadowCamera::GetMatrixUniforms()
    {
        MatricesUniforms uniforms;
        UpdateMatrixUniforms(uniforms);
        return uniforms;
    }
} // namespace enigma::graphic
