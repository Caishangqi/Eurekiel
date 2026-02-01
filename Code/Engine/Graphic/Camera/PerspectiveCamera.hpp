#pragma once

// ============================================================================
// PerspectiveCamera.hpp - [NEW] Perspective camera for GBuffer rendering
// 
// Implements perspective projection for main render pass (deferred rendering).
// Fills gbuffer matrices in MatricesUniforms.
// ============================================================================

#include "CameraBase.hpp"

namespace enigma::graphic
{
    // Forward declaration
    struct MatricesUniforms;

    // ========================================================================
    // [NEW] PerspectiveCamera - Main 3D render pass camera
    // ========================================================================

    /**
     * @brief Perspective camera for 3D GBuffer/deferred rendering
     *
     * [NEW] Implements perspective projection with:
     * - Field of view (FOV) in degrees
     * - Aspect ratio (width/height)
     * - Near/far clipping planes (inherited from CameraBase)
     *
     * Coordinate System:
     * - Standard 3D world coordinates
     * - Uses engine's coordinate system conversion via m_rendererCanonicalMatrix
     *
     * When to use PerspectiveCamera:
     * - 3D game main camera
     * - Any 3D scene requiring depth perception
     * - First-person / third-person views
     *
     * m_rendererCanonicalMatrix: 3D coordinate system conversion
     * - Converts engine coordinate system to rendering coordinate system
     * - Inherited from CameraBase default initialization
     *
     * Usage:
     * ```cpp
     * auto camera = std::make_unique<PerspectiveCamera>(
     *     Vec3::ZERO, EulerAngles(), 90.0f, 16.0f/9.0f, 0.1f, 1000.0f);
     * camera->UpdateMatrixUniforms(uniforms);
     * ```
     */
    class PerspectiveCamera final : public CameraBase
    {
    public:
        // ====================================================================
        // Lifecycle
        // ====================================================================

        /**
         * @brief Construct perspective camera
         * @param position Camera world position
         * @param orientation Camera rotation (yaw, pitch, roll)
         * @param fov Field of view in degrees (0 < fov < 180)
         * @param aspectRatio Width/height ratio (> 0)
         * @param nearPlane Near clipping plane distance
         * @param farPlane Far clipping plane distance
         * @throws InvalidCameraParameterException if FOV or aspectRatio invalid
         */
        PerspectiveCamera(const Vec3& position, const EulerAngles& orientation,
                          float       fov, float                   aspectRatio,
                          float       nearPlane = 0.1f, float      farPlane = 1000.0f);

        ~PerspectiveCamera() override = default;

        // Move semantics
        PerspectiveCamera(PerspectiveCamera&&)            = default;
        PerspectiveCamera& operator=(PerspectiveCamera&&) = default;

        // ====================================================================
        // ICamera Implementation
        // ====================================================================

        /**
         * @brief Get perspective projection matrix
         * @return Perspective projection matrix (view-to-clip)
         */
        [[nodiscard]] Mat44 GetProjectionMatrix() const override;

        /**
         * @brief Get camera type
         * @return CameraType::Perspective
         */
        [[nodiscard]] CameraType GetCameraType() const override;
        /**
         * @brief Fill GBuffer matrices in MatricesUniforms
         * @param uniforms Reference to uniforms struct for GPU upload
         * 
         * Fills: gbufferModelView, gbufferModelViewInverse, gbufferProjection,
         *        gbufferProjectionInverse, modelViewMatrix, modelViewMatrixInverse,
         *        projectionMatrix, projectionMatrixInverse
         */
        void             UpdateMatrixUniforms(MatricesUniforms& uniforms) const override;
        MatricesUniforms GetMatrixUniforms() override;

        // ====================================================================
        // [NEW] Perspective-specific Setters
        // ====================================================================

        /**
         * @brief Set field of view
         * @param fov FOV in degrees (0 < fov < 180)
         * @throws InvalidCameraParameterException if FOV invalid
         */
        void SetFOV(float fov);

        /**
         * @brief Set aspect ratio
         * @param aspectRatio Width/height ratio (> 0)
         * @throws InvalidCameraParameterException if aspectRatio invalid
         */
        void SetAspectRatio(float aspectRatio);

        // ====================================================================
        // [NEW] Perspective-specific Getters
        // ====================================================================

        [[nodiscard]] float GetFOV() const { return m_fov; }
        [[nodiscard]] float GetAspectRatio() const { return m_aspectRatio; }

    private:
        // ====================================================================
        // Validation
        // ====================================================================

        static void ValidateFOV(float fov);
        static void ValidateAspectRatio(float aspectRatio);

        // ====================================================================
        // [NEW] Private Members
        // ====================================================================

        float m_fov         = 90.0f;
        float m_aspectRatio = 16.0f / 9.0f;
    };
} // namespace enigma::graphic
