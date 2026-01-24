#pragma once

#include <cstdint>
#include <stdexcept>

namespace enigma::graphic
{
    /**
     * @brief CustomImageUniform - 自定义材质槽位索引管理
     *
     * 教学要点:
     * 1. 支持16个自定义材质槽位（customImage0-15）
     * 2. 允许用户上传任意纹理到指定槽位
     * 3. 运行时动态更新，无需Flip机制
     * 4. 与Common.hlsl中的CustomImageBuffer结构体完全对应
     *
     * 架构设计:
     * 1. 16个槽位固定分配（64字节）
     * 2. 每个槽位存储一个Bindless纹理索引
     * 3. 未使用的槽位索引为0（无效索引）
     * 4. 支持运行时动态更新任意槽位
     *
     * 对应HLSL (Common.hlsl):
     * ```hlsl
     * struct CustomImageBuffer {
     *     uint customImageIndices[16];  // customImage0-15的Bindless索引
     * };
     *
     * // HLSL访问示例
     * StructuredBuffer<CustomImageBuffer> customImageBuffer =
     *     ResourceDescriptorHeap[customImageBufferIndex];
     * 
     * uint customImage0Index = customImageBuffer[0].customImageIndices[0];
     * Texture2D customTex = ResourceDescriptorHeap[customImage0Index];
     * float4 color = customTex.Sample(sampler0, uv);
     * ```
     *
     * 使用示例（C++端）:
     * ```cpp
     * CustomImageUniforms customBuffer;
     *
     * // 设置自定义材质槽位
     * customBuffer.SetCustomImageIndex(0, myTextureIndex);  // customImage0
     * customBuffer.SetCustomImageIndex(1, anotherIndex);    // customImage1
     *
     * // 批量设置
     * uint32_t indices[16] = { ... };
     * customBuffer.SetCustomImageIndices(indices);
     *
     * // 上传到GPU
     * d12Buffer->UploadData(&customBuffer, sizeof(customBuffer));
     * ```
     *
     * @note 此结构体必须与Common.hlsl中的CustomImageBuffer严格对应（64 bytes）
     */
    struct CustomImageUniforms
    {
        /**
         * @brief 自定义材质索引数组 - customImage0-15
         *
         * 索引语义:
         * - customImageIndices[0] = customImage0
         * - customImageIndices[1] = customImage1
         * - ...
         * - customImageIndices[15] = customImage15
         *
         * 教学要点:
         * 1. 每个槽位独立管理，互不影响
         * 2. 索引为0表示未使用（无效索引）
         * 3. 支持任意时刻更新任意槽位
         * 4. 无需Flip机制（与RenderTargets不同）
         *
         * HLSL访问示例:
         * ```hlsl
         * // 通过Common.hlsl宏访问
         * #define customImage0 GetCustomImage(0)
         * #define customImage1 GetCustomImage(1)
         * // ... customImage2-15
         * 
         * Texture2D GetCustomImage(uint slotIndex) {
         *     StructuredBuffer<CustomImageBuffer> buffer =
         *         ResourceDescriptorHeap[customImageBufferIndex];
         *     uint textureIndex = buffer[0].customImageIndices[slotIndex];
         *     return ResourceDescriptorHeap[textureIndex];
         * }
         * ```
         */
        uint32_t customImageIndices[16];

        // ===== 总计: 16 × 4 = 64 bytes =====

        /**
         * @brief 默认构造函数 - 初始化所有索引为0
         */
        CustomImageUniforms()
        {
            for (int i = 0; i < 16; ++i)
            {
                customImageIndices[i] = 0;
            }
        }

        /**
         * @brief 设置指定槽位的纹理索引
         * @param slotIndex 槽位索引 [0-15]
         * @param textureIndex Bindless纹理索引
         *
         * 教学要点:
         * - 运行时动态更新
         * - 支持任意槽位独立设置
         * - 范围检查确保安全
         *
         * 使用示例:
         * ```cpp
         * customBuffer.SetCustomImageIndex(0, myTextureIndex);
         * ```
         */
        void SetCustomImageIndex(int slotIndex, uint32_t textureIndex)
        {
            if (slotIndex < 0 || slotIndex >= 16)
            {
                throw std::out_of_range("Custom image slot index must be in range [0-15]");
            }
            customImageIndices[slotIndex] = textureIndex;
        }

        /**
         * @brief 获取指定槽位的纹理索引
         * @param slotIndex 槽位索引 [0-15]
         * @return Bindless纹理索引
         */
        uint32_t GetCustomImageIndex(int slotIndex) const
        {
            if (slotIndex < 0 || slotIndex >= 16)
            {
                throw std::out_of_range("Custom image slot index must be in range [0-15]");
            }
            return customImageIndices[slotIndex];
        }

        /**
         * @brief 批量设置所有槽位的纹理索引
         * @param indices 索引数组（至少16个元素）
         *
         * 使用示例:
         * ```cpp
         * uint32_t indices[16] = { index0, index1, ..., index15 };
         * customBuffer.SetCustomImageIndices(indices);
         * ```
         */
        void SetCustomImageIndices(const uint32_t indices[16])
        {
            for (int i = 0; i < 16; ++i)
            {
                customImageIndices[i] = indices[i];
            }
        }

        /**
         * @brief 检查指定槽位是否已设置
         * @param slotIndex 槽位索引 [0-15]
         * @return true 如果槽位索引不为0
         *
         * 教学要点:
         * - 索引0表示未初始化
         * - 用于运行时验证槽位状态
         */
        bool IsSlotValid(int slotIndex) const
        {
            if (slotIndex < 0 || slotIndex >= 16)
            {
                return false;
            }
            return customImageIndices[slotIndex] != 0;
        }

        /**
         * @brief 获取已设置的槽位数量
         * @return 非零索引的槽位数量
         *
         * 使用示例:
         * ```cpp
         * int usedSlots = customBuffer.GetUsedSlotCount();
         * ```
         */
        int GetUsedSlotCount() const
        {
            int count = 0;
            for (int i = 0; i < 16; ++i)
            {
                if (customImageIndices[i] != 0)
                {
                    count++;
                }
            }
            return count;
        }

        /**
         * @brief 重置所有槽位
         *
         * 教学要点:
         * - 用于场景切换或资源清理
         * - 将所有索引设置为0（无效）
         *
         * 使用示例:
         * ```cpp
         * customBuffer.Reset();  // 清空所有自定义材质
         * ```
         */
        void Reset()
        {
            for (int i = 0; i < 16; ++i)
            {
                customImageIndices[i] = 0;
            }
        }

        /**
         * @brief 重置单个槽位
         * @param slotIndex 槽位索引 [0-15]
         *
         * 使用示例:
         * ```cpp
         * customBuffer.ResetSlot(0);  // 清空customImage0
         * ```
         */
        void ResetSlot(int slotIndex)
        {
            if (slotIndex >= 0 && slotIndex < 16)
            {
                customImageIndices[slotIndex] = 0;
            }
        }
    };

    // 编译期验证: 确保结构体大小为64字节（16个uint32_t）
    static_assert(sizeof(CustomImageUniforms) == 64,
                  "CustomImageUniforms must be exactly 64 bytes (16 x 4 bytes) to match HLSL CustomImageBuffer struct");

    // 编译期验证: 确保对齐到16字节（GPU要求）
    static_assert(sizeof(CustomImageUniforms) % 16 == 0,
                  "CustomImageUniforms must be aligned to 16 bytes for GPU upload");

    // 编译期验证: 确保数组元素对齐
    static_assert(alignof(CustomImageUniforms) == 4,
                  "CustomImageUniforms must be 4-byte aligned (uint32_t alignment)");
} // namespace enigma::graphic
