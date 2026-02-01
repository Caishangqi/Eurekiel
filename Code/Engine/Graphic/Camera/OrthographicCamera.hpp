#pragma once

// ============================================================================
// OrthographicCamera.hpp - [NEW] Orthographic camera for UI and 2D rendering
// 
// Implements orthographic projection for UI rendering and 2D scenes.
// Base class for ShadowCamera (virtual, not final).
// ============================================================================

#include "CameraBase.hpp"

#include "Engine/Math/Vec2.hpp"

namespace enigma::graphic
{
    // Forward declaration
    struct MatricesUniforms;

    // ========================================================================
    // [NEW] OrthographicCamera - Parallel projection camera
    // ========================================================================

    /**
     * @brief Orthographic camera for 2D/3D scenes with parallel projection
     *
     * [NEW] Implements orthographic projection with:
     * - Bottom-left and top-right bounds defining view area
     * - Near/far clipping planes (inherited from CameraBase)
     *
     * Note: Class is virtual (not final) to allow UICamera/ShadowCamera inheritance.
     *
     * Coordinate System:
     * - Standard mathematical coordinates (Y increases upward)
     * - Center can be offset via bounds configuration
     *
     * When to use OrthographicCamera:
     * - 3D scenes requiring parallel projection (isometric views, CAD)
     * - 2D games with center-origin coordinate system (math/physics style)
     * - Any scenario where Y-up is preferred
     *
     * For UI rendering with top-left origin, use UICamera instead.
     *
     * m_rendererCanonicalMatrix: Identity (no 3D coordinate conversion)
     * - Avoids Z=0 compression issue in 2D rendering
     *
     * Usage:
     * ```cpp
     * // Center-origin orthographic camera (Y-up)
     * auto camera = std::make_unique<OrthographicCamera>(
     *     Vec3::ZERO, EulerAngles(),
     *     Vec2(-960, -540),  // bottomLeft
     *     Vec2(960, 540),    // topRight
     *     0.1f, 100.0f);
     *
     * // Position (100, 200) means:
     * // - 100 units right of center
     * // - 200 units above center
     *
     * // For UI rendering, use UICamera::Create()
     * auto uiCamera = UICamera::Create(Vec2(1920, 1080));
     * ```
     */
    class OrthographicCamera : public CameraBase
    {
    public:
        // ====================================================================
        // Lifecycle
        // ====================================================================

        /**
         * @brief Construct orthographic camera
         * @param position Camera world position
         * @param orientation Camera rotation (yaw, pitch, roll)
         * @param bottomLeft Bottom-left corner of view bounds
         * @param topRight Top-right corner of view bounds
         * @param nearPlane Near clipping plane distance
         * @param farPlane Far clipping plane distance
         */
        OrthographicCamera(const Vec3& position, const EulerAngles& orientation,
                           const Vec2& bottomLeft, const Vec2&      topRight,
                           float       nearPlane = 0.1f, float      farPlane = 1000.0f);

        ~OrthographicCamera() override = default;

        // Move semantics
        OrthographicCamera(OrthographicCamera&&)            = default;
        OrthographicCamera& operator=(OrthographicCamera&&) = default;

        // ====================================================================
        // ICamera Implementation
        // ====================================================================

        /**
         * @brief Get orthographic projection matrix
         * @return Orthographic projection matrix (view-to-clip)
         */
        [[nodiscard]] Mat44 GetProjectionMatrix() const override;

        /**
         * @brief Get camera type
         * @return CameraType::Orthographic
         */
        [[nodiscard]] CameraType GetCameraType() const override;

        /**
         * @brief Fill GBuffer matrices in MatricesUniforms
         * @param uniforms Reference to uniforms struct for GPU upload
         */
        void             UpdateMatrixUniforms(MatricesUniforms& uniforms) const override;
        MatricesUniforms GetMatrixUniforms() override;

        // ====================================================================
        // [NEW] Orthographic-specific Setters
        // ====================================================================

        void SetBounds(const Vec2& bottomLeft, const Vec2& topRight);

        // ====================================================================
        // [NEW] Orthographic-specific Getters
        // ====================================================================

        [[nodiscard]] const Vec2& GetBottomLeft() const { return m_bottomLeft; }
        [[nodiscard]] const Vec2& GetTopRight() const { return m_topRight; }

    protected:
        // ====================================================================
        // [NEW] Protected Members (accessible by ShadowCamera)
        // ====================================================================

        Vec2 m_bottomLeft = Vec2(-1.0f, -1.0f);
        Vec2 m_topRight   = Vec2(1.0f, 1.0f);
    };
} // namespace enigma::graphic
