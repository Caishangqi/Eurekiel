#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

namespace enigma::graphic
{
    /**
     * @brief BindlessRootSignature类 - Shader Model 6.6极简Root Signature
     *
     * 教学要点:
     * 1. SM6.6 Bindless架构核心：移除所有Descriptor Table，只保留Root Constants
     * 2. 全局描述符堆直接访问：通过D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED标志
     * 3. HLSL无需预绑定：着色器直接用索引访问 ResourceDescriptorHeap[index]
     * 4. 极致性能：Root Signature切换从1000次/帧降至1次/帧（99.9%优化）
     *
     * SM6.6 vs 传统Bindless对比:
     * - 传统: Root Signature包含多个Descriptor Table，每个Pass切换
     * - SM6.6: 单一Root Signature，全局堆直接索引，无需切换
     *
     * Root Signature布局 (只含Root Constants):
     * ```
     * [Root Constants 32 DWORDs = 128 bytes]
     * - 资源索引 (纹理、缓冲区等)
     * - Per-draw常量数据
     * - 变换矩阵等
     * ```
     *
     * HLSL使用示例:
     * ```hlsl
     * // Root Constants
     * cbuffer RootConstants : register(b0, space0) {
     *     uint textureIndex;
     *     uint bufferIndex;
     *     float4x4 worldMatrix;
     * };
     *
     * // 直接访问全局堆 - 无需Descriptor Table绑定
     * Texture2D tex = ResourceDescriptorHeap[textureIndex];
     * StructuredBuffer<float4> buf = ResourceDescriptorHeap[bufferIndex];
     * SamplerState samp = SamplerDescriptorHeap[0];
     * ```
     *
     * @note 这是SM6.6架构的核心组件，全局唯一，所有PSO共享
     */
    class BindlessRootSignature final
    {
    public:
        /**
         * @brief Root Constants配置常量
         *
         * 教学要点:
         * 1. Root Constants限制：DX12最多64 DWORDs（256字节）
         * 2. SM6.6推荐：32 DWORDs（128字节）平衡性能和灵活性
         * 3. 每个DWORD = 4字节 = 1个uint32/float
         */
        static constexpr uint32_t ROOT_CONSTANTS_NUM_32BIT_VALUES = 32;  // 32 DWORDs = 128 bytes
        static constexpr uint32_t ROOT_CONSTANTS_SIZE_BYTES = ROOT_CONSTANTS_NUM_32BIT_VALUES * 4;

        /**
         * @brief Root Parameter索引常量
         */
        static constexpr uint32_t ROOT_PARAMETER_INDEX_CONSTANTS = 0;  // 唯一的Root Parameter

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
         * 2. 创建Root Signature描述
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
         * rootParameter.Constants.Num32BitValues = ROOT_CONSTANTS_NUM_32BIT_VALUES;
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
         */
        void BindToCommandList(ID3D12GraphicsCommandList* commandList) const;

        /**
         * @brief 设置Root Constants到命令列表
         * @param commandList 命令列表
         * @param data Root Constants数据指针
         * @param numValues 要设置的32位值数量（不超过ROOT_CONSTANTS_NUM_32BIT_VALUES）
         * @param offset 起始偏移量（32位值单位）
         *
         * 教学要点:
         * 1. Root Constants是最快的常量传递方式（直接在Root Signature中）
         * 2. 每次Draw Call前可以更新不同的索引和常量
         * 3. 支持部分更新（通过offset和numValues）
         *
         * 使用示例:
         * ```cpp
         * struct RootConstants {
         *     uint32_t textureIndex;
         *     uint32_t bufferIndex;
         *     float worldMatrix[16];  // 4x4矩阵
         * };
         * RootConstants constants = {...};
         * rootSignature->SetRootConstants(cmdList, &constants, 18, 0);
         * ```
         */
        void SetRootConstants(
            ID3D12GraphicsCommandList* commandList,
            const void* data,
            uint32_t numValues,
            uint32_t offset = 0) const;

        /**
         * @brief 设置单个32位Root Constant
         * @param commandList 命令列表
         * @param value 32位值
         * @param offset 偏移量（32位值单位）
         *
         * 教学要点: 优化的单值设置，避免memcpy开销
         */
        void SetRootConstant32Bit(
            ID3D12GraphicsCommandList* commandList,
            uint32_t value,
            uint32_t offset) const;

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
         * 1. 创建Root Parameter数组（只有1个32位常量参数）
         * 2. 设置Root Signature描述
         * 3. 添加SM6.6 Bindless标志
         * 4. 序列化并创建Root Signature
         */
        bool CreateRootSignature(ID3D12Device* device);
    };

} // namespace enigma::graphic
