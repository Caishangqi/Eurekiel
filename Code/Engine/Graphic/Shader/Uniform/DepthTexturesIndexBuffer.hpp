#pragma once

#include <cstdint>

namespace enigma::graphic
{
    /**
     * @brief DepthTexturesIndexBuffer - depthtex0/1/2索引管理（引擎自有深度纹理）
     *
     * 教学要点:
     * 1. 存储3个引擎创建的深度纹理索引（depthtex0, depthtex1, depthtex2）
     * 2. 不需要Main/Alt双缓冲机制（depth textures是引擎渲染过程中自然产生的）
     * 3. 每个Pass执行前上传到GPU StructuredBuffer
     * 4. 与Common.hlsl中的DepthTexturesBuffer结构体完全对应
     *
     * Iris架构参考:
     * - RenderTargets.java: 包含noTranslucents (depthtex1) 和 noHand (depthtex2)
     * - https://shaders.properties/current/reference/buffers/depthtex/
     *
     * 与RenderTargets的区别:
     * - RenderTargets (colortex0-15): 需要flip机制，因为前后帧可能相互依赖
     * - DepthTextures (depthtex0/1/2): 不需要flip，每帧独立生成
     *   - depthtex0: 当前帧完整深度（包含半透明物体）
     *   - depthtex1: 半透明物体前的深度（no translucents）
     *   - depthtex2: 手部前的深度（no hand）
     *
     * 深度纹理生成时机:
     * 1. depthtex1: 在TERRAIN_SOLID/ENTITIES阶段后生成（不含半透明和手部）
     * 2. depthtex0: 在TERRAIN_TRANSLUCENT阶段后生成（完整深度）
     * 3. depthtex2: 在HAND阶段前复制（不含手部）
     *
     * 对应HLSL (Common.hlsl):
     * ```hlsl
     * struct DepthTexturesBuffer {
     *     uint depthtex0Index;  // 完整深度
     *     uint depthtex1Index;  // 半透明前深度
     *     uint depthtex2Index;  // 手部前深度
     *     uint padding;         // 对齐到16字节
     * };
     *
     * // HLSL访问示例
     * StructuredBuffer<DepthTexturesBuffer> depthBuffer =
     *     ResourceDescriptorHeap[depthTexturesBufferIndex];
     * uint depthIndex = depthBuffer[0].depthtex0Index;
     * Texture2D<float> depth = ResourceDescriptorHeap[depthIndex];
     * float depthValue = depth.Load(int3(pixelPos, 0));
     * ```
     *
     * 使用示例（C++端）:
     * ```cpp
     * DepthTexturesIndexBuffer depthBuffer;
     *
     * // 设置引擎创建的深度纹理索引
     * depthBuffer.depthtex0Index = mainDepthTextureIndex;       // 完整深度
     * depthBuffer.depthtex1Index = noTranslucentsDepthIndex;    // 半透明前
     * depthBuffer.depthtex2Index = noHandDepthIndex;            // 手部前
     *
     * // 上传到GPU
     * d12Buffer->UploadData(&depthBuffer, sizeof(depthBuffer));
     * ```
     *
     * @note 此结构体必须与Common.hlsl中的DepthTexturesBuffer严格对应（16 bytes）
     * @note 所有深度纹理由引擎渲染过程自动生成，不从Minecraft或外部获取
     */
    struct DepthTexturesIndexBuffer
    {
        /**
         * @brief depthtex0索引 - 完整深度缓冲（包含所有物体）
         *
         * 教学要点:
         * 1. 完整的深度信息，包含不透明 + 半透明 + 手部
         * 2. 在TERRAIN_TRANSLUCENT阶段后生成
         * 3. 用于后处理效果（景深、SSAO等）
         *
         * 生成时机:
         * - WorldRenderingPhase::TERRAIN_TRANSLUCENT 之后
         * - 包含天空、地形、实体、半透明物体的完整深度
         *
         * HLSL访问:
         * ```hlsl
         * Texture2D<float> GetDepthTex0() {
         *     StructuredBuffer<DepthTexturesBuffer> depthBuffer =
         *         ResourceDescriptorHeap[depthTexturesBufferIndex];
         *     return ResourceDescriptorHeap[depthBuffer[0].depthtex0Index];
         * }
         *
         * float depth = GetDepthTex0().Load(int3(pixelPos, 0));
         * ```
         */
        uint32_t depthtex0Index;

        /**
         * @brief depthtex1索引 - 半透明物体前的深度缓冲
         *
         * 教学要点:
         * 1. 只包含不透明物体的深度（no translucents）
         * 2. 在TERRAIN_TRANSLUCENT阶段前生成
         * 3. 用于半透明物体的正确混合和深度测试
         *
         * 生成时机:
         * - WorldRenderingPhase::ENTITIES 之后
         * - WorldRenderingPhase::TERRAIN_TRANSLUCENT 之前
         * - 包含天空、地形实体、实体的深度（不含半透明和手部）
         *
         * 典型用途:
         * - 水面反射/折射需要不透明物体深度
         * - 半透明粒子的深度测试
         *
         * HLSL访问:
         * ```hlsl
         * Texture2D<float> GetDepthTex1() {
         *     StructuredBuffer<DepthTexturesBuffer> depthBuffer =
         *         ResourceDescriptorHeap[depthTexturesBufferIndex];
         *     return ResourceDescriptorHeap[depthBuffer[0].depthtex1Index];
         * }
         * ```
         */
        uint32_t depthtex1Index;

        /**
         * @brief depthtex2索引 - 手部前的深度缓冲
         *
         * 教学要点:
         * 1. 包含除手部外所有物体的深度（no hand）
         * 2. 在HAND阶段前复制depthtex0
         * 3. 用于手部的深度测试和特效
         *
         * 生成时机:
         * - WorldRenderingPhase::HAND 之前
         * - 复制自depthtex0（包含天空、地形、实体、半透明）
         *
         * 典型用途:
         * - 手部的景深特效
         * - 手部与世界的深度混合
         *
         * HLSL访问:
         * ```hlsl
         * Texture2D<float> GetDepthTex2() {
         *     StructuredBuffer<DepthTexturesBuffer> depthBuffer =
         *         ResourceDescriptorHeap[depthTexturesBufferIndex];
         *     return ResourceDescriptorHeap[depthBuffer[0].depthtex2Index];
         * }
         * ```
         */
        uint32_t depthtex2Index;

        /**
         * @brief 对齐填充 - 确保结构体对齐到16字节
         *
         * 教学要点:
         * 1. DirectX 12要求StructuredBuffer对齐到16字节
         * 2. 3个uint32_t = 12字节，需要4字节padding
         * 3. 未来可用于扩展（例如：深度纹理格式标志）
         */
        uint32_t padding;

        // ===== 总计: 4 × 4 = 16 bytes =====

        /**
         * @brief 默认构造函数 - 初始化为0
         */
        DepthTexturesIndexBuffer()
            : depthtex0Index(0)
              , depthtex1Index(0)
              , depthtex2Index(0)
              , padding(0)
        {
        }

        /**
         * @brief 批量设置所有深度纹理索引
         * @param depth0 depthtex0的Bindless纹理索引
         * @param depth1 depthtex1的Bindless纹理索引
         * @param depth2 depthtex2的Bindless纹理索引
         *
         * 使用示例:
         * ```cpp
         * depthBuffer.SetIndices(mainDepthIndex, noTranslucentsIndex, noHandIndex);
         * ```
         */
        void SetIndices(uint32_t depth0, uint32_t depth1, uint32_t depth2)
        {
            depthtex0Index = depth0;
            depthtex1Index = depth1;
            depthtex2Index = depth2;
        }

        /**
         * @brief 设置depthtex0索引
         * @param textureIndex Bindless纹理索引
         */
        void SetDepthTex0(uint32_t textureIndex)
        {
            depthtex0Index = textureIndex;
        }

        /**
         * @brief 设置depthtex1索引
         * @param textureIndex Bindless纹理索引
         */
        void SetDepthTex1(uint32_t textureIndex)
        {
            depthtex1Index = textureIndex;
        }

        /**
         * @brief 设置depthtex2索引
         * @param textureIndex Bindless纹理索引
         */
        void SetDepthTex2(uint32_t textureIndex)
        {
            depthtex2Index = textureIndex;
        }

        /**
         * @brief 获取depthtex0索引
         * @return Bindless纹理索引
         */
        uint32_t GetDepthTex0() const
        {
            return depthtex0Index;
        }

        /**
         * @brief 获取depthtex1索引
         * @return Bindless纹理索引
         */
        uint32_t GetDepthTex1() const
        {
            return depthtex1Index;
        }

        /**
         * @brief 获取depthtex2索引
         * @return Bindless纹理索引
         */
        uint32_t GetDepthTex2() const
        {
            return depthtex2Index;
        }

        /**
         * @brief 验证索引有效性
         * @return true 如果所有索引都不为0
         *
         * 教学要点:
         * 1. 用于运行时验证深度纹理是否正确设置
         * 2. 索引为0表示未初始化（通常0是保留索引）
         */
        bool IsValid() const
        {
            return depthtex0Index != 0 && depthtex1Index != 0 && depthtex2Index != 0;
        }

        /**
         * @brief 重置所有索引
         *
         * 教学要点:
         * 1. 用于帧结束后清理资源引用
         * 2. 或用于Shader Pack切换时的状态重置
         */
        void Reset()
        {
            depthtex0Index = 0;
            depthtex1Index = 0;
            depthtex2Index = 0;
            padding        = 0;
        }
    };

    // 编译期验证: 确保结构体大小为16字节
    static_assert(sizeof(DepthTexturesIndexBuffer) == 16,
                  "DepthTexturesIndexBuffer must be exactly 16 bytes to match HLSL DepthTexturesBuffer struct");

    // 编译期验证: 确保对齐到16字节
    static_assert(alignof(DepthTexturesIndexBuffer) == 4,
                  "DepthTexturesIndexBuffer must be 4-byte aligned for GPU upload");
} // namespace enigma::graphic
