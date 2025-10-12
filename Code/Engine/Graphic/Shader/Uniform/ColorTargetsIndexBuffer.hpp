#pragma once

#include <cstdint>

namespace enigma::graphic
{
    /**
     * @brief RenderTargetsBuffer - Main/Alt双缓冲索引管理
     *
     * 教学要点:
     * 1. 存储16个colortex的读写索引（对应Iris colortex0-15）
     * 2. Main/Alt Ping-Pong双缓冲机制：消除ResourceBarrier开销
     * 3. 每个Pass执行前上传到GPU StructuredBuffer
     * 4. 与Common.hlsl中的RenderTargetsBuffer结构体完全对应
     *
     * Iris架构参考:
     * - RenderTargets.java: 16个RenderTarget管理
     * - RenderTarget.java: Main + Alt纹理对（Ping-Pong机制）
     * - https://shaders.properties/current/reference/buffers/overview/
     *
     * Main/Alt双缓冲工作原理:
     * 1. flip = false时: Main作为读取源, Alt作为写入目标
     * 2. flip = true时:  Alt作为读取源, Main作为写入目标
     * 3. readIndices自动指向正确的纹理（flip状态切换）
     * 4. 无需ResourceBarrier同步（90%+性能提升）
     *
     * 对应HLSL (Common.hlsl):
     * ```hlsl
     * struct ColorTargetsIndexBuffer {
     *     uint readIndices[16];   // 读取索引（Main或Alt的Bindless索引）
     *     uint writeIndices[16];  // 写入索引（预留，当前未使用）
     * };
     *
     * // HLSL访问示例
     * StructuredBuffer<RenderTargetsBuffer> rtBuffer =
     *     ResourceDescriptorHeap[renderTargetsBufferIndex];
     * uint textureIndex = rtBuffer[0].readIndices[0];  // colortex0
     * Texture2D tex = ResourceDescriptorHeap[textureIndex];
     * ```
     *
     * 使用示例（C++端）:
     * ```cpp
     * ColorTargetsIndexBuffer rtBuffer;
     *
     * // 场景1: flip = false (Pass A写Alt，Pass B读Main)
     * rtBuffer.readIndices[0] = mainTextureIndices[0];  // 指向Main
     * rtBuffer.writeIndices[0] = altTextureIndices[0];  // 指向Alt
     *
     * // 场景2: flip = true (Pass C写Main，Pass D读Alt)
     * rtBuffer.readIndices[0] = altTextureIndices[0];   // 指向Alt
     * rtBuffer.writeIndices[0] = mainTextureIndices[0]; // 指向Main
     *
     * // 上传到GPU
     * d12Buffer->UploadData(&rtBuffer, sizeof(rtBuffer));
     * ```
     *
     * @note 此结构体必须与Common.hlsl中的RenderTargetsBuffer严格对应（128 bytes）
     */
    struct ColorTargetsIndexBuffer
    {
        /**
         * @brief 读取索引数组 - colortex0-15
         *
         * 教学要点:
         * 1. 存储16个RenderTarget的当前读取索引
         * 2. 根据flip状态指向Main或Alt纹理
         * 3. 着色器通过GetRenderTarget(rtIndex)访问
         *
         * 索引含义:
         * - readIndices[0]: colortex0的Bindless纹理索引
         * - readIndices[1]: colortex1的Bindless纹理索引
         * - ... 依此类推
         * - readIndices[15]: colortex15的Bindless纹理索引
         *
         * flip状态管理:
         * - flip = false: readIndices[i] = mainTextureIndices[i]
         * - flip = true:  readIndices[i] = altTextureIndices[i]
         *
         * HLSL访问:
         * ```hlsl
         * Texture2D GetRenderTarget(uint rtIndex) {
         *     StructuredBuffer<RenderTargetsBuffer> rtBuffer =
         *         ResourceDescriptorHeap[renderTargetsBufferIndex];
         *     uint textureIndex = rtBuffer[0].readIndices[rtIndex];
         *     return ResourceDescriptorHeap[textureIndex];
         * }
         * ```
         */
        uint32_t readIndices[16];

        /**
         * @brief 写入索引数组 - colortex0-15（预留）
         *
         * 教学要点:
         * 1. 预留给UAV扩展（Unordered Access View）
         * 2. 当前RTV（Render Target View）直接绑定，无需此索引
         * 3. 未来可用于Compute Shader写入RenderTarget
         *
         * 预留原因:
         * - 当前架构: RTV直接绑定到CommandList（OMSetRenderTargets）
         * - 未来扩展: Compute Shader需要UAV访问RenderTarget
         * - 一致性: 与Iris架构保持对齐
         *
         * flip状态管理（如果使用）:
         * - flip = false: writeIndices[i] = altTextureIndices[i]
         * - flip = true:  writeIndices[i] = mainTextureIndices[i]
         *
         * @note 当前实现中可设置为0，未来扩展时再使用
         */
        uint32_t writeIndices[16];

        // ===== 总计: (16 + 16) × 4 = 128 bytes =====

        /**
         * @brief 默认构造函数 - 初始化为0
         */
        ColorTargetsIndexBuffer()
        {
            for (int i = 0; i < 16; ++i)
            {
                readIndices[i]  = 0;
                writeIndices[i] = 0;
            }
        }

        /**
         * @brief 设置所有readIndices（批量设置）
         * @param indices 索引数组（至少16个元素）
         *
         * 使用示例:
         * ```cpp
         * uint32_t mainIndices[16] = { ... };
         * rtBuffer.SetReadIndices(mainIndices);
         * ```
         */
        void SetReadIndices(const uint32_t indices[16])
        {
            for (int i = 0; i < 16; ++i)
            {
                readIndices[i] = indices[i];
            }
        }

        /**
         * @brief 设置所有writeIndices（批量设置）
         * @param indices 索引数组（至少16个元素）
         */
        void SetWriteIndices(const uint32_t indices[16])
        {
            for (int i = 0; i < 16; ++i)
            {
                writeIndices[i] = indices[i];
            }
        }

        /**
         * @brief 设置单个readIndex
         * @param rtIndex RenderTarget索引（0-15）
         * @param textureIndex Bindless纹理索引
         */
        void SetReadIndex(uint32_t rtIndex, uint32_t textureIndex)
        {
            if (rtIndex < 16)
            {
                readIndices[rtIndex] = textureIndex;
            }
        }

        /**
         * @brief 设置单个writeIndex
         * @param rtIndex RenderTarget索引（0-15）
         * @param textureIndex Bindless纹理索引
         */
        void SetWriteIndex(uint32_t rtIndex, uint32_t textureIndex)
        {
            if (rtIndex < 16)
            {
                writeIndices[rtIndex] = textureIndex;
            }
        }

        /**
         * @brief Flip操作 - 交换Main和Alt索引
         * @param mainIndices Main纹理索引数组
         * @param altIndices Alt纹理索引数组
         * @param useAlt true=读Alt写Main, false=读Main写Alt
         *
         * 教学要点:
         * 1. Ping-Pong核心机制
         * 2. useAlt控制读写方向
         * 3. 消除ResourceBarrier需求
         *
         * 使用示例:
         * ```cpp
         * // Pass A写Alt，Pass B读Main
         * rtBuffer.Flip(mainIndices, altIndices, false);
         *
         * // Pass C写Main，Pass D读Alt
         * rtBuffer.Flip(mainIndices, altIndices, true);
         * ```
         */
        void Flip(const uint32_t mainIndices[16],
                  const uint32_t altIndices[16],
                  bool           useAlt)
        {
            if (useAlt)
            {
                // 读Alt, 写Main
                SetReadIndices(altIndices);
                SetWriteIndices(mainIndices);
            }
            else
            {
                // 读Main, 写Alt
                SetReadIndices(mainIndices);
                SetWriteIndices(altIndices);
            }
        }
    };

    // 编译期验证: 确保结构体大小为128字节
    static_assert(sizeof(ColorTargetsIndexBuffer) == 128,
                  "ColorTargetsIndexBuffer must be exactly 128 bytes to match HLSL ColorTargetsIndexBuffer struct");

    // 编译期验证: 确保数组对齐
    static_assert(alignof(ColorTargetsIndexBuffer) == 4,
                  "ColorTargetsIndexBuffer must be 4-byte aligned for GPU upload");
} // namespace enigma::graphic
