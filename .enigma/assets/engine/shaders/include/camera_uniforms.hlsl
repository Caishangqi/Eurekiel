/**
 * @file camera_uniforms.hlsl
 * @brief Camera Uniforms Constant Buffer - Iris CameraUniforms.java Compatible
 * @date 2026-01-23
 * @version v1.0
 *
 * [Iris Reference]
 *   Source: CameraUniforms.java (lines 22-35)
 *   Path: Iris-multiloader-new/common/src/main/java/net/irisshaders/iris/uniforms/CameraUniforms.java
 *
 * [Iris Fields]
 *   Line 26: near                      - Near plane (fixed 0.05)
 *   Line 27: far                       - Far plane (renderDistance * 16 blocks)
 *   Line 28: cameraPosition            - Camera world position
 *   Line 29: eyeAltitude               - Camera Y position
 *   Line 30: previousCameraPosition    - Previous frame camera position
 *   Line 31: cameraPositionInt         - Integer part of position
 *   Line 32: cameraPositionFract       - Fractional part of position
 *   Line 33: previousCameraPositionInt - Previous frame integer position
 *   Line 34: previousCameraPositionFract - Previous frame fractional position
 *
 * [Memory Layout] 96 bytes (6 rows x 16 bytes)
 *   Row 0 [0-15]:   cameraPosition (float3), near
 *   Row 1 [16-31]:  previousCameraPosition (float3), far
 *   Row 2 [32-47]:  cameraPositionFract (float3), eyeAltitude
 *   Row 3 [48-63]:  previousCameraPositionFract (float3), _pad0
 *   Row 4 [64-79]:  cameraPositionInt (int3), _pad1
 *   Row 5 [80-95]:  previousCameraPositionInt (int3), _pad2
 *
 * C++ Counterpart: Code/Engine/Graphic/Shader/Uniform/CameraUniforms.hpp
 */

// =============================================================================
// CameraUniforms Constant Buffer (96 bytes = 6 rows x 16 bytes)
// =============================================================================

cbuffer CameraUniforms : register(b9)
{
    // ==================== Row 0: Camera Position + Near (16 bytes) ====================
    // @iris cameraPosition (CameraUniforms.java:28)
    // Camera world position, periodically reset for precision
    float3 cameraPosition;

    // @iris near (CameraUniforms.java:26)
    // Near plane distance, fixed at 0.05 in Iris
    float near;

    // ==================== Row 1: Previous Camera Position + Far (16 bytes) ====================
    // @iris previousCameraPosition (CameraUniforms.java:30)
    // Previous frame camera position for motion blur, TAA, etc.
    float3 previousCameraPosition;

    // @iris far (CameraUniforms.java:27)
    // Far plane = renderDistance * 16 (in blocks)
    // Example: 6 chunks = 96 blocks
    float far;

    // ==================== Row 2: Camera Position Fract + Eye Altitude (16 bytes) ====================
    // @iris cameraPositionFract (CameraUniforms.java:32)
    // Fractional part of camera position [0,1)
    float3 cameraPositionFract;

    // @iris eyeAltitude (CameraUniforms.java:29)
    // Player eye height (= cameraPosition.y)
    float eyeAltitude;

    // ==================== Row 3: Previous Camera Position Fract (16 bytes) ====================
    // @iris previousCameraPositionFract (CameraUniforms.java:34)
    // Previous frame fractional position [0,1)
    float3 previousCameraPositionFract;
    float  _cam_pad0;

    // ==================== Row 4: Camera Position Int (16 bytes) ====================
    // @iris cameraPositionInt (CameraUniforms.java:31)
    // Integer part of camera position (floor)
    int3 cameraPositionInt;
    int  _cam_pad1;

    // ==================== Row 5: Previous Camera Position Int (16 bytes) ====================
    // @iris previousCameraPositionInt (CameraUniforms.java:33)
    // Previous frame integer position
    int3 previousCameraPositionInt;
    int  _cam_pad2;
};

// =============================================================================
// [HELPER FUNCTIONS] Depth Reconstruction
// =============================================================================

/**
 * @brief Convert NDC depth to linear view-space depth
 * @param ndcDepth Depth value from depth buffer [0, 1]
 * @return Linear depth in view space (world units)
 *
 * [IMPORTANT] Uses near/far from CameraUniforms
 * Formula: linearDepth = (near * far) / (far - ndcDepth * (far - near))
 */
float LinearizeDepth(float ndcDepth)
{
    // Handle sky (depth = 1.0)
    if (ndcDepth >= 1.0)
    {
        return far;
    }
    return (near * far) / (far - ndcDepth * (far - near));
}

/**
 * @brief Convert linear depth back to NDC depth
 * @param linearDepth Linear depth in view space
 * @return NDC depth [0, 1]
 */
float LinearToNDCDepth(float linearDepth)
{
    return (far * (linearDepth - near)) / (linearDepth * (far - near));
}

/**
 * Usage Example:
 *
 * #include "../include/camera_uniforms.hlsl"
 *
 * float4 main(PSInput input) : SV_TARGET
 * {
 *     // Sample depth
 *     float depth = depthtex0.Sample(sampler0, input.uv).r;
 *
 *     // Convert to linear depth using near/far from CameraUniforms
 *     float linearDepth = LinearizeDepth(depth);
 *
 *     // Use for fog, DOF, etc.
 *     float fogFactor = saturate(linearDepth / far);
 *
 *     return float4(color * (1.0 - fogFactor), 1.0);
 * }
 */
