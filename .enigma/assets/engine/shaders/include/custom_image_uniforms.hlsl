/**
 * @brief CustomImageUniforms - Custom material slot constant buffer (64 bytes)
 *
 * [IMPORTANT] HLSL cbuffer packing rule:
 * - Using uint4[4] ensures tight packing matching C++ uint32_t[16] layout
 * - Memory layout: [0-3][4-7][8-11][12-15] = 4 * 16 = 64 bytes
 * - A plain uint[16] would pad each element to 16 bytes = 256 bytes total!
 *
 * Architectural design:
 * - Structure size: 64 bytes (4 x uint4)
 * - PerObject frequency, supports independent materials for each object
 * - Root CBV binding, zero overhead
 * - Compatible with Iris style (customImage0-15 macro)
 *
 * Corresponding to C++: CustomImageUniforms.hpp
 */
cbuffer CustomImageUniforms: register(b2)
{
    uint4 customImageIndicesPacked[4]; // customImage0-15 Bindless indices (64 bytes)
};

/**
 * @brief Get the Bindless index of the custom material slot
 * @param slotIndex slot index (0-15)
 * @return Bindless index, if the slot is not set, return UINT32_MAX (0xFFFFFFFF)
 *
 * [IMPORTANT] uint4 unpacking (same pattern as depth_texture_uniforms.hlsl):
 * - slot >> 2 = which uint4 (0-3)
 * - slot & 3  = which component (x=0, y=1, z=2, w=3)
 */
uint GetCustomImageIndex(uint slotIndex)
{
    if (slotIndex >= 16)
    {
        return 0xFFFFFFFF;
    }
    uint4 packed = customImageIndicesPacked[slotIndex >> 2];
    return packed[slotIndex & 3];
}

/**
 * @brief Get custom material texture (convenient access function)
 * @param slotIndex slot index (0-15)
 * @return corresponding Bindless texture (Texture2D)
 */
Texture2D GetCustomImage(uint slotIndex)
{
    uint bindlessIndex = GetCustomImageIndex(slotIndex);
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
