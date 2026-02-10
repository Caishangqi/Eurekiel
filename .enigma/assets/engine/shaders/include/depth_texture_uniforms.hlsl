/**
* @brief DepthTextureIndexUniforms - Depth RT indices (64 bytes)
 * @register b4
 * Stores Bindless indices for depthtex0-15. No flip mechanism.
 * C++ counterpart: DepthTexturesIndexBuffer.hpp
 *
 * [IMPORTANT] HLSL cbuffer packing rule:
 * - Using uint4[4] ensures tight packing matching C++ uint32_t[16] layout
 * - Memory layout: [0-3][4-7][8-11][12-15] = 4 * 16 = 64 bytes
 */
cbuffer DepthTextureIndexUniforms : register(b4)
{
    uint4 depthTextureIndicesPacked[4]; // depthtex0-15 indices (64 bytes)
};

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

#define depthtex0  GetDepthTexture(0)
#define depthtex1  GetDepthTexture(1)
#define depthtex2  GetDepthTexture(2)
