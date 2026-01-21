/**
 * @file Common.hlsl
 * @brief Bindless resource access core library - Iris fully compatible architecture + CustomImage extension
 * @date 2025-11-04
 * @version v3.3 - Full comment update (T2-T10 structure documentation)
 *
 *Teaching points:
 * 1. All shaders must #include "Common.hlsl"
 * 2. Supports the complete Iris texture system: colortex, shadowcolor, depthtex, shadowtex, noisetex
 * 3. Added CustomImage system: customImage0-15 (custom material slot)
 * 4. Main/Alt double buffering - eliminate ResourceBarrier overhead
 * 5. Bindless resource access - Shader Model 6.6
 * 6. MRT dynamic generation - PSOutput is dynamically created by ShaderCodeGenerator
 * 7. Shadow system split design - shadowcolor and shadowtex independent Buffer management
 *
 *Major changes (v3.3):
 * - T2: Complete Iris documentation of IDData structure (9 fields, entity/block/item ID system)
 * - T3: Complete documentation of BiomeAndDimensionData structure (12 fields, biome + dimension attributes)
 * - T4: Complete documentation of RenderingData structure (17 fields, rendering phase + fog parameters + Alpha test)
 * - T5: WorldAndWeatherData structure fully documented (12 fields, sun/moon position + weather system)
 * - T6: Complete documentation of CameraAndPlayerData structure (camera position + player status)
 * - T7: DepthTexturesBuffer structure documentation (depthtex0/1/2 semantic description)
 * - T8: CustomImageUniforms structure documentation (custom material slot system)
 * - T9: ShadowTexturesBuffer annotation update (independent Buffer management, not "aggressive merging")
 * - T10: Macro definition comment update (Iris compatibility + unified interface design)
 *
 * Architecture reference:
 * - DirectX12-Bindless-MRT-RENDERTARGETS-Architecture.md
 * - RootConstant-Redesign-v3.md (56-byte split design)
 * - CustomImageUniform.hpp (custom material slot system)
 */

/**
 * @brief CustomImageUniforms - Custom material slot constant buffer (64 bytes)
 *
 *Teaching points:
 * 1. Use cbuffer to store the Bindless index of 16 custom material slots (customImage0-15)
 * 2. PerObject frequency - each Draw Call can independently update different materials
 * 3. Direct access through Root CBV - high performance, no need for StructuredBuffer indirection
 * 4. 64-byte alignment - GPU friendly, no additional padding required
 *
 * Architectural design:
 * - Structure size: 64 bytes (16 uint indexes)
 * - PerObject frequency, supports independent materials for each object
 * - Root CBV binding, zero overhead
 * - Compatible with Iris style (customImage0-15 macro)
 *
 * Usage scenarios:
 * - Deferred rendering PBR material map
 * - Custom post-processing LUT texture
 * - Special effect maps (noise, mask, etc.)
 *
 * Corresponding to C++: CustomImageUniform.hpp (note: the C++ side is named in the singular)
 */
cbuffer CustomImageUniforms: register(b2)
{
    uint customImageIndices[16]; // customImage0-15 Bindless indices
};

//──────────────────────────────────────────────────────────────────────────────
// [NEW] RenderTarget Index Buffers - Shader RT Fetching Feature
//──────────────────────────────────────────────────────────────────────────────

/**
 * @brief ColorTargetsBuffer - Color RT indices (128 bytes)
 * @register b3
 * Stores Bindless indices for colortex0-15 with flip support.
 * C++ counterpart: ColorTargetsIndexBuffer.hpp
 *
 * [IMPORTANT] HLSL cbuffer packing rule:
 * - Scalar arrays (uint[16]) align each element to 16 bytes (float4 boundary)
 * - Using uint4[4] ensures tight packing matching C++ uint32_t[16] layout
 * - Memory layout: [0-3][4-7][8-11][12-15] = 4 * 16 = 64 bytes per array
 */
cbuffer ColorTargetsBuffer : register(b3)
{
    uint4 colorReadIndicesPacked[4]; // colortex0-15 read indices (64 bytes)
    uint4 colorWriteIndicesPacked[4]; // colortex0-15 write indices (64 bytes)
};

/**
 * @brief DepthTexturesBuffer - Depth RT indices (64 bytes)
 * @register b4
 * Stores Bindless indices for depthtex0-15. No flip mechanism.
 * C++ counterpart: DepthTexturesIndexBuffer.hpp
 *
 * [IMPORTANT] HLSL cbuffer packing rule:
 * - Using uint4[4] ensures tight packing matching C++ uint32_t[16] layout
 * - Memory layout: [0-3][4-7][8-11][12-15] = 4 * 16 = 64 bytes
 */
cbuffer DepthTexturesBuffer : register(b4)
{
    uint4 depthTextureIndicesPacked[4]; // depthtex0-15 indices (64 bytes)
};

/**
 * @brief ShadowColorBuffer - Shadow color RT indices (64 bytes)
 * @register b5
 * Stores Bindless indices for shadowcolor0-7 with flip support.
 * C++ counterpart: ShadowColorIndexBuffer.hpp
 *
 * [IMPORTANT] HLSL cbuffer packing rule:
 * - Using uint4[2] ensures tight packing matching C++ uint32_t[8] layout
 * - Memory layout: [0-3][4-7] = 2 * 16 = 32 bytes per array
 */
cbuffer ShadowColorBuffer : register(b5)
{
    uint4 shadowColorReadIndicesPacked[2]; // shadowcolor0-7 read indices (32 bytes)
    uint4 shadowColorWriteIndicesPacked[2]; // shadowcolor0-7 write indices (32 bytes)
};

/**
 * @brief ShadowTexturesBuffer - Shadow depth RT indices (16 bytes)
 * @register b6
 * Stores Bindless indices for shadowtex0-1. No flip mechanism.
 * C++ counterpart: ShadowTexturesIndexBuffer.hpp
 *
 * [IMPORTANT] HLSL cbuffer packing rule:
 * - Using uint4 ensures tight packing matching C++ uint32_t[4] layout
 * - Memory layout: [shadowtex0, shadowtex1, pad, pad] = 16 bytes
 */
cbuffer ShadowTexturesBuffer : register(b6)
{
    uint4 shadowTexIndicesPacked; // [shadowtex0, shadowtex1, pad, pad] (16 bytes)
};

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

cbuffer Matrices : register(b7)
{
    float4x4 gbufferView;
    float4x4 gbufferViewInverse;
    float4x4 gbufferProjection;
    float4x4 gbufferProjectionInverse;
    float4x4 gbufferRenderer;

    float4x4 shadowView;
    float4x4 shadowViewInverse;
    float4x4 shadowProjection;
    float4x4 shadowProjectionInverse;

    float4x4 normalMatrix;

    float4x4 textureMatrix;
};


/**
 * @brief Get the Bindless index of the custom material slot Newly added
 * @param slotIndex slot index (0-15)
 * @return Bindless index, if the slot is not set, return UINT32_MAX (0xFFFFFFFF)
 *
 *Teaching points:
 * - Directly access cbuffer CustomImageUniforms to obtain Bindless index
 * - The return value is a Bindless index, which can directly access ResourceDescriptorHeap
 * - UINT32_MAX indicates that the slot is not set and the validity should be checked before use.
 *
 * Working principle:
 * 1. Read the index directly from cbuffer CustomImageUniforms (Root CBV high-performance access)
 * 2. Query customImageIndices[slotIndex] to obtain the Bindless index
 * 3. Return the index for use by GetCustomImage()
 *
 * Security:
 * - Slot index will be limited to the range 0-15
 * - Out of range returns UINT32_MAX (invalid index)
 */
uint GetCustomImageIndex(uint slotIndex)
{
    // Prevent out-of-bounds access
    if (slotIndex >= 16)
    {
        return 0xFFFFFFFF; // UINT32_MAX - invalid index
    }
    // 2. Query the Bindless index of the specified slot
    uint bindlessIndex = customImageIndices[slotIndex];

    // 3. Return the Bindless index
    return bindlessIndex;
}

/**
 * @brief Get custom material texture (convenient access function) added
 * @param slotIndex slot index (0-15)
 * @return corresponding Bindless texture (Texture2D)
 *
 *Teaching points:
 * - Encapsulates GetCustomImageIndex() + ResourceDescriptorHeap access
 * - Automatically handle invalid index cases (return black texture or default texture)
 * - Follows the Iris texture access pattern
 *
 * Usage example:
 * ```hlsl
 * // Get the albedo map (assuming slot 0 stores albedo)
 * Texture2D albedoTex = GetCustomImage(0);
 * float4 albedo = albedoTex.Sample(linearSampler, uv);
 *
 * // Get the roughness map (assuming slot 1 stores roughness)
 * Texture2D roughnessTex = GetCustomImage(1);
 * float roughness = roughnessTex.Sample(linearSampler, uv).r;
 * ```
 *
 * Note:
 * - If slot is not set (UINT32_MAX), accessing ResourceDescriptorHeap may cause undefined behavior
 * - Before calling, make sure the slot is set correctly, or use the default value.
 */
Texture2D GetCustomImage(uint slotIndex)
{
    // 1. Get the Bindless index
    uint bindlessIndex = GetCustomImageIndex(slotIndex);

    // 2. Access the global descriptor heap through Bindless index
    // Note: If bindlessIndex is UINT32_MAX, additional validity checks may be required here.
    // The current design assumes that the C++ side will ensure that all used slots are set correctly
    return ResourceDescriptorHeap[bindlessIndex];
}

//─────────────────────────────────────────────────
// Iris compatible macros (full texture system)
//─────────────────────────────────────────────────
#define customImage0  GetCustomImage(0)
#define customImage1  GetCustomImage(1)
#define customImage2  GetCustomImage(2)
#define customImage3  GetCustomImage(3)
#define customImage4  GetCustomImage(4)
#define customImage5  GetCustomImage(5)
#define customImage6  GetCustomImage(6)
#define customImage7  GetCustomImage(7)
#define customImage8  GetCustomImage(8)
#define customImage9  GetCustomImage(9)
#define customImage10 GetCustomImage(10)
#define customImage11 GetCustomImage(11)
#define customImage12 GetCustomImage(12)
#define customImage13 GetCustomImage(13)
#define customImage14 GetCustomImage(14)
#define customImage15 GetCustomImage(15)

//──────────────────────────────────────────────────────────────────────────────
// [NEW] RenderTarget Access Functions - Shader RT Fetching Feature
//──────────────────────────────────────────────────────────────────────────────

/**
 * @brief Get color texture by slot index (colortex0-15)
 * @param slot Slot index [0-15]
 * @return Texture2D from ResourceDescriptorHeap
 *
 * [IMPORTANT] uint4 unpacking:
 * - slot >> 2 = which uint4 (0-3)
 * - slot & 3  = which component (x=0, y=1, z=2, w=3)
 */
Texture2D GetColorTexture(uint slot)
{
    if (slot >= 16) return ResourceDescriptorHeap[0]; // Safe fallback
    uint4 packed = colorReadIndicesPacked[slot >> 2];
    uint  index  = packed[slot & 3];
    return ResourceDescriptorHeap[index];
}

/**
 * @brief Get depth texture by slot index (depthtex0-15)
 * @param slot Slot index [0-15]
 * @return Texture2D from ResourceDescriptorHeap
 *
 * [IMPORTANT] uint4 unpacking:
 * - slot >> 2 = which uint4 (0-3)
 * - slot & 3  = which component (x=0, y=1, z=2, w=3)
 */
Texture2D<float> GetDepthTexture(uint slot)
{
    if (slot >= 16) return ResourceDescriptorHeap[0];
    uint4 packed = depthTextureIndicesPacked[slot >> 2];
    uint  index  = packed[slot & 3];
    return ResourceDescriptorHeap[index];
}

/**
 * @brief Get shadow color texture by slot index (shadowcolor0-7)
 * @param slot Slot index [0-7]
 * @return Texture2D from ResourceDescriptorHeap
 *
 * [IMPORTANT] uint4 unpacking:
 * - slot >> 2 = which uint4 (0-1)
 * - slot & 3  = which component (x=0, y=1, z=2, w=3)
 */
Texture2D GetShadowColor(uint slot)
{
    if (slot >= 8) return ResourceDescriptorHeap[0];
    uint4 packed = shadowColorReadIndicesPacked[slot >> 2];
    uint  index  = packed[slot & 3];
    return ResourceDescriptorHeap[index];
}

/**
 * @brief Get shadow depth texture by slot index (shadowtex0-1)
 * @param slot Slot index [0-1]
 * @return Texture2D from ResourceDescriptorHeap
 *
 * [IMPORTANT] uint4 unpacking:
 * - shadowTexIndicesPacked.x = shadowtex0
 * - shadowTexIndicesPacked.y = shadowtex1
 */
Texture2D GetShadowTexture(uint slot)
{
    if (slot == 0) return ResourceDescriptorHeap[shadowTexIndicesPacked.x];
    if (slot == 1) return ResourceDescriptorHeap[shadowTexIndicesPacked.y];
    return ResourceDescriptorHeap[0];
}

//──────────────────────────────────────────────────────────────────────────────
// Iris Compatibility Macros - colortex, depthtex, shadowcolor, shadowtex
//──────────────────────────────────────────────────────────────────────────────

// colortex0-15
#define colortex0  GetColorTexture(0)
#define colortex1  GetColorTexture(1)
#define colortex2  GetColorTexture(2)
#define colortex3  GetColorTexture(3)
#define colortex4  GetColorTexture(4)
#define colortex5  GetColorTexture(5)
#define colortex6  GetColorTexture(6)
#define colortex7  GetColorTexture(7)
#define colortex8  GetColorTexture(8)
#define colortex9  GetColorTexture(9)
#define colortex10 GetColorTexture(10)
#define colortex11 GetColorTexture(11)
#define colortex12 GetColorTexture(12)
#define colortex13 GetColorTexture(13)
#define colortex14 GetColorTexture(14)
#define colortex15 GetColorTexture(15)

// depthtex0-2 (commonly used)
#define depthtex0  GetDepthTexture(0)
#define depthtex1  GetDepthTexture(1)
#define depthtex2  GetDepthTexture(2)

// shadowcolor0-7
#define shadowcolor0 GetShadowColor(0)
#define shadowcolor1 GetShadowColor(1)
#define shadowcolor2 GetShadowColor(2)
#define shadowcolor3 GetShadowColor(3)
#define shadowcolor4 GetShadowColor(4)
#define shadowcolor5 GetShadowColor(5)
#define shadowcolor6 GetShadowColor(6)
#define shadowcolor7 GetShadowColor(7)

// shadowtex0-1
#define shadowtex0 GetShadowTexture(0)
#define shadowtex1 GetShadowTexture(1)


//──────────────────────────────────────────────────────────────────────────────
// [REFACTOR] Dynamic Sampler System - Replaces Static Samplers
//──────────────────────────────────────────────────────────────────────────────
// Static samplers (linearSampler, pointSampler, etc.) have been replaced with
// dynamic samplers via SamplerDescriptorHeap. Include sampler_uniforms.hlsl
// for GetSampler() function and sampler0-3 macros.
//
// Migration: linearSampler -> sampler0, pointSampler -> sampler1, etc.
// Legacy aliases are provided in sampler_uniforms.hlsl for backward compatibility.
//──────────────────────────────────────────────────────────────────────────────
#include "../include/sampler_uniforms.hlsl"

//─────────────────────────────────────────────────
// Fixed Input Layout (vertex format) - reserved for Gbuffers
//─────────────────────────────────────────────────
/**
 * @brief vertex shader input structure
 *
 *Teaching points:
 * 1. Fixed vertex format - all geometry uses the same layout
 * 2. Corresponds to C++ enigma::graphic::Vertex (Vertex_PCUTBN)
 * 3. For Gbuffers Pass (geometry rendering)
 *
 * Note: Composite Pass uses full-screen triangles and does not require complex vertex formats
 */
struct VSInput
{
    float3 Position : POSITION; // Vertex position (world space)
    float4 Color : COLOR0; // Vertex color (R8G8B8A8_UNORM)
    float2 TexCoord : TEXCOORD0; // Texture coordinates
    float3 Tangent : TANGENT; // Tangent vector
    float3 Bitangent : BITANGENT; // Bitangent vector
    float3 Normal : NORMAL; // normal vector
};

/**
 * @brief vertex shader output / pixel shader input
 */
struct VSOutput
{
    float4 Position : SV_POSITION; // Clipping space position
    float4 Color : COLOR0; // Vertex color (after unpacking)
    float2 TexCoord : TEXCOORD0; // Texture coordinates
    float3 Tangent : TANGENT; // Tangent vector
    float3 Bitangent : BITANGENT; // Bitangent vector
    float3 Normal : NORMAL; // normal vector
    float3 WorldPos : TEXCOORD1; // World space position
};

// Pixel shader input (same as VSOutput)
typedef VSOutput PSInput;

/**
 * @brief Unpack Rgba8 color (uint → float4)
 * @param packedColor packed RGBA8 color (0xAABBGGRR)
 * @return unpacked float4 color (0.0-1.0 range)
 */
float4 UnpackRgba8(uint packedColor)
{
    float r = float((packedColor >> 0) & 0xFF) / 255.0;
    float g = float((packedColor >> 8) & 0xFF) / 255.0;
    float b = float((packedColor >> 16) & 0xFF) / 255.0;
    float a = float((packedColor >> 24) & 0xFF) / 255.0;
    return float4(r, g, b, a);
}

/**
 * @brief Standard vertex transformation function (for Fallback Shader)
 * @param input vertex input (VSInput structure)
 * @return VSOutput - transformed vertex output
 *
 * Teaching points:
 * 1. Fallback shader for gbuffers_basic and gbuffers_textured
 * 2. Automatically handle MVP transformation, color unpacking, and data transfer
 * 3. Get the transformation matrix from Uniform Buffers
 * 4. KISS principle - minimalist implementation, no additional calculations
 *
 * Workflow:
 * 1. [NEW] Directly access cbuffer Matrices to obtain the transformation matrix (no function call required)
 * 2. Vertex position transformation: Position → ViewSpace → ClipSpace
 * 3. Normal transformation: normalMatrix transforms the normal vector
 * 4. Color unpacking: uint → float4
 * 5. Pass all vertex attributes to the pixel shader
 */
VSOutput StandardVertexTransform(VSInput input)
{
    VSOutput output;

    // 1. Vertex position transformation
    float4 localPos  = float4(input.Position, 1.0);
    float4 worldPos  = mul(modelMatrix, localPos);
    float4 cameraPos = mul(gbufferView, worldPos);
    float4 renderPos = mul(gbufferRenderer, cameraPos);
    float4 clipPos   = mul(gbufferProjection, renderPos);

    output.Position = clipPos;
    output.WorldPos = worldPos.xyz;
    output.Color    = input.Color;
    output.TexCoord = input.TexCoord;

    // 4. Normal transformation
    output.Normal = normalize(mul(normalMatrix, float4(input.Normal, 0.0)).xyz);

    // 5. Tangents and secondary tangents
    output.Tangent   = normalize(mul(gbufferView, float4(input.Tangent, 0.0)).xyz);
    output.Bitangent = normalize(mul(gbufferView, float4(input.Bitangent, 0.0)).xyz);

    return output;
}

#define PI 3.14159265359
#define TWO_PI 6.28318530718
#define HALF_PI 1.57079632679
