#pragma once

// ============================================================================
// ICamera.hpp - [NEW] Camera interface (Strategy Pattern)
// 
// Pure virtual interface defining contract for all camera types.
// Implementations: PerspectiveCamera, OrthographicCamera, ShadowCamera, etc.
// ============================================================================

#include "CameraCommon.hpp"
#include "Engine/Graphic/Shader/Uniform/CameraUniforms.hpp"

#include "Engine/Math/Mat44.hpp"

namespace enigma::graphic
{
    // Forward declaration
    struct MatricesUniforms;

    // ========================================================================
    // [NEW] ICamera Interface - Strategy Pattern base
    // ========================================================================

    /**
     * @brief Pure virtual interface for camera implementations
     * 
     * [NEW] Strategy Pattern interface for camera system:
     * - GetViewMatrix():         Returns world-to-view transformation
     * - GetProjectionMatrix():   Returns view-to-clip transformation
     * - GetCameraType():         Returns camera classification
     * - UpdateMatrixUniforms():  Fills MatricesUniforms for GPU upload
     * 
     * Usage:
     * ```cpp
     * std::unique_ptr<ICamera> camera = std::make_unique<PerspectiveCamera>(...);
     * Mat44 view = camera->GetViewMatrix();
     * Mat44 proj = camera->GetProjectionMatrix();
     * camera->UpdateMatrixUniforms(uniforms);
     * ```
     */
    class ICamera
    {
    public:
        // ====================================================================
        // Lifecycle
        // ====================================================================

        virtual ~ICamera() = default;

        // Prevent copying, allow moving
        ICamera()                          = default;
        ICamera(const ICamera&)            = delete;
        ICamera& operator=(const ICamera&) = delete;
        ICamera(ICamera&&)                 = default;
        ICamera& operator=(ICamera&&)      = default;

        // ====================================================================
        // [NEW] Core Interface Methods
        // ====================================================================

        /**
         * @brief Get view matrix (world-to-view transformation)
         * @return View matrix transforming world space to camera/view space
         */
        [[nodiscard]] virtual Mat44 GetViewMatrix() const = 0;
        [[nodiscard]] virtual Mat44 GetRendererCanonicalMatrix() const = 0;

        /**
         * @brief Get projection matrix (view-to-clip transformation)
         * @return Projection matrix transforming view space to clip space
         */
        [[nodiscard]] virtual Mat44 GetProjectionMatrix() const = 0;

        /**
         * @brief Get camera type classification
         * @return CameraType enum value identifying camera behavior
         */
        [[nodiscard]] virtual CameraType GetCameraType() const = 0;

        /**
         * @brief Update MatricesUniforms with camera matrices
         * @param uniforms Reference to MatricesUniforms struct for GPU upload
         * 
         * Template Method hook - implementations fill relevant matrix fields:
         * - Perspective: gbufferModelView, gbufferProjection, etc.
         * - Shadow: shadowView, shadowProjection, etc.
         */
        virtual void             UpdateMatrixUniforms(MatricesUniforms& uniforms) const = 0;
        virtual MatricesUniforms GetMatrixUniforms() = 0;
        virtual CameraUniforms   GetCameraUniforms() = 0;
    };
} // namespace enigma::graphic
