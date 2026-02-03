cbuffer ShadowTexturesIndexUniforms : register(b6)
{
    uint4 shadowTexIndicesPacked[4];
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
    if (slot >= 16) return ResourceDescriptorHeap[0];
    uint4 packed = shadowTexIndicesPacked[slot >> 2];
    uint  index  = packed[slot & 3];
    return ResourceDescriptorHeap[index];
}


// shadowtex0-1
#define shadowtex0 GetShadowTexture(0)
#define shadowtex1 GetShadowTexture(1)
