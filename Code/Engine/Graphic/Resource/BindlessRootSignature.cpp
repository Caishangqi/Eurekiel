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
                      "  - Root Signature: %u DWORDs (%.1f%% budget)",
                      ROOT_SIGNATURE_DWORD_COUNT,
                      ROOT_SIGNATURE_BUDGET_USED);
        core::LogInfo("BindlessRootSignature",
                      "  - Root CBV: 14 slots (Slot 0-13, 28 DWORDs)");
        core::LogInfo("BindlessRootSignature",
                      "  - Root Constants: 1 DWORD (NoiseTexture)");
        core::LogInfo("BindlessRootSignature",
                      "  - Phase 1: Slot 7 (Matrices) active");
        core::LogInfo("BindlessRootSignature",
                      "  - Flags: CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | SAMPLER_HEAP_DIRECTLY_INDEXED");

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

    void BindlessRootSignature::SetRootConstants(
        ID3D12GraphicsCommandList* commandList,
        const void*                data,
        uint32_t                   numValues,
        uint32_t                   offset) const
    {
        if (!m_initialized || !commandList || !data)
        {
            core::LogError("BindlessRootSignature",
                           "SetRootConstants: invalid parameters");
            return;
        }

        // 验证范围
        if (offset + numValues > ROOT_CONSTANTS_NUM_32BIT_VALUES)
        {
            core::LogError("BindlessRootSignature",
                           "SetRootConstants: out of range (offset=%u, numValues=%u, max=%u)",
                           offset, numValues, ROOT_CONSTANTS_NUM_32BIT_VALUES);
            return;
        }

        // 设置Root Constants (NoiseTexture index)
        commandList->SetGraphicsRoot32BitConstants(
            ROOT_CONSTANTS_NOISE_TEXTURE, // Slot 14
            numValues,
            data,
            offset);
    }

    void BindlessRootSignature::SetRootConstant32Bit(
        ID3D12GraphicsCommandList* commandList,
        uint32_t                   value,
        uint32_t                   offset) const
    {
        if (!m_initialized || !commandList)
        {
            core::LogError("BindlessRootSignature",
                           "SetRootConstant32Bit: not initialized or invalid command list");
            return;
        }

        if (offset >= ROOT_CONSTANTS_NUM_32BIT_VALUES)
        {
            core::LogError("BindlessRootSignature",
                           "SetRootConstant32Bit: offset %u out of range (max=%u)",
                           offset, ROOT_CONSTANTS_NUM_32BIT_VALUES);
            return;
        }

        // 设置单个Root Constant (NoiseTexture index)
        commandList->SetGraphicsRoot32BitConstant(
            ROOT_CONSTANTS_NOISE_TEXTURE, // Slot 14
            value,
            offset);
    }

    void BindlessRootSignature::SetRootCBV(
        ID3D12GraphicsCommandList* commandList,
        RootParameterIndex         slot,
        D3D12_GPU_VIRTUAL_ADDRESS  bufferLocation) const
    {
        if (!m_initialized || !commandList)
        {
            core::LogError("BindlessRootSignature",
                           "SetRootCBV: not initialized or invalid command list");
            return;
        }

        // 验证槽位范围 (只允许Root CBV槽位)
        if (slot < ROOT_CBV_CAMERA_AND_PLAYER || slot > ROOT_CBV_PLAYER)
        {
            core::LogError("BindlessRootSignature",
                           "SetRootCBV: invalid slot %u (must be 0-13)",
                           static_cast<uint32_t>(slot));
            return;
        }

        // 绑定Root CBV
        commandList->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(slot),
            bufferLocation);
    }

    // ========================================================================
    // 私有辅助方法
    // ========================================================================

    bool BindlessRootSignature::CreateRootSignature(ID3D12Device* device)
    {
        // 1. 创建Root Parameters数组 (Phase 1: 15个参数，Slot 15预留)
        std::vector<CD3DX12_ROOT_PARAMETER> rootParameters(ROOT_PARAMETER_COUNT - 1); // 0-14

        // [NEW] Slot 0-13: 14个Root CBV (Phase 1只初始化结构,Slot 7激活)
        for (uint32_t i = ROOT_CBV_CAMERA_AND_PLAYER; i <= ROOT_CBV_PLAYER; ++i)
        {
            rootParameters[i].InitAsConstantBufferView(
                i, // shaderRegister (b0-b13)
                0, // registerSpace (space0)
                D3D12_SHADER_VISIBILITY_ALL); // visibility
        }

        // [NEW] Slot 14: Root Constants (NoiseTexture index)
        rootParameters[ROOT_CONSTANTS_NOISE_TEXTURE].InitAsConstants(
            ROOT_CONSTANTS_NUM_32BIT_VALUES, // 1 DWORD
            14, // b14
            1, // space1 (避免与b0-b13冲突)
            D3D12_SHADER_VISIBILITY_ALL);

        // [RESERVED] Slot 15: Descriptor Table (Phase 2实现)
        // 暂不创建,Phase 1只用0-14

        // 2. 创建Root Signature描述 - SM6.6关键配置
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(
            static_cast<UINT>(rootParameters.size()), // Phase 1: 15个参数
            rootParameters.data(),
            0, // 无Static Sampler（使用动态Sampler堆）
            nullptr, // Static Sampler数组
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | // SM6.6 Bindless
            D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED); // Sampler直接索引

        // 3. 序列化Root Signature
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

        // 4. 创建Root Signature对象
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

        // 5. 设置调试名称
        m_rootSignature->SetName(L"Root CBV架构 Bindless Root Signature (30 DWORDs)");

        core::LogInfo("BindlessRootSignature",
                      "CreateRootSignature: Root CBV架构 created successfully");
        core::LogInfo("BindlessRootSignature",
                      "  - 14 Root CBV slots (b0-b13, 28 DWORDs)");
        core::LogInfo("BindlessRootSignature",
                      "  - 1 Root Constants (b14 space1, 1 DWORD)");
        core::LogInfo("BindlessRootSignature",
                      "  - Total: 30 DWORDs (46.9%% budget)");

        return true;
    }
} // namespace enigma::graphic
