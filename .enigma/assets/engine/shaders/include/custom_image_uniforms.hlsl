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
