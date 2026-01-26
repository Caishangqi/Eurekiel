/**
* @brief ShadowTexturesIndexUniforms - Shadow depth RT indices (16 bytes)
 * @register b6
 * Stores Bindless indices for shadowtex0-1. No flip mechanism.
 * C++ counterpart: ShadowTexturesIndexBuffer.hpp
 *
 * [IMPORTANT] HLSL cbuffer packing rule:
 * - Using uint4 ensures tight packing matching C++ uint32_t[4] layout
 * - Memory layout: [shadowtex0, shadowtex1, pad, pad] = 16 bytes
 */
cbuffer ShadowTexturesIndexUniforms : register(b6)
{
    uint4 shadowTexIndicesPacked; // [shadowtex0, shadowtex1, pad, pad] (16 bytes)
};

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


// shadowtex0-1
#define shadowtex0 GetShadowTexture(0)
#define shadowtex1 GetShadowTexture(1)
