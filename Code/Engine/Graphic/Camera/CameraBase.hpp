#pragma once

// ============================================================================
// CameraBase.hpp - [NEW] Abstract base class for camera implementations
// 
// [REFACTOR] Extracted common functionality from EnigmaCamera.cpp
// Provides: position, orientation, near/far planes, view matrix calculation
// ============================================================================

#include "ICamera.hpp"

#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // [NEW] CameraBase - Template Method base class
    // ========================================================================

    /**
     * @brief Abstract base class implementing ICamera interface
     * 
     * [REFACTOR] Common camera functionality extracted from EnigmaCamera:
     * - Position and orientation management
     * - Near/far plane configuration
     * - View matrix calculation (world-to-camera transform)
     * 
     * Subclasses must implement:
     * - GetProjectionMatrix(): Camera-specific projection
     * - GetCameraType(): Camera classification
     * - UpdateMatrixUniforms(): Fill GPU uniform buffer
     */
    class CameraBase : public ICamera
    {
    public:
        // ====================================================================
        // Lifecycle
        // ====================================================================

        CameraBase() = default;
        CameraBase(const Vec3& position, const EulerAngles& orientation, float nearPlane = 0.1f, float farPlane = 1000.0f);
        ~CameraBase() override = default;

        // Move semantics
        CameraBase(CameraBase&&)            = default;
        CameraBase& operator=(CameraBase&&) = default;

        // ====================================================================
        // ICamera Implementation
        // ====================================================================

        /**
         * @brief Get view matrix (world-to-camera transformation)
         * @return Inverse of camera-to-world transform
         */
        [[nodiscard]] Mat44 GetViewMatrix() const override;

        // Pure virtual - must be implemented by subclasses
        [[nodiscard]] Mat44      GetProjectionMatrix() const override = 0;
        [[nodiscard]] CameraType GetCameraType() const override = 0;
        Mat44                    GetRendererCanonicalMatrix() const override;
        void                     UpdateMatrixUniforms(MatricesUniforms& uniforms) const override = 0;
        MatricesUniforms         GetMatrixUniforms() override = 0;
        CameraUniforms           GetCameraUniforms() override;

        // ====================================================================
        // [NEW] Public Setters
        // ====================================================================

        void SetPosition(const Vec3& position);
        void SetOrientation(const EulerAngles& orientation);
        void SetPositionAndOrientation(const Vec3& position, const EulerAngles& orientation);
        void SetNearFar(float nearPlane, float farPlane);

        // ====================================================================
        // [NEW] Public Getters
        // ====================================================================

        [[nodiscard]] const Vec3&        GetPosition() const { return m_position; }
        [[nodiscard]] const EulerAngles& GetOrientation() const { return m_orientation; }
        [[nodiscard]] float              GetNearPlane() const { return m_nearPlane; }
        [[nodiscard]] float              GetFarPlane() const { return m_farPlane; }

    protected:
        // ====================================================================
        // [REFACTOR] Protected Helpers - Extracted from EnigmaCamera
        // ====================================================================

        /**
         * @brief Calculate camera-to-world transformation matrix
         * @return Matrix transforming camera space to world space
         * 
         * [REFACTOR] Rotation order: Translation * Z(yaw) * X(roll) * Y(pitch)
         * Extracted from EnigmaCamera::GetCameraToWorldTransform()
         */
        [[nodiscard]] Mat44 GetCameraToWorldTransform() const;

        /**
         * @brief Calculate view matrix from camera-to-world transform
         * @return Inverse of camera-to-world transform
         */
        [[nodiscard]] Mat44 CalculateViewMatrix() const;

    protected:
        // ====================================================================
        // [NEW] Protected Members
        // ====================================================================

        Vec3        m_position                = Vec3::ZERO;
        EulerAngles m_orientation             = EulerAngles();
        Mat44       m_rendererCanonicalMatrix = Mat44();
        float       m_nearPlane               = 0.1f;
        float       m_farPlane                = 1000.0f;
    };
} // namespace enigma::graphic
