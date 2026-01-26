/**
* @brief ShadowColorIndexUniforms - Shadow color RT indices (64 bytes)
 * @register b5
 * Stores Bindless indices for shadowcolor0-7 with flip support.
 * C++ counterpart: ShadowColorIndexBuffer.hpp
 *
 * [IMPORTANT] HLSL cbuffer packing rule:
 * - Using uint4[2] ensures tight packing matching C++ uint32_t[8] layout
 * - Memory layout: [0-3][4-7] = 2 * 16 = 32 bytes per array
 */
cbuffer ShadowColorIndexUniforms : register(b5)
{
    uint4 shadowColorReadIndicesPacked[2]; // shadowcolor0-7 read indices (32 bytes)
    uint4 shadowColorWriteIndicesPacked[2]; // shadowcolor0-7 write indices (32 bytes)
};

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

// shadowcolor0-7
#define shadowcolor0 GetShadowColor(0)
#define shadowcolor1 GetShadowColor(1)
#define shadowcolor2 GetShadowColor(2)
#define shadowcolor3 GetShadowColor(3)
#define shadowcolor4 GetShadowColor(4)
#define shadowcolor5 GetShadowColor(5)
#define shadowcolor6 GetShadowColor(6)
#define shadowcolor7 GetShadowColor(7)
