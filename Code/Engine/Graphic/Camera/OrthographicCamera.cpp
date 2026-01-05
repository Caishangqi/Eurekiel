#include "OrthographicCamera.hpp"
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // Lifecycle
    // ========================================================================

    OrthographicCamera::OrthographicCamera(const Vec3& position, const EulerAngles& orientation,
                                           const Vec2& bottomLeft, const Vec2&      topRight,
                                           float       nearPlane, float             farPlane)
        : CameraBase(position, orientation, nearPlane, farPlane)
          , m_bottomLeft(bottomLeft)
          , m_topRight(topRight)
    {
    }

    // ========================================================================
    // Static Factory Methods
    // ========================================================================

    OrthographicCamera OrthographicCamera::CreateUI2D(const Vec2& screenSize)
    {
        // [NEW] UI camera: origin position, identity orientation, screen bounds
        return OrthographicCamera(
            Vec3::ZERO,
            EulerAngles(),
            Vec2(0.0f, 0.0f),
            screenSize,
            0.0f,
            1.0f
        );
    }

    // ========================================================================
    // ICamera Implementation
    // ========================================================================

    Mat44 OrthographicCamera::GetProjectionMatrix() const
    {
        // [NEW] Orthographic projection with center offset
        // Extracted from EnigmaCamera.cpp orthographic branch
        Vec2 size   = m_topRight - m_bottomLeft;
        Vec2 center = (m_topRight + m_bottomLeft) * 0.5f;

        Mat44 projection = Mat44::MakeOrthoProjection(
            -size.x * 0.5f,
            size.x * 0.5f,
            -size.y * 0.5f,
            size.y * 0.5f,
            m_nearPlane,
            m_farPlane
        );

        // Apply center offset if not at origin
        if (center.x != 0.0f || center.y != 0.0f)
        {
            Mat44 translation = Mat44::MakeTranslation3D(Vec3(-center.x, -center.y, 0.0f));
            projection.Append(translation);
        }

        return projection;
    }

    CameraType OrthographicCamera::GetCameraType() const
    {
        return CameraType::Orthographic;
    }

    void OrthographicCamera::UpdateMatrixUniforms(MatricesUniforms& uniforms) const
    {
        // [NEW] Fill GBuffer matrices for orthographic rendering
        Mat44 view          = GetViewMatrix();
        Mat44 proj          = GetProjectionMatrix();
        Mat44 cameraToWorld = GetCameraToWorldTransform();

        // GBuffer matrices (main render pass)
        uniforms.gbufferView              = view;
        uniforms.gbufferViewInverse       = cameraToWorld;
        uniforms.gbufferProjection        = proj;
        uniforms.gbufferProjectionInverse = proj.GetOrthonormalInverse();
        uniforms.gbufferRenderer          = m_rendererCanonicalMatrix;
    }

    MatricesUniforms OrthographicCamera::GetMatrixUniforms()
    {
        MatricesUniforms uniforms;
        UpdateMatrixUniforms(uniforms);
        return uniforms;
    }

    // ========================================================================
    // Orthographic-specific Setters
    // ========================================================================

    void OrthographicCamera::SetBounds(const Vec2& bottomLeft, const Vec2& topRight)
    {
        m_bottomLeft = bottomLeft;
        m_topRight   = topRight;
    }
} // namespace enigma::graphic
