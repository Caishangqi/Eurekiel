#pragma once

#include <cstdint>

namespace enigma::graphic
{
    /**
     * @brief ShadowTexturesIndexBuffer - Shadow 深度纹理索引管理（shadowtex0/1）
     *
     * 教学要点:
     * 1. 专门管理 shadowtex0/1 的只读深度纹理索引
     * 2. 不需要 Flip 机制（每帧引擎渲染过程中自然生成）
     * 3. 与 ShadowColorIndexBuffer 分离，职责单一
     * 4. 与 Common.hlsl 中的 ShadowTexturesBuffer 结构体完全对应（16 bytes）
     *
     * Iris架构参考:
     * - ShadowRenderTargets.java: 管理 shadowtex 深度纹理
     * - https://shaders.properties/current/reference/buffers/shadowtex/
     *
     * 设计优势:
     * - 职责单一：只管理只读的 shadowtex 深度纹理
     * - 清晰分离：与 shadowcolor（需要 flip）完全独立
     * - 简单高效：固定 2 个索引，16 bytes 对齐
     *
     * 与 DepthTextures 的相似性:
     * - 都是深度纹理，只读，不需要 flip
     * - 都是引擎渲染过程中自动生成
     * - shadowtex0/1: 阴影贴图的深度
     * - depthtex0/1/2: 主相机的深度
     *
     * Shadow 深度纹理生成时机:
     * 1. shadowtex0: Shadow Pass 完整渲染后（包含所有物体）
     * 2. shadowtex1: Shadow Pass 不透明物体渲染后（不含半透明）
     *
     * 对应HLSL (Common.hlsl):
     * ```hlsl
     * struct ShadowTexturesBuffer {
     *     uint shadowtex0Index;  // 完整阴影深度
     *     uint shadowtex1Index;  // 半透明前阴影深度
     *     uint padding[2];       // 对齐到16字节
     * };
     *
     * // HLSL访问示例
     * StructuredBuffer<ShadowTexturesBuffer> shadowTexBuffer =
     *     ResourceDescriptorHeap[shadowTexturesBufferIndex];
     *
     * // 访问shadowtex0
     * uint shadowtex0Index = shadowTexBuffer[0].shadowtex0Index;
     * Texture2D<float> shadowDepth = ResourceDescriptorHeap[shadowtex0Index];
     * float depth = shadowDepth.Load(int3(shadowPos, 0));
     * ```
     *
     * 使用示例（C++端）:
     * ```cpp
     * ShadowTexturesIndexBuffer shadowTexBuffer;
     *
     * // 设置引擎创建的阴影深度纹理索引
     * shadowTexBuffer.SetShadowTexIndices(
     *     mainShadowDepthIndex,        // shadowtex0: 完整阴影深度
     *     noTranslucentsShadowIndex    // shadowtex1: 半透明前阴影深度
     * );
     *
     * // 上传到GPU
     * d12Buffer->UploadData(&shadowTexBuffer, sizeof(shadowTexBuffer));
     * ```
     *
     * @note 此结构体必须与Common.hlsl中的ShadowTexturesBuffer严格对应（16 bytes）
     * @note 所有阴影深度纹理由引擎 Shadow Pass 自动生成
     */
    struct ShadowTexturesIndexBuffer
    {
        /**
         * @brief shadowtex0索引 - 完整阴影深度缓冲
         *
         * 教学要点:
         * 1. 包含所有物体的阴影深度（不透明 + 半透明）
         * 2. 用于标准阴影采样和PCF软阴影
         * 3. 只读纹理，每帧 Shadow Pass 生成
         *
         * 生成时机:
         * - Shadow Pass 完整渲染后（包含所有物体）
         *
         * HLSL访问:
         * ```hlsl
         * Texture2D<float> GetShadowTex0() {
         *     StructuredBuffer<ShadowTexturesBuffer> shadowTexBuffer =
         *         ResourceDescriptorHeap[shadowTexturesBufferIndex];
         *     return ResourceDescriptorHeap[shadowTexBuffer[0].shadowtex0Index];
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
         * 3. 只读纹理，每帧 Shadow Pass 生成
         *
         * 生成时机:
         * - Shadow Pass 不透明物体渲染后（不含半透明）
         *
         * HLSL访问:
         * ```hlsl
         * Texture2D<float> GetShadowTex1() {
         *     StructuredBuffer<ShadowTexturesBuffer> shadowTexBuffer =
         *         ResourceDescriptorHeap[shadowTexturesBufferIndex];
         *     return ResourceDescriptorHeap[shadowTexBuffer[0].shadowtex1Index];
         * }
         * ```
         */
        uint32_t shadowtex1Index;

        /**
         * @brief 对齐填充 - 确保结构体对齐到16字节边界
         *
         * 教学要点:
         * 1. DirectX 12 要求 StructuredBuffer 对齐到 16 字节
         * 2. 2 个 uint32_t (8 bytes) + padding (8 bytes) = 16 bytes
         */
        uint32_t padding[2];

        // ===== 总计: 4 × 4 = 16 bytes =====

        /**
         * @brief 默认构造函数 - 初始化为0
         */
        ShadowTexturesIndexBuffer()
            : shadowtex0Index(0)
              , shadowtex1Index(0)
              , padding{0, 0}
        {
        }

        // ========== ShadowTex操作（固定索引，无Flip）==========

        /**
         * @brief 批量设置shadowtex索引
         * @param shadowtex0 shadowtex0的Bindless索引
         * @param shadowtex1 shadowtex1的Bindless索引
         *
         * 使用示例:
         * ```cpp
         * shadowTexBuffer.SetShadowTexIndices(mainShadowDepth, noTranslucentsShadow);
         * ```
         */
        void SetShadowTexIndices(uint32_t shadowtex0, uint32_t shadowtex1)
        {
            shadowtex0Index = shadowtex0;
            shadowtex1Index = shadowtex1;
        }

        /**
         * @brief 设置shadowtex0索引
         * @param textureIndex Bindless纹理索引
         */
        void SetShadowTex0(uint32_t textureIndex)
        {
            shadowtex0Index = textureIndex;
        }

        /**
         * @brief 设置shadowtex1索引
         * @param textureIndex Bindless纹理索引
         */
        void SetShadowTex1(uint32_t textureIndex)
        {
            shadowtex1Index = textureIndex;
        }

        /**
         * @brief 获取shadowtex0索引
         * @return Bindless纹理索引
         */
        uint32_t GetShadowTex0() const
        {
            return shadowtex0Index;
        }

        /**
         * @brief 获取shadowtex1索引
         * @return Bindless纹理索引
         */
        uint32_t GetShadowTex1() const
        {
            return shadowtex1Index;
        }

        // ========== 验证和工具方法 ==========

        /**
         * @brief 验证shadowtex有效性
         * @return true 如果shadowtex0有效（shadowtex1可选）
         *
         * 教学要点:
         * 1. shadowtex0 是必需的（完整阴影深度）
         * 2. shadowtex1 是可选的（半透明前阴影深度）
         * 3. 索引为0表示未初始化
         */
        bool IsValid() const
        {
            return shadowtex0Index != 0;
        }

        /**
         * @brief 验证两个shadowtex都有效
         * @return true 如果shadowtex0和shadowtex1都不为0
         */
        bool HasBothTextures() const
        {
            return shadowtex0Index != 0 && shadowtex1Index != 0;
        }

        /**
         * @brief 重置所有索引
         *
         * 教学要点:
         * 1. 用于帧结束后清理资源引用
         * 2. 或用于 Shader Pack 切换时的状态重置
         */
        void Reset()
        {
            shadowtex0Index = 0;
            shadowtex1Index = 0;
            padding[0]      = 0;
            padding[1]      = 0;
        }
    };

    // 编译期验证: 确保结构体大小为16字节
    static_assert(sizeof(ShadowTexturesIndexBuffer) == 16,
                  "ShadowTexturesIndexBuffer must be exactly 16 bytes to match HLSL ShadowTexturesBuffer struct");

    // 编译期验证: 确保对齐到16字节（GPU要求）
    static_assert(sizeof(ShadowTexturesIndexBuffer) % 16 == 0,
                  "ShadowTexturesIndexBuffer must be aligned to 16 bytes for GPU upload");

    // 编译期验证: 确保对齐
    static_assert(alignof(ShadowTexturesIndexBuffer) == 4,
                  "ShadowTexturesIndexBuffer must be 4-byte aligned for GPU upload");
} // namespace enigma::graphic
