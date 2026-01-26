/**
* @brief ColorTextureIndexUniforms - Color RT indices (128 bytes)
 * @register b3
 * Stores Bindless indices for colortex0-15 with flip support.
 * C++ counterpart: ColorTargetsIndexBuffer.hpp
 *
 * [IMPORTANT] HLSL cbuffer packing rule:
 * - Scalar arrays (uint[16]) align each element to 16 bytes (float4 boundary)
 * - Using uint4[4] ensures tight packing matching C++ uint32_t[16] layout
 * - Memory layout: [0-3][4-7][8-11][12-15] = 4 * 16 = 64 bytes per array
 */
cbuffer ColorTextureIndexUniforms : register(b3)
{
    uint4 colorReadIndicesPacked[4]; // colortex0-15 read indices (64 bytes)
    uint4 colorWriteIndicesPacked[4]; // colortex0-15 write indices (64 bytes)
};

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
