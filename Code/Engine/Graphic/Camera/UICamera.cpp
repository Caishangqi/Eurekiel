// ============================================================================
// UICamera.cpp - [NEW] UI/2D rendering camera implementation
// ============================================================================

#include "UICamera.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // Lifecycle
    // ========================================================================

    UICamera::UICamera(const Vec2& screenSize, float nearPlane, float farPlane)
        : OrthographicCamera(Vec3::ZERO, EulerAngles(), Vec2(0.0f, 0.0f), screenSize, nearPlane, farPlane)
    {
        // [NEW] UI camera uses identity renderer matrix (no 3D coordinate conversion)
        // Y-axis flip is handled in GetProjectionMatrix() instead
        m_rendererCanonicalMatrix = Mat44();
    }

    // ========================================================================
    // Static Factory Methods
    // ========================================================================

    UICamera UICamera::Create(const Vec2& screenSize)
    {
        return UICamera(screenSize, 0.0f, 1.0f);
    }

    // ========================================================================
    // ICamera Implementation
    // ========================================================================

    Mat44 UICamera::GetProjectionMatrix() const
    {
        // [NEW] Orthographic projection with Y-axis flip for DirectX
        Vec2 size   = m_topRight - m_bottomLeft;
        Vec2 center = (m_topRight + m_bottomLeft) * 0.5f;

        // [IMPORTANT] Flip Y axis for DirectX texture coordinate system (top-left origin)
        // OpenGL: bottom = -size.y/2, top = +size.y/2
        // DirectX: bottom = +size.y/2, top = -size.y/2 (flipped)
        Mat44 projection = Mat44::MakeOrthoProjection(
            -size.x * 0.5f,
            size.x * 0.5f,
            size.y * 0.5f, // [UI] Swapped for Y-flip
            -size.y * 0.5f, // [UI] Swapped for Y-flip
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

    CameraType UICamera::GetCameraType() const
    {
        return CameraType::UI;
    }

    // ========================================================================
    // UI-specific Methods
    // ========================================================================

    void UICamera::SetScreenSize(const Vec2& screenSize)
    {
        m_bottomLeft = Vec2(0.0f, 0.0f);
        m_topRight   = screenSize;
    }
} // namespace enigma::graphic
