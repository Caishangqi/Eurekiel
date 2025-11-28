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
     * Slot 15:    Custom Buffer Descriptor Table (1 DWORD) - space1隔离
     * ```
     *
     * 对应HLSL:
     * ```hlsl
     * // Slot 0-14: Root CBV使用space0
     * cbuffer Matrices : register(b7) {
     *     MatricesUniforms matrices;
     * };
     * cbuffer RootConstants : register(b14, space1) {
     *     uint noiseTextureIndex;
     * };
     * 
     * // Slot 15+: Custom Buffer必须使用space1
     * cbuffer MyCustomBuffer : register(b88, space1) {
     *     float4 myData;
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
            // Root CBV槽位 (Slot 0-14, 28 DWORDs)
            ROOT_CBV_UNDEFINE_0 = 0,
            ROOT_CBV_PER_OBJECT = 1, // PerObjectUniforms
            ROOT_CBV_CUSTOM_IMAGE = 2, // CustomImageUniforms
            ROOT_CBV_UNDEFINE_3 = 3,
            ROOT_CBV_UNDEFINE_4 = 4,
            ROOT_CBV_UNDEFINE_5 = 5,
            ROOT_CBV_UNDEFINE_6 = 6,
            ROOT_CBV_MATRICES = 7, // b7  - MatricesUniforms
            ROOT_CBV_UNDEFINE_8 = 8,
            ROOT_CBV_UNDEFINE_9 = 9,
            ROOT_CBV_UNDEFINE_10 = 10,
            ROOT_CBV_UNDEFINE_11 = 11,
            ROOT_CBV_UNDEFINE_12 = 12,
            ROOT_CBV_UNDEFINE_13 = 13,

            ROOT_CBV_UNDEFINE_14 = 14,
            ROOT_DESCRIPTOR_TABLE_CUSTOM = 15, // Descriptor Table for custom uniform

            ROOT_PARAMETER_COUNT = 16
        };

        /**
         * @brief Custom Buffer系统常量
         */
        static constexpr uint32_t MAX_CUSTOM_BUFFERS = 100; // 最大Custom Buffer数量

        /**
         * @brief Root Constants配置
         */
        static constexpr uint32_t ROOT_CONSTANTS_NUM_32BIT_VALUES = 1; // NoiseTexture index
        static constexpr uint32_t ROOT_CONSTANTS_SIZE_BYTES       = 4; // 1 DWORD = 4 bytes

        /**
         * @brief Root Signature预算统计
         */
        static constexpr uint32_t ROOT_SIGNATURE_DWORD_COUNT = 31; // 28 + 1 + 1 + 1 [NEW]
        static constexpr uint32_t ROOT_SIGNATURE_MAX_DWORDS  = 64; // DX12限制
        static constexpr float    ROOT_SIGNATURE_BUDGET_USED = 48.4f; // 31/64 = 48.4% [NEW]

        // [NEW] 编译期预算验证
        static_assert(ROOT_SIGNATURE_DWORD_COUNT <= ROOT_SIGNATURE_MAX_DWORDS,
                      "Root Signature exceeds 64 DWORDs limit");

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
