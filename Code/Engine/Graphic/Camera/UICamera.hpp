#pragma once

// ============================================================================
// UICamera.hpp - [NEW] UI/2D rendering camera with Y-axis flip
//
// Extends OrthographicCamera for 2D UI rendering with DirectX coordinate
// system (top-left origin, Y increases downward).
// ============================================================================

#include "OrthographicCamera.hpp"

namespace enigma::graphic
{
    // Forward declaration
    struct MatricesUniforms;

    // ========================================================================
    // [NEW] UICamera - 2D UI and 2D Game rendering camera
    // ========================================================================

    /**
     * @brief UI camera for 2D rendering with DirectX coordinate system
     *
     * [NEW] Extends OrthographicCamera with:
     * - Y-axis flip for DirectX texture coordinate system (top-left origin)
     * - Identity renderer matrix (no 3D coordinate conversion)
     * - Optimized for screen-space UI and 2D game rendering
     *
     * Coordinate System:
     * - Origin at top-left corner (0, 0)
     * - X increases rightward
     * - Y increases downward (flipped from 3D)
     *
     * When to use UICamera:
     * - 2D games with screen-space coordinates
     * - UI/HUD rendering
     * - Any 2D content where (0,0) should be top-left
     *
     * m_rendererCanonicalMatrix: Identity (no correction needed)
     * - Y-flip is handled in GetProjectionMatrix() via swapped bottom/top
     *
     * Usage:
     * ```cpp
     * // Create UI camera covering screen
     * auto uiCamera = UICamera::Create(Vec2(1920, 1080));
     *
     * // Sprite at (100, 200) means:
     * // - 100 pixels from left edge
     * // - 200 pixels from top edge
     * ```
     */
    class UICamera final : public OrthographicCamera
    {
    public:
        // ====================================================================
        // Lifecycle
        // ====================================================================

        /**
         * @brief Construct UI camera
         * @param screenSize Screen dimensions (width, height)
         * @param nearPlane Near clipping plane distance (default 0.0)
         * @param farPlane Far clipping plane distance (default 1.0)
         */
        UICamera(const Vec2& screenSize, float nearPlane = 0.0f, float farPlane = 1.0f);

        ~UICamera() override = default;

        // Move semantics
        UICamera(UICamera&&)            = default;
        UICamera& operator=(UICamera&&) = default;

        // ====================================================================
        // [NEW] Static Factory Methods
        // ====================================================================

        /**
         * @brief Create UI camera for 2D rendering
         * @param screenSize Screen dimensions (width, height)
         * @return UICamera configured for UI rendering
         */
        [[nodiscard]] static UICamera Create(const Vec2& screenSize);

        // ====================================================================
        // ICamera Implementation
        // ====================================================================

        /**
         * @brief Get orthographic projection matrix with Y-axis flip
         * @return Orthographic projection matrix (view-to-clip) with Y flipped
         *
         * [IMPORTANT] Y-axis is flipped for DirectX texture coordinate system:
         * - OpenGL: bottom = -size.y/2, top = +size.y/2
         * - DirectX: bottom = +size.y/2, top = -size.y/2 (flipped)
         */
        [[nodiscard]] Mat44 GetProjectionMatrix() const override;

        /**
         * @brief Get camera type
         * @return CameraType::UI
         */
        [[nodiscard]] CameraType GetCameraType() const override;

        // ====================================================================
        // [NEW] UI-specific Methods
        // ====================================================================

        void                      SetScreenSize(const Vec2& screenSize);
        [[nodiscard]] const Vec2& GetScreenSize() const { return m_topRight; }
    };
} // namespace enigma::graphic
