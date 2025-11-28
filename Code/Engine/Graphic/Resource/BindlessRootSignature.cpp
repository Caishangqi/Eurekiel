#include "BindlessRootSignature.hpp"
#include "../Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

#include "ThirdParty/d3dx12/d3dx12.h"

namespace enigma::graphic
{
    // ========================================================================
    // BindlessRootSignature 实现
    // ========================================================================

    BindlessRootSignature::BindlessRootSignature()
        : m_rootSignature(nullptr)
          , m_initialized(false)
    {
        // 构造函数：只初始化成员变量，实际创建在Initialize()
    }

    bool BindlessRootSignature::Initialize()
    {
        if (m_initialized)
        {
            core::LogWarn("BindlessRootSignature", "Already initialized");
            return true;
        }

        // 1. 获取D3D12设备
        auto device = D3D12RenderSystem::GetDevice();
        if (!device)
        {
            core::LogError("BindlessRootSignature", "Failed to get D3D12 device");
            return false;
        }

        // 2. 创建Root Signature
        if (!CreateRootSignature(device))
        {
            core::LogError("BindlessRootSignature", "Failed to create root signature");
            return false;
        }

        m_initialized = true;

        core::LogInfo("BindlessRootSignature",
                      "Initialized successfully (Root CBV架构)");
        core::LogInfo("BindlessRootSignature",
                      "  - Root Signature: %u DWORDs (%.1f%% budget)", // [NEW]
                      ROOT_SIGNATURE_DWORD_COUNT,
                      ROOT_SIGNATURE_BUDGET_USED);
        core::LogInfo("BindlessRootSignature",
                      "  - Root CBV: 15 slots (Slot 0-14, 30 DWORDs)"); // [NEW]
        core::LogInfo("BindlessRootSignature",
                      "  - Custom Buffer Descriptor Table: Slot 15 (1 DWORD)"); // [NEW]
        core::LogInfo("BindlessRootSignature",
                      "  - Phase 1: Slot 7 (Matrices) active");
        core::LogInfo("BindlessRootSignature",
                      "  - Flags: CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED");

        return true;
    }

    void BindlessRootSignature::Shutdown()
    {
        if (!m_initialized)
            return;

        m_rootSignature.Reset();
        m_initialized = false;

        core::LogInfo("BindlessRootSignature", "Shutdown completed");
    }

    void BindlessRootSignature::BindToCommandList(ID3D12GraphicsCommandList* commandList) const
    {
        if (!m_initialized || !commandList)
        {
            core::LogError("BindlessRootSignature",
                           "BindToCommandList: not initialized or invalid command list");
            return;
        }

        commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    }

    // ========================================================================
    // 私有辅助方法
    // ========================================================================

    bool BindlessRootSignature::CreateRootSignature(ID3D12Device* device)
    {
        // 1. 创建Root Parameters数组 (16个参数: Slot 0-15)
        std::vector<CD3DX12_ROOT_PARAMETER> rootParameters(ROOT_PARAMETER_COUNT); // 0-15 [NEW]

        // [NEW] Slot 0-13: 14个Root CBV (Phase 1只初始化结构,Slot 7激活)
        for (uint32_t i = ROOT_CBV_UNDEFINE_0; i <= ROOT_CBV_UNDEFINE_14; ++i)
        {
            rootParameters[i].InitAsConstantBufferView(
                i, // shaderRegister (b0-b14)
                0, // registerSpace (space0)
                D3D12_SHADER_VISIBILITY_ALL); // visibility
        }

        // ============================================================================
        // [Slot 15] Custom Buffer Descriptor Table (space1隔离)
        // ============================================================================
        // 
        // 设计决策：使用registerSpace=1 (space1)实现与Root CBV的完全隔离
        // 
        // 架构说明：
        // - Root CBV (Slot 0-14): 使用space0的b0-b14，直接绑定
        // - Custom Buffer Table:  使用space1，支持b0-b99
        // 
        // 映射关系：Table[N] → register(bN, space1)
        // 例如：Table[88] → register(b88, space1)
        // 
        // [IMPORTANT] Shader编写规则：
        // - slot < 15:  cbuffer MyBuffer : register(bN) { ... }
        // - slot >= 15: cbuffer MyBuffer : register(bN, space1) { ... }
        // 
        // 参考：2025-11-27-custom-buffer-api-design-research.md
        // ============================================================================
        CD3DX12_DESCRIPTOR_RANGE customBufferRange;
        customBufferRange.Init(
            D3D12_DESCRIPTOR_RANGE_TYPE_CBV,           // rangeType
            MAX_CUSTOM_BUFFERS,                         // numDescriptors (100个CBV)
            0,                                          // baseShaderRegister (b0起始，在space1中)
            1);                                         // registerSpace = space1 (与Root CBV的space0隔离)

        rootParameters[ROOT_DESCRIPTOR_TABLE_CUSTOM].InitAsDescriptorTable(
            1,                              // numDescriptorRanges
            &customBufferRange,            // pDescriptorRanges
            D3D12_SHADER_VISIBILITY_ALL);  // visibility

        // 2. 创建Static Sampler数组（与Common.hlsl中的Sampler声明对应）
        // TODO: 未来可以改为动态配置SetSamplerState()
        // 当前使用Static Sampler以实现零预算开销和最优性能
        // Static Sampler是编译时确定的，无法运行时修改
        // 注意：Static Sampler不占用Root Signature预算，是性能最优的采样器配置方式
        std::array<CD3DX12_STATIC_SAMPLER_DESC, 3> staticSamplers;

        // 2.1 Linear Sampler (s0) - 线性过滤，用于常规纹理采样
        staticSamplers[0].Init(
            0,                                      // shaderRegister (s0)
            D3D12_FILTER_MIN_MAG_MIP_LINEAR,       // filter - 三线性过滤
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,       // addressU - U轴重复寻址
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,       // addressV - V轴重复寻址
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,       // addressW - W轴重复寻址
            0.0f,                                   // mipLODBias
            16,                                     // maxAnisotropy - 各向异性过滤级别
            D3D12_COMPARISON_FUNC_NEVER,           // comparisonFunc
            D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE, // borderColor
            0.0f,                                   // minLOD
            D3D12_FLOAT32_MAX,                      // maxLOD
            D3D12_SHADER_VISIBILITY_ALL,            // visibility - 所有着色器阶段可见
            0);                                     // registerSpace (space0)

        // 2.2 Point Sampler (s1) - 点采样，用于精确像素访问
        staticSamplers[1].Init(
            1,                                      // shaderRegister (s1)
            D3D12_FILTER_MIN_MAG_MIP_POINT,        // filter - 点采样无过滤
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,      // addressU - U轴钳位寻址
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,      // addressV - V轴钳位寻址
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,      // addressW - W轴钳位寻址
            0.0f,                                   // mipLODBias
            1,                                      // maxAnisotropy - 点采样不使用各向异性
            D3D12_COMPARISON_FUNC_NEVER,           // comparisonFunc
            D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK, // borderColor
            0.0f,                                   // minLOD
            D3D12_FLOAT32_MAX,                      // maxLOD
            D3D12_SHADER_VISIBILITY_ALL,            // visibility
            0);                                     // registerSpace (space0)

        // 2.3 Shadow Comparison Sampler (s2) - 阴影比较采样器
        staticSamplers[2].Init(
            2,                                      // shaderRegister (s2)
            D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter - 比较采样线性过滤
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,     // addressU - 边界寻址
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,     // addressV - 边界寻址
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,     // addressW - 边界寻址
            0.0f,                                   // mipLODBias
            0,                                      // maxAnisotropy - 比较采样器不使用各向异性
            D3D12_COMPARISON_FUNC_LESS_EQUAL,      // comparisonFunc - 深度比较函数
            D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE, // borderColor
            0.0f,                                   // minLOD
            D3D12_FLOAT32_MAX,                      // maxLOD
            D3D12_SHADER_VISIBILITY_ALL,            // visibility
            0);                                     // registerSpace (space0)

        // 3. 创建Root Signature描述 - SM6.6关键配置
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(
            static_cast<UINT>(rootParameters.size()), // 16个参数 (Slot 0-15) [NEW]
            rootParameters.data(),
            static_cast<UINT>(staticSamplers.size()), // NumStaticSamplers - 3个Static Sampler
            staticSamplers.data(),                     // pStaticSamplers - Static Sampler数组
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED);
            // 注意：移除了 D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
            // 原因：使用Static Sampler时不需要动态Sampler堆索引

        // 4. 序列化Root Signature
        Microsoft::WRL::ComPtr<ID3DBlob> signature;
        Microsoft::WRL::ComPtr<ID3DBlob> error;

        HRESULT hr = D3D12SerializeRootSignature(
            &rootSignatureDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &signature,
            &error);

        if (FAILED(hr))
        {
            if (error)
            {
                core::LogError("BindlessRootSignature",
                               "D3D12SerializeRootSignature failed: %s",
                               static_cast<const char*>(error->GetBufferPointer()));
            }
            else
            {
                core::LogError("BindlessRootSignature",
                               "D3D12SerializeRootSignature failed with HRESULT: 0x%08X", hr);
            }
            return false;
        }

        // 5. 创建Root Signature对象
        hr = device->CreateRootSignature(
            0, // NodeMask (单GPU为0)
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(&m_rootSignature));

        if (FAILED(hr))
        {
            core::LogError("BindlessRootSignature",
                           "CreateRootSignature failed with HRESULT: 0x%08X", hr);
            return false;
        }

        // 6. 设置调试名称
        m_rootSignature->SetName(L"Root CBV架构 Bindless Root Signature (31 DWORDs)"); // [NEW]

        core::LogInfo("BindlessRootSignature",
                      "CreateRootSignature: Root CBV架构 created successfully");
        core::LogInfo("BindlessRootSignature",
                      "  - 15 Root CBV slots (b0-b14, 30 DWORDs)"); // [NEW]
        core::LogInfo("BindlessRootSignature",
                      "  - 1 Descriptor Table (Custom Buffers, 1 DWORD)"); // [NEW]
        core::LogInfo("BindlessRootSignature",
                      "  - 3 Static Samplers (s0-s2, 0 DWORDs)");
        core::LogInfo("BindlessRootSignature",
                      "  - Custom Buffer Descriptor Table initialized: %u CBVs, b15-b%u", // [NEW]
                      MAX_CUSTOM_BUFFERS,
                      14 + MAX_CUSTOM_BUFFERS);
        core::LogInfo("BindlessRootSignature",
                      "  - Total: 31 DWORDs (48.4%% budget)"); // [NEW]

        return true;
    }
} // namespace enigma::graphic
