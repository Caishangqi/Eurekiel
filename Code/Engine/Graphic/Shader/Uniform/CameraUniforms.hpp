#pragma once

#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/IntVec3.hpp"

namespace enigma::graphic
{
    // =============================================================================
    // CameraUniforms - Iris CameraUniforms.java Compatible Constant Buffer
    // =============================================================================
    //
    // [Purpose]
    //   Provides camera data to shaders, strictly matching Iris CameraUniforms.java
    //   Reference: Iris CameraUniforms.java addCameraUniforms() (lines 22-35)
    //
    // [Shader Side Declaration]
    //   Corresponds to HLSL: cbuffer CameraUniforms : register(b3, space1)
    //   File location: .enigma/assets/engine/shaders/include/camera_uniforms.hlsl
    //
    // [Iris CameraUniforms.java Variable Mapping]
    //   Line 26: near                      -> near (ONCE, fixed 0.05)
    //   Line 27: far                       -> far (PER_FRAME, renderDistance * 16)
    //   Line 28: cameraPosition            -> cameraPosition
    //   Line 29: eyeAltitude               -> eyeAltitude (= cameraPosition.y)
    //   Line 30: previousCameraPosition    -> previousCameraPosition
    //   Line 31: cameraPositionInt         -> cameraPositionInt
    //   Line 32: cameraPositionFract       -> cameraPositionFract
    //   Line 33: previousCameraPositionInt -> previousCameraPositionInt
    //   Line 34: previousCameraPositionFract -> previousCameraPositionFract
    //
    // [Memory Layout] 160 bytes (10 rows x 16 bytes)
    //   Row 0 [0-15]:   cameraPosition (float3), near
    //   Row 1 [16-31]:  previousCameraPosition (float3), far
    //   Row 2 [32-47]:  cameraPositionFract (float3), eyeAltitude
    //   Row 3 [48-63]:  previousCameraPositionFract (float3), _pad0
    //   Row 4 [64-79]:  cameraPositionInt (int3), _pad1
    //   Row 5 [80-95]:  previousCameraPositionInt (int3), _pad2
    //
    // =============================================================================

#pragma warning(push)
#pragma warning(disable: 4324) // Structure padded due to alignas - expected behavior

    struct CameraUniforms
    {
        // ==================== Row 0: Camera Position + Near (16 bytes) ====================
        // @iris cameraPosition (CameraUniforms.java:28)
        // Camera world position, periodically reset for precision
        // Iris: resets every 30000 blocks or teleport > 1000 blocks
        alignas(16) Vec3 cameraPosition;

        // @iris near (CameraUniforms.java:26)
        // Near plane distance, fixed at 0.05 in Iris
        float nearPlane;

        // ==================== Row 1: Previous Camera Position + Far (16 bytes) ====================
        // @iris previousCameraPosition (CameraUniforms.java:30)
        // Previous frame camera position for motion blur, TAA, etc.
        alignas(16) Vec3 previousCameraPosition;

        // @iris far (CameraUniforms.java:27)
        // Far plane = renderDistance * 16 (in blocks)
        // Example: 6 chunks = 96 blocks
        float farPlane;

        // ==================== Row 2: Camera Position Fract + Eye Altitude (16 bytes) ====================
        // @iris cameraPositionFract (CameraUniforms.java:32)
        // Fractional part of camera position [0,1)
        alignas(16) Vec3 cameraPositionFract;

        // @iris eyeAltitude (CameraUniforms.java:29)
        // Player eye height (= cameraPosition.y)
        float eyeAltitude;

        // ==================== Row 3: Previous Camera Position Fract (16 bytes) ====================
        // @iris previousCameraPositionFract (CameraUniforms.java:34)
        // Previous frame fractional position [0,1)
        alignas(16) Vec3 previousCameraPositionFract;
        float            _pad0;

        // ==================== Row 4: Camera Position Int (16 bytes) ====================
        // @iris cameraPositionInt (CameraUniforms.java:31)
        // Integer part of camera position (floor)
        alignas(16) IntVec3 cameraPositionInt;
        int                 _pad1;

        // ==================== Row 5: Previous Camera Position Int (16 bytes) ====================
        // @iris previousCameraPositionInt (CameraUniforms.java:33)
        // Previous frame integer position
        alignas(16) IntVec3 previousCameraPositionInt;
        int                 _pad2;
        // ==================== Default Constructor ====================
        CameraUniforms()
            : cameraPosition(Vec3::ZERO)
              , nearPlane(0.05f) // Iris default: 0.05
              , previousCameraPosition(Vec3::ZERO)
              , farPlane(96.0f) // Default: 6 chunks * 16 = 96 blocks
              , cameraPositionFract(Vec3::ZERO)
              , eyeAltitude(0.0f)
              , previousCameraPositionFract(Vec3::ZERO)
              , _pad0(0.0f)
              , cameraPositionInt(IntVec3::ZERO)
              , _pad1(0)
              , previousCameraPositionInt(IntVec3::ZERO)
              , _pad2(0)
        {
        }
    };

#pragma warning(pop)

    // Compile-time validation: 6 rows * 16 bytes = 96 bytes
    static_assert(sizeof(CameraUniforms) == 96,
                  "CameraUniforms size mismatch - expected 96 bytes (6 rows * 16 bytes)");
} // namespace enigma::graphic
