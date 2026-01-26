/**
* @brief PerObjectUniforms - Per-Object data constant buffer (cbuffer register(b1))
 *
 *Teaching points:
 * 1. Use cbuffer to store the transformation matrix and color of each object
 * 2. GPU hardware directly optimizes cbuffer access
 * 3. Support Per-Object draw mode (updated every Draw Call)
 * 4. The field order must be exactly the same as PerObjectUniforms.hpp
 * 5. Use float4 to store Rgba8 colors (converted on CPU side)
 *
 * Architectural advantages:
 * - High performance: Root CBV direct access, no StructuredBuffer indirection required
 * - Memory alignment: 16-byte alignment, GPU friendly
 * - Compatibility: Supports Instancing and Per-Object data
 *
 * Corresponding to C++: PerObjectUniforms.hpp
 */
cbuffer PerObjectUniforms : register(b1)
{
    float4x4 modelMatrix; // Model matrix (model space → world space)
    float4x4 modelMatrixInverse; // Model inverse matrix (world space → model space)
    float4   modelColor; // Model color (RGBA, normalized to [0,1])
};
