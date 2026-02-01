#pragma once

// ============================================================================
// ShadowCamera.hpp - [NEW] Shadow camera for shadow pass rendering
// 
// Extends OrthographicCamera for shadow mapping. Fills shadow matrices
// in MatricesUniforms instead of gbuffer matrices.
// ============================================================================

#include "OrthographicCamera.hpp"

namespace enigma::graphic
{
    // Forward declaration
    struct MatricesUniforms;

    // ========================================================================
    // [NEW] ShadowCamera - Shadow pass rendering camera
    // ========================================================================

    /**
     * @brief Shadow camera for shadow map generation
     *
     * [NEW] Extends OrthographicCamera with:
     * - Cascade shadow mapping support via cascadeIndex
     * - Fills ONLY shadow matrices (not gbuffer matrices)
     * - Light-space view/projection for shadow depth rendering
     *
     * Coordinate System:
     * - Light-space coordinates (looking from light toward scene)
     * - Orthographic projection for directional lights
     *
     * When to use ShadowCamera:
     * - Shadow map rendering pass
     * - Directional light shadow generation
     * - Cascade shadow mapping (CSM)
     *
     * m_rendererCanonicalMatrix: Light-space coordinate conversion
     * - SetIJK3D(Vec3(0,0,1), Vec3(-1,0,0), Vec3(0,1,0))
     * - Converts to light-space rendering coordinates
     *
     * Matrix Output (UpdateMatrixUniforms):
     * - Fills: shadowView, shadowViewInverse, shadowProjection, shadowProjectionInverse
     * - Does NOT fill gbuffer matrices
     *
     * Usage:
     * ```cpp
     * auto shadowCam = std::make_unique<ShadowCamera>(
     *     lightPos, lightDir, Vec2(-50, -50), Vec2(50, 50), 0.1f, 500.0f);
     * shadowCam->SetCascadeIndex(0);  // For CSM
     * shadowCam->UpdateMatrixUniforms(uniforms);
     * ```
     */
    class ShadowCamera final : public OrthographicCamera
    {
    public:
        // ====================================================================
        // Lifecycle
        // ====================================================================

        /**
         * @brief Construct shadow camera from light perspective
         * @param lightPosition Light world position
         * @param lightDirection Light direction (used for orientation)
         * @param shadowBoundsMin Bottom-left corner of shadow bounds
         * @param shadowBoundsMax Top-right corner of shadow bounds
         * @param nearPlane Near clipping plane distance
         * @param farPlane Far clipping plane distance
         */
        ShadowCamera(const Vec3& lightPosition, const EulerAngles& lightDirection,
                     const Vec2& shadowBoundsMin, const Vec2&      shadowBoundsMax,
                     float       nearPlane = 0.1f, float           farPlane = 1000.0f);

        ~ShadowCamera() override = default;

        // Move semantics
        ShadowCamera(ShadowCamera&&)            = default;
        ShadowCamera& operator=(ShadowCamera&&) = default;

        // ====================================================================
        // ICamera Implementation
        // ====================================================================

        /**
         * @brief Get camera type
         * @return CameraType::Shadow
         */
        [[nodiscard]] CameraType GetCameraType() const override;

        /**
         * @brief Fill shadow matrices in MatricesUniforms
         * @param uniforms Reference to uniforms struct for GPU upload
         * 
         * [IMPORTANT] Fills ONLY shadow matrices:
         * - shadowView
         * - shadowViewInverse
         * - shadowProjection
         * - shadowProjectionInverse
         * 
         * Does NOT fill gbuffer matrices.
         */
        void             UpdateMatrixUniforms(MatricesUniforms& uniforms) const override;
        MatricesUniforms GetMatrixUniforms() override;

        // ====================================================================
        // [NEW] Cascade Shadow Mapping Support
        // ====================================================================

        void              SetCascadeIndex(int cascadeIndex) { m_cascadeIndex = cascadeIndex; }
        [[nodiscard]] int GetCascadeIndex() const { return m_cascadeIndex; }

    private:
        // ====================================================================
        // [NEW] Private Members
        // ====================================================================

        int m_cascadeIndex = 0;
    };
} // namespace enigma::graphic
