/**
 * @file sampler_uniforms.hlsl
 * @brief [NEW] Dynamic Sampler System - Bindless Sampler Access via Uniform Buffer
 * @date 2026-01-17
 * @version v1.0
 *
 * Key Points:
 * 1. Uses cbuffer to store sampler bindless indices (16 slots)
 * 2. Register b8 - Engine reserved slot for sampler indices
 * 3. PerFrame update frequency - Updated when sampler config changes
 * 4. Field order must match SamplerIndicesBuffer.hpp exactly
 *
 * Architecture Benefits:
 * - Dynamic: Samplers can be changed at runtime via SetSamplerConfig()
 * - Bindless: Uses SM6.6 SamplerDescriptorHeap for direct heap access
 * - Unified: Follows same pattern as ColorTargetsIndexBuffer
 *
 * Default Sampler Slots:
 * - sampler0: Linear filter (texture sampling)
 * - sampler1: Point filter (pixel-perfect sampling)
 * - sampler2: Shadow comparison sampler
 * - sampler3: Point filter with wrap addressing
 *
 * C++ Counterpart: Engine/Code/Engine/Graphic/Shader/Uniform/SamplerIndicesBuffer.hpp
 *
 * Reference:
 * - SM6.6 Bindless: SamplerDescriptorHeap[index] syntax
 * - D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
 */

#ifndef SAMPLER_UNIFORMS_HLSL
#define SAMPLER_UNIFORMS_HLSL

// =============================================================================
// [CONSTANTS] Sampler System Limits
// =============================================================================

#define MAX_SAMPLERS 16

// =============================================================================
// SamplerIndices Constant Buffer (64 bytes)
// =============================================================================
/**
 * @brief SamplerIndices - Bindless Sampler Index Buffer
 *
 * Memory Layout (16-byte aligned):
 * [0-63]  samplerIndices[16] (16 * 4 = 64 bytes)
 *
 * Each index points to a sampler in the SamplerDescriptorHeap.
 * Use GetSampler(index) to retrieve the actual SamplerState.
 */
cbuffer SamplerIndices : register(b8)
{
    uint samplerIndices[MAX_SAMPLERS];
};

// =============================================================================
// [ACCESSOR FUNCTION] GetSampler
// =============================================================================
/**
 * @brief Retrieve a SamplerState from the bindless sampler heap
 * @param index Sampler slot index (0-15)
 * @return SamplerState from the descriptor heap
 *
 * Uses SM6.6 SamplerDescriptorHeap keyword for direct heap access.
 * The samplerIndices buffer contains the actual heap indices.
 */
SamplerState GetSampler(uint index)
{
    return SamplerDescriptorHeap[samplerIndices[index]];
}

// =============================================================================
// [USER-FRIENDLY MACROS] Default Sampler Accessors
// =============================================================================
// These macros provide convenient access to the 4 default samplers.
// They replace the old static sampler declarations (linearSampler, etc.)
//
// Slot Mapping:
//   sampler0 = Linear filter   (replaces linearSampler)
//   sampler1 = Point filter    (replaces pointSampler)
//   sampler2 = Shadow sampler  (replaces shadowSampler)
//   sampler3 = Point + Wrap    (replaces wrapSampler)
// =============================================================================

#define sampler0 GetSampler(0)
#define sampler1 GetSampler(1)
#define sampler2 GetSampler(2)
#define sampler3 GetSampler(3)

// =============================================================================
// [LEGACY COMPATIBILITY] Sampler Aliases
// =============================================================================
// These aliases maintain backward compatibility with existing shader code.
// New code should use sampler0-3 macros directly.
// =============================================================================

#define linearSampler sampler0
#define pointSampler  sampler1
#define shadowSampler sampler2
#define wrapSampler   sampler3

#endif // SAMPLER_UNIFORMS_HLSL

/**
 * Usage Example:
 *
 * #include "../include/sampler_uniforms.hlsl"
 *
 * Texture2D diffuseTexture : register(t0);
 *
 * float4 main(PSInput input) : SV_TARGET
 * {
 *     // Method 1: Use numbered macro (recommended)
 *     float4 color = diffuseTexture.Sample(sampler0, input.uv);
 *
 *     // Method 2: Use legacy alias (backward compatible)
 *     float4 color2 = diffuseTexture.Sample(linearSampler, input.uv);
 *
 *     // Method 3: Use GetSampler directly (for dynamic index)
 *     uint samplerIdx = 1; // Point sampler
 *     float4 color3 = diffuseTexture.Sample(GetSampler(samplerIdx), input.uv);
 *
 *     return color;
 * }
 */
