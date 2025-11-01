#pragma once

#include <cstdint>

namespace enigma::graphic
{
    /**
     * @brief ShadowColorIndexBuffer - Shadow Color 纹理索引管理（shadowcolor0-7）
     *
     * 教学要点:
     * 1. 专门管理 shadowcolor0-7 的 Main/Alt 双缓冲索引
     * 2. 支持 Ping-Pong Flip 机制，消除 ResourceBarrier 开销
     * 3. 与 ShadowTexturesIndexBuffer 分离，职责单一
     * 4. 与 Common.hlsl 中的 ShadowColorBuffer 结构体完全对应（64 bytes）
     *
     * Iris架构参考:
     * - ShadowRenderTargets.java: 管理 shadowcolor 纹理
     * - https://shaders.properties/current/reference/buffers/shadowcolor/
     *
     * 设计优势:
     * - 职责单一：只管理需要 flip 的 shadowcolor 纹理
     * - 清晰分离：与 shadowtex（只读深度）完全独立
     * - Ping-Pong 优化：通过索引切换避免 GPU 同步等待
     *
     * 对应HLSL (Common.hlsl):
     * ```hlsl
     * struct ShadowColorBuffer {
     *     // Shadow Color Targets (需要flip) - 64 bytes
     *     uint shadowColorReadIndices[8];   // shadowcolor0-7读取索引
     *     uint shadowColorWriteIndices[8];  // shadowcolor0-7写入索引（预留）
     * };
     *
     * // HLSL访问示例
     * StructuredBuffer<ShadowColorBuffer> shadowColorBuffer =
     *     ResourceDescriptorHeap[shadowColorBufferIndex];
     *
     * // 访问shadowcolor0
     * uint colorIndex = shadowColorBuffer[0].shadowColorReadIndices[0];
     * Texture2D color = ResourceDescriptorHeap[colorIndex];
     * ```
     *
     * 使用示例（C++端）:
     * ```cpp
     * ShadowColorIndexBuffer shadowColorBuffer;
     *
     * // 设置shadowcolor（需要flip）
     * shadowColorBuffer.Flip(mainShadowColorIndices, altShadowColorIndices, false);
     *
     * // 上传到GPU
     * d12Buffer->UploadData(&shadowColorBuffer, sizeof(shadowColorBuffer));
     * ```
     *
     * @note 此结构体必须与Common.hlsl中的ShadowColorBuffer严格对应（64 bytes）
     */
    struct ShadowColorIndexBuffer
    {
        // ========== Shadow Color Targets（需要Flip）==========

        /**
         * @brief shadowcolor读取索引数组 - shadowcolor0-7
         *
         * 教学要点:
         * 1. 存储8个shadowcolor的当前读取索引
         * 2. 根据flip状态指向Main或Alt纹理
         * 3. 支持多Pass阴影渲染（shadowcomp阶段）
         *
         * flip状态管理:
         * - flip = false: shadowColorReadIndices[i] = mainIndices[i]
         * - flip = true:  shadowColorReadIndices[i] = altIndices[i]
         *
         * HLSL访问:
         * ```hlsl
         * Texture2D GetShadowColor(uint shadowIndex) {
         *     StructuredBuffer<ShadowColorBuffer> shadowColorBuffer =
         *         ResourceDescriptorHeap[shadowColorBufferIndex];
         *     uint textureIndex = shadowColorBuffer[0].shadowColorReadIndices[shadowIndex];
         *     return ResourceDescriptorHeap[textureIndex];
         * }
         * ```
         */
        uint32_t shadowColorReadIndices[8];

        /**
         * @brief shadowcolor写入索引数组（预留UAV扩展）
         *
         * 教学要点:
         * 1. 预留给UAV扩展
         * 2. 当前RTV直接绑定，无需此索引
         * 3. 未来可用于Compute Shader写入shadowcolor
         *
         * flip状态管理（如果使用）:
         * - flip = false: shadowColorWriteIndices[i] = altIndices[i]
         * - flip = true:  shadowColorWriteIndices[i] = mainIndices[i]
         */
        uint32_t shadowColorWriteIndices[8];

        // ===== 总计: (8 + 8) × 4 = 64 bytes =====

        /**
         * @brief 默认构造函数 - 初始化为0
         */
        ShadowColorIndexBuffer()
        {
            for (int i = 0; i < 8; ++i)
            {
                shadowColorReadIndices[i]  = 0;
                shadowColorWriteIndices[i] = 0;
            }
        }

        // ========== ShadowColor操作（Flip机制）==========

        /**
         * @brief 设置所有shadowColor readIndices
         * @param indices 索引数组（至少8个元素）
         */
        void SetShadowColorReadIndices(const uint32_t indices[8])
        {
            for (int i = 0; i < 8; ++i)
            {
                shadowColorReadIndices[i] = indices[i];
            }
        }

        /**
         * @brief 设置所有shadowColor writeIndices
         * @param indices 索引数组（至少8个元素）
         */
        void SetShadowColorWriteIndices(const uint32_t indices[8])
        {
            for (int i = 0; i < 8; ++i)
            {
                shadowColorWriteIndices[i] = indices[i];
            }
        }

        /**
         * @brief Flip操作 - 交换shadowcolor的Main和Alt索引
         * @param mainIndices Main纹理索引数组（shadowcolor）
         * @param altIndices Alt纹理索引数组（shadowcolor）
         * @param useAlt true=读Alt写Main, false=读Main写Alt
         *
         * 教学要点:
         * 1. Ping-Pong核心机制，实现双缓冲切换
         * 2. 消除shadowcolor的ResourceBarrier需求
         * 3. 提升渲染性能，避免GPU同步等待
         */
        void Flip(const uint32_t mainIndices[8],
                  const uint32_t altIndices[8],
                  bool           useAlt)
        {
            if (useAlt)
            {
                // 读Alt, 写Main
                SetShadowColorReadIndices(altIndices);
                SetShadowColorWriteIndices(mainIndices);
            }
            else
            {
                // 读Main, 写Alt
                SetShadowColorReadIndices(mainIndices);
                SetShadowColorWriteIndices(altIndices);
            }
        }

        // ========== 验证和工具方法 ==========

        /**
         * @brief 验证shadowcolor有效性
         * @return true 如果至少有一个shadowcolor被设置
         */
        bool HasValidShadowColor() const
        {
            for (int i = 0; i < 8; ++i)
            {
                if (shadowColorReadIndices[i] != 0)
                {
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief 验证整体有效性
         * @return true 如果至少有一个shadowcolor被设置
         */
        bool IsValid() const
        {
            return HasValidShadowColor();
        }

        /**
         * @brief 获取使用的shadowcolor数量
         */
        uint32_t GetActiveShadowColorCount() const
        {
            uint32_t count = 0;
            for (int i = 0; i < 8; ++i)
            {
                if (shadowColorReadIndices[i] != 0)
                {
                    ++count;
                }
            }
            return count;
        }

        /**
         * @brief 重置所有索引
         */
        void Reset()
        {
            for (int i = 0; i < 8; ++i)
            {
                shadowColorReadIndices[i]  = 0;
                shadowColorWriteIndices[i] = 0;
            }
        }
    };

    // 编译期验证: 确保结构体大小为64字节
    static_assert(sizeof(ShadowColorIndexBuffer) == 64,
                  "ShadowColorIndexBuffer must be exactly 64 bytes to match HLSL ShadowColorBuffer struct");

    // 编译期验证: 确保对齐
    static_assert(alignof(ShadowColorIndexBuffer) == 4,
                  "ShadowColorIndexBuffer must be 4-byte aligned for GPU upload");
} // namespace enigma::graphic
