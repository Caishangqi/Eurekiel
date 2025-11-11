#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

namespace enigma::graphic
{
    /**
     * @brief BindlessRootSignature类 - Root CBV架构Root Signature
     *
     * 教学要点:
     * 1. Root CBV架构 - 14个Root Descriptor (b0-b13)，性能最优
     * 2. SM6.6 Bindless架构：移除Descriptor Table，全局堆直接索引
     * 3. 全局共享Root Signature：所有PSO使用同一个Root Signature
     * 4. 30 DWORDs预算：28 (Root CBV) + 1 (Root Constants) + 1 (Descriptor Table预留)
     *
     * Root Signature布局 (30 DWORDs = 46.9% budget):
     * ```
     * Slot 0-13:  Root CBV (28 DWORDs, 2 DWORDs each)
     *   - b0:  CameraAndPlayerUniforms
     *   - b1:  ScreenUniforms
     *   - b2:  IdUniforms
     *   - b3:  WorldUniforms
     *   - b4:  BiomeUniforms
     *   - b5:  RenderingUniforms
     *   - b6:  RenderTargetsIndexBuffer
     *   - b7:  MatricesUniforms [PHASE 1 ACTIVE]
     *   - b8:  DepthTexturesIndexBuffer
     *   - b9:  ShadowUniforms
     *   - b10: [Reserved]
     *   - b11: ColorTargetsIndexBuffer
     *   - b12: ShadowColorIndexBuffer
     *   - b13: PlayerUniforms
     * Slot 14:    Root Constants (1 DWORD) - NoiseTexture index (b14 space1)
     * Slot 15:    Descriptor Table预留 (1 DWORD) - Custom Buffer
     * ```
     *
     * 对应HLSL:
     * ```hlsl
     * cbuffer Matrices : register(b7) {
     *     MatricesUniforms matrices;
     * };
     * cbuffer RootConstants : register(b14, space1) {
     *     uint noiseTextureIndex;
     * };
     * ```
     *
     * 架构优势:
     * - Root CBV性能最优（GPU硬件优化路径）
     * - 无需offset参数，Shader代码简化
     * - 避免StructuredBuffer覆写Bug
     *
     * @note Phase 1只激活Slot 7 (Matrices)，其他Slot预留
     */
    class BindlessRootSignature final
    {
    public:
        /**
         * @brief Root Signature布局枚举 (30 DWORDs总预算)
         *
         * 教学要点:
         * 1. Root CBV: 每个占用2 DWORDs (64位GPU虚拟地址)
         * 2. Root Constants: 每个DWORD占用1 DWORD
         * 3. Descriptor Table: 占用1 DWORD (GPU Heap偏移)
         * 4. DX12限制：最多64 DWORDs（256字节）
         *
         * Phase 1实现:
         * - 激活Slot 7 (MatricesUniforms)
         * - 保留其他13个Root CBV槽位
         * - 预算使用: 30/64 DWORDs (46.9%)
         */
        enum RootParameterIndex : uint32_t
        {
            // Root CBV槽位 (Slot 0-13, 28 DWORDs)
            ROOT_CBV_CAMERA_AND_PLAYER = 0, // b0  - CameraAndPlayerUniforms
            ROOT_CBV_SCREEN = 1, // b1  - ScreenUniforms
            ROOT_CBV_ID = 2, // b2  - IdUniforms
            ROOT_CBV_WORLD = 3, // b3  - WorldUniforms
            ROOT_CBV_BIOME = 4, // b4  - BiomeUniforms
            ROOT_CBV_RENDERING = 5, // b5  - RenderingUniforms
            ROOT_CBV_RENDER_TARGETS = 6, // b6  - RenderTargetsIndexBuffer
            ROOT_CBV_MATRICES = 7, // b7  - MatricesUniforms [PHASE 1]
            ROOT_CBV_DEPTH_TEXTURES = 8, // b8  - DepthTexturesIndexBuffer
            ROOT_CBV_SHADOW = 9, // b9  - ShadowUniforms
            ROOT_CBV_RESERVED_10 = 10, // b10 - [Reserved]
            ROOT_CBV_COLOR_TARGETS = 11, // b11 - ColorTargetsIndexBuffer
            ROOT_CBV_SHADOW_COLOR = 12, // b12 - ShadowColorIndexBuffer
            ROOT_CBV_PLAYER = 13, // b13 - PlayerUniforms

            // 其他Root Parameters
            ROOT_CONSTANTS_NOISE_TEXTURE = 14, // b14 space1 - NoiseTexture index (1 DWORD)
            ROOT_DESCRIPTOR_TABLE_CUSTOM = 15, // 预留Custom Buffer (1 DWORD)

            ROOT_PARAMETER_COUNT = 16
        };

        /**
         * @brief Root Constants配置
         */
        static constexpr uint32_t ROOT_CONSTANTS_NUM_32BIT_VALUES = 1; // NoiseTexture index
        static constexpr uint32_t ROOT_CONSTANTS_SIZE_BYTES       = 4; // 1 DWORD = 4 bytes

        /**
         * @brief Root Signature预算统计
         */
        static constexpr uint32_t ROOT_SIGNATURE_DWORD_COUNT = 30; // 28 + 1 + 1
        static constexpr uint32_t ROOT_SIGNATURE_MAX_DWORDS  = 64; // DX12限制
        static constexpr float    ROOT_SIGNATURE_BUDGET_USED = 46.9f; // 30/64 = 46.9%

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
         * rootParameter.Constants.Num32BitValues = 13; // 13 DWORDs = 52 bytes
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
         * @param offset 偏移量（32位值单位，默认0）
         *
         * 教学要点: 优化的单值设置，避免memcpy开销
         *
         * 使用示例:
         * ```cpp
         * // 设置NoiseTexture索引
         * rootSignature->SetRootConstant32Bit(cmdList, noiseTextureIndex);
         * ```
         */
        void SetRootConstant32Bit(
            ID3D12GraphicsCommandList* commandList,
            uint32_t                   value,
            uint32_t                   offset = 0) const;

        /**
         * @brief 绑定Root CBV到指定槽位
         * @param commandList 命令列表
         * @param slot Root Parameter索引 (使用RootParameterIndex枚举)
         * @param bufferLocation Buffer的GPU虚拟地址
         *
         * 教学要点:
         * 1. Root CBV直接绑定GPU虚拟地址，无需Descriptor Heap
         * 2. 性能最优 - GPU硬件优化路径
         * 3. Phase 1只使用Slot 7 (Matrices)
         *
         * 使用示例:
         * ```cpp
         * D3D12_GPU_VIRTUAL_ADDRESS gpuAddr = buffer->GetGPUVirtualAddress();
         * rootSignature->SetRootCBV(cmdList, ROOT_CBV_MATRICES, gpuAddr);
         * ```
         */
        void SetRootCBV(
            ID3D12GraphicsCommandList* commandList,
            RootParameterIndex         slot,
            D3D12_GPU_VIRTUAL_ADDRESS  bufferLocation) const;

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
