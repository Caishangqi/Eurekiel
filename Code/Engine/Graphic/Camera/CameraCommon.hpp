#pragma once

// ============================================================================
// CameraCommon.hpp - [NEW] Foundation types for Camera system
// 
// Contains: LogCamera category, CameraType enum, Exception hierarchy
// Pattern: Follows UniformCommon.hpp exception design
// ============================================================================

#include <cstdint>
#include <stdexcept>
#include <string>

#include "Engine/Core/LogCategory/LogCategory.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // [NEW] Log Category Declaration
    // ========================================================================

    DECLARE_LOG_CATEGORY_EXTERN(LogCamera);

    // ========================================================================
    // [NEW] CameraType Enum - Camera classification
    // ========================================================================

    /**
     * @brief Camera type classification for rendering system
     * 
     * [NEW] Defines camera behavior and matrix calculation strategy:
     * - Perspective:   Standard 3D camera with FOV-based projection
     * - Orthographic:  2D/UI camera with parallel projection
     * - Shadow:        Light-space camera for shadow mapping
     * - Reflection:    Mirror/water reflection camera
     * - Cubemap:       6-face environment capture camera
     */
    enum class CameraType : uint8_t
    {
        Perspective = 0, // Standard 3D perspective projection
        Orthographic = 1, // Parallel projection for 2D/UI
        Shadow = 2, // Shadow map generation
        Reflection = 3, // Planar reflection rendering
        Cubemap = 4 // Environment map capture (6 faces)
    };

    // ========================================================================
    // [NEW] Exception Hierarchy - Type-safe error handling
    // ========================================================================

    /**
     * @brief Base exception class for Camera system
     * 
     * [NEW] Root of Camera exception hierarchy
     * Inherits from std::runtime_error for standard library compatibility
     */
    class CameraException : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    /**
     * @brief Exception for invalid camera parameters
     * 
     * [NEW] Thrown when camera configuration is invalid:
     * - Invalid FOV (<=0 or >=180)
     * - Invalid aspect ratio (<=0)
     * - Invalid near/far planes (near >= far or negative)
     */
    class InvalidCameraParameterException : public CameraException
    {
    public:
        using CameraException::CameraException;
    };

    /**
     * @brief Exception for camera matrix calculation errors
     * 
     * [NEW] Thrown when matrix operations fail:
     * - Singular matrix (non-invertible)
     * - NaN/Inf values in matrix
     * - Invalid view direction (zero length)
     */
    class CameraMatrixException : public CameraException
    {
    public:
        using CameraException::CameraException;
    };
} // namespace enigma::graphic
