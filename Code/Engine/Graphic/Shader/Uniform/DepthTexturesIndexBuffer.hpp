#pragma once

#include <cstdint>

namespace enigma::graphic
{
    /**
     * @brief DepthTexturesIndexBuffer - 动态深度纹理索引管理（M3.1版本）
     *
     * **M3.1改进**:
     * 1. 支持1-16个深度纹理（动态配置）
     * 2. 保持与Iris的向后兼容（depthtex0/1/2）
     * 3. 固定64字节结构（16个uint32_t索引）
     * 4. 与Common.hlsl中的DepthTexturesBuffer结构体完全对应
     *
     * 教学要点:
     * 1. 动态深度纹理数量配置
     * 2. 不需要Main/Alt双缓冲机制（depth textures是引擎渲染过程中自然产生的）
     * 3. 每个Pass执行前上传到GPU StructuredBuffer
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
     * 对应HLSL (Common.hlsl) - M3.1版本:
     * ```hlsl
     * struct DepthTexturesBuffer {
     *     uint depthTextureIndices[16];  // 最多16个深度纹理索引
     * };
     *
     * // HLSL访问示例（向后兼容）
     * StructuredBuffer<DepthTexturesBuffer> depthBuffer =
     *     ResourceDescriptorHeap[depthTexturesBufferIndex];
     * 
     * uint depthtex0Index = depthBuffer[0].depthTextureIndices[0];
     * uint depthtex1Index = depthBuffer[0].depthTextureIndices[1];
     * uint depthtex2Index = depthBuffer[0].depthTextureIndices[2];
     * 
     * Texture2D<float> depth = ResourceDescriptorHeap[depthtex0Index];
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
         * @brief 深度纹理索引数组 - M3.1动态配置（最多16个）
         *
         * **索引语义（向后兼容Iris）**:
         * - depthTextureIndices[0] = depthtex0 (主深度纹理，完整深度)
         * - depthTextureIndices[1] = depthtex1 (半透明前深度)
         * - depthTextureIndices[2] = depthtex2 (手部前深度)
         * - depthTextureIndices[3-15] = 用户自定义深度纹理
         *
         * **M3.1新特性**:
         * - 支持1-16个深度纹理
         * - 动态调整数量
         * - 每个深度纹理可独立配置分辨率
         *
         * **生成时机（前3个深度纹理）**:
         * 1. depthtex0: TERRAIN_TRANSLUCENT之后（完整深度）
         * 2. depthtex1: ENTITIES之后、TERRAIN_TRANSLUCENT之前（不含半透明）
         * 3. depthtex2: HAND之前（不含手部）
         *
         * **HLSL访问示例**:
         * ```hlsl
         * StructuredBuffer<DepthTexturesBuffer> depthBuffer =
         *     ResourceDescriptorHeap[depthTexturesBufferIndex];
         * 
         * // 向后兼容访问
         * uint depthtex0Index = depthBuffer[0].depthTextureIndices[0];
         * uint depthtex1Index = depthBuffer[0].depthTextureIndices[1];
         * uint depthtex2Index = depthBuffer[0].depthTextureIndices[2];
         * 
         * // 自定义深度纹理访问
         * uint customDepthIndex = depthBuffer[0].depthTextureIndices[3];
         * 
         * Texture2D<float> depth = ResourceDescriptorHeap[depthtex0Index];
         * float depthValue = depth.Load(int3(pixelPos, 0));
         * ```
         *
         * 教学要点:
         * 1. 固定16个槽位确保GPU内存布局稳定
         * 2. 未使用的槽位索引为0（无效索引）
         * 3. DirectX 12要求StructuredBuffer对齐到16字节
         */
        uint32_t depthTextureIndices[16];

        // ===== 总计: 16 × 4 = 64 bytes =====

        /**
         * @brief 默认构造函数 - 初始化所有索引为0
         * 
         * M3.1: 初始化16个槽位全部为0
         */
        DepthTexturesIndexBuffer()
        {
            for (int i = 0; i < 16; ++i)
            {
                depthTextureIndices[i] = 0;
            }
        }

        /**
         * @brief 批量设置深度纹理索引（兼容旧接口）
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
            depthTextureIndices[0] = depth0;
            depthTextureIndices[1] = depth1;
            depthTextureIndices[2] = depth2;
        }

        /**
         * @brief M3.1: 设置指定索引的深度纹理
         * @param index 深度纹理索引 [0-15]
         * @param textureIndex Bindless纹理索引
         * 
         * 教学要点:
         * - 支持动态设置任意深度纹理
         * - 超出范围会抛出异常
         */
        void SetDepthTextureIndex(int index, uint32_t textureIndex)
        {
            if (index < 0 || index >= 16)
            {
                throw std::out_of_range("Depth texture index must be in range [0-15]");
            }
            depthTextureIndices[index] = textureIndex;
        }

        /**
         * @brief M3.1: 获取指定索引的深度纹理
         * @param index 深度纹理索引 [0-15]
         * @return Bindless纹理索引
         */
        uint32_t GetDepthTextureIndex(int index) const
        {
            if (index < 0 || index >= 16)
            {
                throw std::out_of_range("Depth texture index must be in range [0-15]");
            }
            return depthTextureIndices[index];
        }

        /**
         * @brief 设置depthtex0索引（向后兼容）
         * @param textureIndex Bindless纹理索引
         */
        void SetDepthTex0(uint32_t textureIndex)
        {
            depthTextureIndices[0] = textureIndex;
        }

        /**
         * @brief 设置depthtex1索引（向后兼容）
         * @param textureIndex Bindless纹理索引
         */
        void SetDepthTex1(uint32_t textureIndex)
        {
            depthTextureIndices[1] = textureIndex;
        }

        /**
         * @brief 设置depthtex2索引（向后兼容）
         * @param textureIndex Bindless纹理索引
         */
        void SetDepthTex2(uint32_t textureIndex)
        {
            depthTextureIndices[2] = textureIndex;
        }

        /**
         * @brief 获取depthtex0索引（向后兼容）
         * @return Bindless纹理索引
         */
        uint32_t GetDepthTex0() const
        {
            return depthTextureIndices[0];
        }

        /**
         * @brief 获取depthtex1索引（向后兼容）
         * @return Bindless纹理索引
         */
        uint32_t GetDepthTex1() const
        {
            return depthTextureIndices[1];
        }

        /**
         * @brief 获取depthtex2索引（向后兼容）
         * @return Bindless纹理索引
         */
        uint32_t GetDepthTex2() const
        {
            return depthTextureIndices[2];
        }

        /**
         * @brief 验证前3个深度纹理索引有效性（向后兼容）
         * @return true 如果depthtex0/1/2都不为0
         *
         * 教学要点:
         * 1. 用于运行时验证深度纹理是否正确设置
         * 2. 索引为0表示未初始化（通常0是保留索引）
         */
        bool IsValid() const
        {
            return depthTextureIndices[0] != 0 &&
                depthTextureIndices[1] != 0 &&
                depthTextureIndices[2] != 0;
        }

        /**
         * @brief M3.1: 验证指定数量的深度纹理索引有效性
         * @param count 需要验证的深度纹理数量 [1-16]
         * @return true 如果前count个索引都不为0
         */
        bool IsValidCount(int count) const
        {
            if (count < 1 || count > 16) return false;

            for (int i = 0; i < count; ++i)
            {
                if (depthTextureIndices[i] == 0)
                {
                    return false;
                }
            }
            return true;
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
            for (int i = 0; i < 16; ++i)
            {
                depthTextureIndices[i] = 0;
            }
        }
    };

    // M3.1编译期验证: 确保结构体大小为64字节（16个uint32_t）
    static_assert(sizeof(DepthTexturesIndexBuffer) == 64,
                  "DepthTexturesIndexBuffer must be exactly 64 bytes (16 x 4 bytes) to match HLSL DepthTexturesBuffer struct");

    // 编译期验证: 确保对齐到16字节（GPU要求）
    static_assert(sizeof(DepthTexturesIndexBuffer) % 16 == 0,
                  "DepthTexturesIndexBuffer must be aligned to 16 bytes for GPU upload");

    // 编译期验证: 确保数组元素对齐
    static_assert(alignof(DepthTexturesIndexBuffer) == 4,
                  "DepthTexturesIndexBuffer must be 4-byte aligned (uint32_t alignment)");
} // namespace enigma::graphic
