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
    // [NEW] OrthographicCamera - UI and 2D rendering camera
    // ========================================================================

    /**
     * @brief Orthographic camera for UI rendering and 2D scenes
     * 
     * [NEW] Implements orthographic projection with:
     * - Bottom-left and top-right bounds defining view area
     * - Near/far clipping planes (inherited from CameraBase)
     * - Static factory CreateUI2D() for common UI camera setup
     * 
     * Note: Class is virtual (not final) to allow ShadowCamera inheritance.
     * 
     * Usage:
     * ```cpp
     * // UI camera covering screen
     * auto uiCamera = OrthographicCamera::CreateUI2D(Vec2(1920, 1080));
     * 
     * // Custom orthographic camera
     * auto camera = std::make_unique<OrthographicCamera>(
     *     Vec3::ZERO, EulerAngles(), Vec2(-10, -10), Vec2(10, 10), 0.1f, 100.0f);
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
        // [NEW] Static Factory Methods
        // ====================================================================

        /**
         * @brief Create UI camera for 2D rendering
         * @param screenSize Screen dimensions (width, height)
         * @return OrthographicCamera configured for UI rendering
         * 
         * Creates camera at origin with identity orientation,
         * view bounds from (0,0) to screenSize.
         */
        [[nodiscard]] static OrthographicCamera CreateUI2D(const Vec2& screenSize);

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
