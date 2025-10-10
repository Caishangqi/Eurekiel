#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

namespace enigma::graphic
{
    /**
     * @brief BindlessRootSignature类 - RENDERTARGETS混合架构Root Signature
     *
     * 教学要点:
     * 1. 44 bytes Root Constants - 11个uint32_t索引，支持细粒度更新
     * 2. SM6.6 Bindless架构：移除所有Descriptor Table，全局堆直接索引
     * 3. 全局共享Root Signature：所有PSO使用同一个Root Signature
     * 4. 极致性能：Root Signature切换从1000次/帧降至1次/帧（99.9%优化）
     *
     * 架构对比:
     * - True Bindless (8 bytes): uniformBufferIndex + renderTargetIndicesBase
     * - RENDERTARGETS混合架构 (44 bytes): 11个独立索引，支持Iris兼容性
     *
     * 对应HLSL (Common.hlsl):
     * ```hlsl
     * cbuffer RootConstants : register(b0, space0) {
     *     uint cameraAndPlayerBufferIndex;       // Offset 0
     *     uint playerStatusBufferIndex;          // Offset 4
     *     uint screenAndSystemBufferIndex;       // Offset 8
     *     uint idBufferIndex;                    // Offset 12
     *     uint worldAndWeatherBufferIndex;       // Offset 16
     *     uint biomeAndDimensionBufferIndex;     // Offset 20
     *     uint renderingBufferIndex;             // Offset 24
     *     uint matricesBufferIndex;              // Offset 28
     *     uint renderTargetsBufferIndex;         // Offset 32 ⭐ 关键
     *     uint depthTextureBufferIndex;          // Offset 36
     *     uint customImageBufferIndex;           // Offset 40
     * };
     * ```
     *
     * Root Signature布局:
     * ```
     * [Root Constants 11 DWORDs = 44 bytes]
     * - 11个StructuredBuffer/Texture索引
     * - 支持部分更新 (SetGraphicsRoot32BitConstant)
     * ```
     *
     * HLSL Bindless访问示例:
     * ```hlsl
     * // 获取RenderTarget（Main/Alt自动处理）
     * Texture2D GetRenderTarget(uint rtIndex) {
     *     StructuredBuffer<RenderTargetsBuffer> rtBuffer =
     *         ResourceDescriptorHeap[renderTargetsBufferIndex];
     *     uint textureIndex = rtBuffer[0].readIndices[rtIndex];
     *     return ResourceDescriptorHeap[textureIndex];
     * }
     *
     * // Iris兼容宏
     * #define colortex0 GetRenderTarget(0)
     * float4 color = colortex0.Sample(linearSampler, uv);
     * ```
     *
     * @note 这是RENDERTARGETS混合架构的核心组件，全局唯一，所有PSO共享
     */
    class BindlessRootSignature final
    {
    public:
        /**
         * @brief Root Constants配置常量
         *
         * 教学要点:
         * 1. RENDERTARGETS混合架构：44 bytes (11个uint32_t)
         * 2. DX12限制：最多64 DWORDs（256字节）
         * 3. 对应Common.hlsl中的RootConstants定义
         *
         * 设计决策:
         * - 44 bytes = 11 DWORDs 足够支持所有Iris Buffer索引
         * - 每个索引4字节，可独立更新（细粒度控制）
         * - 预留空间：64 DWORDs - 11 = 53 DWORDs 可用于未来扩展
         */
        static constexpr uint32_t ROOT_CONSTANTS_NUM_32BIT_VALUES = 11; // 11 DWORDs = 44 bytes
        static constexpr uint32_t ROOT_CONSTANTS_SIZE_BYTES       = ROOT_CONSTANTS_NUM_32BIT_VALUES * 4;

        /**
         * @brief Root Parameter索引常量
         */
        static constexpr uint32_t ROOT_PARAMETER_INDEX_CONSTANTS = 0; // 唯一的Root Parameter

        /**
         * @brief Root Constants字段偏移量（DWORD单位）
         *
         * 教学要点:
         * 1. 每个索引占用1个DWORD（4字节）
         * 2. 偏移量用于SetGraphicsRoot32BitConstant细粒度更新
         * 3. 完全对应Common.hlsl中的RootConstants字段顺序
         *
         * 使用示例:
         * ```cpp
         * // 只更新renderTargetsBufferIndex
         * cmdList->SetGraphicsRoot32BitConstant(
         *     ROOT_PARAMETER_INDEX_CONSTANTS,
         *     rtBufferIndex,
         *     OFFSET_RENDER_TARGETS_BUFFER_INDEX
         * );
         * ```
         */
        static constexpr uint32_t OFFSET_CAMERA_AND_PLAYER_BUFFER_INDEX   = 0; // Offset 0
        static constexpr uint32_t OFFSET_PLAYER_STATUS_BUFFER_INDEX       = 1; // Offset 4
        static constexpr uint32_t OFFSET_SCREEN_AND_SYSTEM_BUFFER_INDEX   = 2; // Offset 8
        static constexpr uint32_t OFFSET_ID_BUFFER_INDEX                  = 3; // Offset 12
        static constexpr uint32_t OFFSET_WORLD_AND_WEATHER_BUFFER_INDEX   = 4; // Offset 16
        static constexpr uint32_t OFFSET_BIOME_AND_DIMENSION_BUFFER_INDEX = 5; // Offset 20
        static constexpr uint32_t OFFSET_RENDERING_BUFFER_INDEX           = 6; // Offset 24
        static constexpr uint32_t OFFSET_MATRICES_BUFFER_INDEX            = 7; // Offset 28
        static constexpr uint32_t OFFSET_RENDER_TARGETS_BUFFER_INDEX      = 8; // Offset 32 ⭐ 关键
        static constexpr uint32_t OFFSET_DEPTH_TEXTURE_BUFFER_INDEX       = 9; // Offset 36
        static constexpr uint32_t OFFSET_CUSTOM_IMAGE_BUFFER_INDEX        = 10; // Offset 40

    public:
        /**
         * @brief 构造函数
         */
        BindlessRootSignature();

        /**
         * @brief 析构函数 - RAII自动释放
         */
        ~BindlessRootSignature() = default;

        // 禁用拷贝，支持移动
        BindlessRootSignature(const BindlessRootSignature&)            = delete;
        BindlessRootSignature& operator=(const BindlessRootSignature&) = delete;
        BindlessRootSignature(BindlessRootSignature&&)                 = default;
        BindlessRootSignature& operator=(BindlessRootSignature&&)      = default;

        // ========================================================================
        // 生命周期管理
        // ========================================================================

        /**
         * @brief 初始化Root Signature
         * @return 成功返回true，失败返回false
         *
         * 教学要点:
         * 1. 从D3D12RenderSystem::GetDevice()获取设备
         * 2. 创建Root Signature描述（1个Root Constants参数，11个DWORDs）
         * 3. 设置SM6.6 Bindless标志：D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED
         * 4. 序列化并创建Root Signature对象
         *
         * Root Signature配置:
         * ```cpp
         * D3D12_ROOT_SIGNATURE_DESC desc = {};
         * desc.NumParameters = 1;  // 只有1个Root Constants参数
         * desc.pParameters = &rootParameter;
         * desc.Flags =
         *     D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
         *     D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |  // SM6.6关键
         *     D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;        // Sampler也直接索引
         * ```
         *
         * Root Parameter配置:
         * ```cpp
         * D3D12_ROOT_PARAMETER rootParameter = {};
         * rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
         * rootParameter.Constants.ShaderRegister = 0;  // b0
         * rootParameter.Constants.RegisterSpace = 0;   // space0
         * rootParameter.Constants.Num32BitValues = 11; // 11 DWORDs = 44 bytes
         * rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
         * ```
         */
        bool Initialize();

        /**
         * @brief 释放所有资源
         */
        void Shutdown();

        /**
         * @brief 检查是否已初始化
         */
        bool IsInitialized() const { return m_initialized; }

        // ========================================================================
        // Root Signature访问接口
        // ========================================================================

        /**
         * @brief 获取Root Signature对象
         * @return Root Signature指针
         *
         * 教学要点: 用于PSO创建和命令列表绑定
         */
        ID3D12RootSignature* GetRootSignature() const { return m_rootSignature.Get(); }

        /**
         * @brief 绑定Root Signature到命令列表
         * @param commandList 命令列表
         *
         * 教学要点:
         * 1. 每帧只需绑定一次（不像传统方式需要每个Pass绑定）
         * 2. 通常在BeginFrame时调用
         * 3. 全局共享 - 所有PSO使用同一个Root Signature
         */
        void BindToCommandList(ID3D12GraphicsCommandList* commandList) const;

        /**
         * @brief 设置完整Root Constants到命令列表
         * @param commandList 命令列表
         * @param data Root Constants数据指针（RootConstants结构体）
         * @param numValues 要设置的32位值数量（默认11，可部分更新）
         * @param offset 起始偏移量（32位值单位，默认0）
         *
         * 教学要点:
         * 1. Root Constants是最快的常量传递方式（直接在Root Signature中）
         * 2. 每次Pass执行前可以更新不同的索引
         * 3. 支持部分更新（通过offset和numValues）
         *
         * 使用示例:
         * ```cpp
         * RootConstants constants = {...};  // 44 bytes
         * rootSignature->SetRootConstants(cmdList, &constants, 11, 0);
         * ```
         */
        void SetRootConstants(
            ID3D12GraphicsCommandList* commandList,
            const void*                data,
            uint32_t                   numValues = ROOT_CONSTANTS_NUM_32BIT_VALUES,
            uint32_t                   offset    = 0) const;

        /**
         * @brief 设置单个32位Root Constant
         * @param commandList 命令列表
         * @param value 32位值
         * @param offset 偏移量（32位值单位，使用OFFSET_*常量）
         *
         * 教学要点: 优化的单值设置，避免memcpy开销
         *
         * 使用示例:
         * ```cpp
         * // 只更新renderTargetsBufferIndex
         * rootSignature->SetRootConstant32Bit(
         *     cmdList,
         *     rtBufferIndex,
         *     BindlessRootSignature::OFFSET_RENDER_TARGETS_BUFFER_INDEX
         * );
         * ```
         */
        void SetRootConstant32Bit(
            ID3D12GraphicsCommandList* commandList,
            uint32_t                   value,
            uint32_t                   offset) const;

    private:
        // Root Signature对象
        Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

        // 初始化状态
        bool m_initialized = false;

        // ========================================================================
        // 私有辅助方法
        // ========================================================================

        /**
         * @brief 创建Root Signature
         * @param device DX12设备
         * @return 成功返回true
         *
         * 实现指导:
         * 1. 创建Root Parameter数组（只有1个32位常量参数，11个DWORDs）
         * 2. 设置Root Signature描述
         * 3. 添加SM6.6 Bindless标志
         * 4. 序列化并创建Root Signature
         */
        bool CreateRootSignature(ID3D12Device* device);
    };
} // namespace enigma::graphic
