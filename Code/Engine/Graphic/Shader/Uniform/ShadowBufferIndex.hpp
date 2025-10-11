#pragma once

#include <cstdint>

namespace enigma::graphic
{
    /**
     * @brief ShadowBufferIndex - 合并的Shadow系统索引管理（shadowcolor + shadowtex）
     *
     * 教学要点:
     * 1. 合并所有Shadow相关纹理到单一Buffer（激进方案）
     * 2. shadowcolor0-7: Main/Alt双缓冲，需要flip机制
     * 3. shadowtex0/1: 深度纹理，只读，不需要flip
     * 4. 与Common.hlsl中的ShadowBuffer结构体完全对应（80 bytes）
     *
     * Iris架构参考:
     * - ShadowRenderTargets.java: 统一管理shadowcolor + shadowtex
     * - https://shaders.properties/current/reference/buffers/shadowcolor/
     * - https://shaders.properties/current/reference/buffers/shadowtex/
     *
     * 合并设计优势:
     * - 极简Root Constants：只需1个索引（4 bytes）指向完整Shadow系统
     * - 统一管理：Shadow相关的所有资源在一个Buffer
     * - 清晰分离：flip部分（shadowcolor）vs 固定部分（shadowtex）
     *
     * 对应HLSL (Common.hlsl):
     * ```hlsl
     * struct ShadowBuffer {
     *     // Shadow Color Targets (需要flip) - 64 bytes
     *     uint shadowColorReadIndices[8];   // shadowcolor0-7读取索引
     *     uint shadowColorWriteIndices[8];  // shadowcolor0-7写入索引（预留）
     *
     *     // Shadow Depth Textures (不需要flip) - 16 bytes
     *     uint shadowtex0Index;             // 完整阴影深度
     *     uint shadowtex1Index;             // 半透明前阴影深度
     *     uint padding[2];                  // 对齐到16字节
     * };
     *
     * // HLSL访问示例
     * StructuredBuffer<ShadowBuffer> shadowBuffer =
     *     ResourceDescriptorHeap[shadowBufferIndex];
     *
     * // 访问shadowcolor0
     * uint colorIndex = shadowBuffer[0].shadowColorReadIndices[0];
     * Texture2D color = ResourceDescriptorHeap[colorIndex];
     *
     * // 访问shadowtex0
     * uint depthIndex = shadowBuffer[0].shadowtex0Index;
     * Texture2D<float> depth = ResourceDescriptorHeap[depthIndex];
     * ```
     *
     * 使用示例（C++端）:
     * ```cpp
     * ShadowBufferIndex shadowBuffer;
     *
     * // 设置shadowcolor（需要flip）
     * shadowBuffer.Flip(mainShadowColorIndices, altShadowColorIndices, false);
     *
     * // 设置shadowtex（固定，不flip）
     * shadowBuffer.shadowtex0Index = mainShadowDepthIndex;
     * shadowBuffer.shadowtex1Index = noTranslucentsShadowIndex;
     *
     * // 上传到GPU
     * d12Buffer->UploadData(&shadowBuffer, sizeof(shadowBuffer));
     * ```
     *
     * @note 此结构体必须与Common.hlsl中的ShadowBuffer严格对应（80 bytes）
     */
    struct ShadowBufferIndex
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
         *     StructuredBuffer<ShadowBuffer> shadowBuffer =
         *         ResourceDescriptorHeap[shadowBufferIndex];
         *     uint textureIndex = shadowBuffer[0].shadowColorReadIndices[shadowIndex];
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

        // ========== Shadow Depth Textures（不需要Flip）==========

        /**
         * @brief shadowtex0索引 - 完整阴影深度缓冲
         *
         * 教学要点:
         * 1. 包含所有物体的阴影深度（不透明 + 半透明）
         * 2. 用于标准阴影采样和PCF软阴影
         * 3. 只读纹理，不需要flip
         *
         * HLSL访问:
         * ```hlsl
         * Texture2D<float> GetShadowTex0() {
         *     StructuredBuffer<ShadowBuffer> shadowBuffer =
         *         ResourceDescriptorHeap[shadowBufferIndex];
         *     return ResourceDescriptorHeap[shadowBuffer[0].shadowtex0Index];
         * }
         * ```
         */
        uint32_t shadowtex0Index;

        /**
         * @brief shadowtex1索引 - 半透明物体前的阴影深度缓冲
         *
         * 教学要点:
         * 1. 只包含不透明物体的阴影深度
         * 2. 用于半透明物体的阴影处理
         * 3. 只读纹理，不需要flip
         *
         * HLSL访问:
         * ```hlsl
         * Texture2D<float> GetShadowTex1() {
         *     StructuredBuffer<ShadowBuffer> shadowBuffer =
         *         ResourceDescriptorHeap[shadowBufferIndex];
         *     return ResourceDescriptorHeap[shadowBuffer[0].shadowtex1Index];
         * }
         * ```
         */
        uint32_t shadowtex1Index;

        /**
         * @brief 对齐填充 - 确保结构体对齐到16字节边界
         */
        uint32_t padding[2];

        // ===== 总计: (8 + 8) × 4 + 4 × 4 = 64 + 16 = 80 bytes =====

        /**
         * @brief 默认构造函数 - 初始化为0
         */
        ShadowBufferIndex()
            : shadowtex0Index(0)
              , shadowtex1Index(0)
              , padding{0, 0}
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
         * 1. Ping-Pong核心机制，只影响shadowcolor
         * 2. shadowtex0/1不受影响（固定只读）
         * 3. 消除shadowcolor的ResourceBarrier需求
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

        // ========== ShadowTex操作（固定索引）==========

        /**
         * @brief 批量设置shadowtex索引
         * @param shadowtex0 shadowtex0的Bindless索引
         * @param shadowtex1 shadowtex1的Bindless索引
         */
        void SetShadowTexIndices(uint32_t shadowtex0, uint32_t shadowtex1)
        {
            shadowtex0Index = shadowtex0;
            shadowtex1Index = shadowtex1;
        }

        /**
         * @brief 设置shadowtex0索引
         */
        void SetShadowTex0(uint32_t textureIndex)
        {
            shadowtex0Index = textureIndex;
        }

        /**
         * @brief 设置shadowtex1索引
         */
        void SetShadowTex1(uint32_t textureIndex)
        {
            shadowtex1Index = textureIndex;
        }

        /**
         * @brief 获取shadowtex0索引
         */
        uint32_t GetShadowTex0() const
        {
            return shadowtex0Index;
        }

        /**
         * @brief 获取shadowtex1索引
         */
        uint32_t GetShadowTex1() const
        {
            return shadowtex1Index;
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
         * @brief 验证shadowtex有效性
         * @return true 如果shadowtex0有效（shadowtex1可选）
         */
        bool HasValidShadowTex() const
        {
            return shadowtex0Index != 0;
        }

        /**
         * @brief 验证整体有效性
         * @return true 如果Shadow系统至少部分配置
         */
        bool IsValid() const
        {
            return HasValidShadowColor() || HasValidShadowTex();
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
            shadowtex0Index = 0;
            shadowtex1Index = 0;
            padding[0]      = 0;
            padding[1]      = 0;
        }
    };

    // 编译期验证: 确保结构体大小为80字节
    static_assert(sizeof(ShadowBufferIndex) == 80,
                  "ShadowBufferIndex must be exactly 80 bytes to match HLSL ShadowBuffer struct");

    // 编译期验证: 确保对齐
    static_assert(alignof(ShadowBufferIndex) == 4,
                  "ShadowBufferIndex must be 4-byte aligned for GPU upload");
} // namespace enigma::graphic
