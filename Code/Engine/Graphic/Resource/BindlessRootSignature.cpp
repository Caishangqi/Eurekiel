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
                      "Initialized successfully (RENDERTARGETS混合架构)");
        core::LogInfo("BindlessRootSignature",
                      "  - Root Constants: %u DWORDs (%u bytes)",
                      ROOT_CONSTANTS_NUM_32BIT_VALUES,
                      ROOT_CONSTANTS_SIZE_BYTES);
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

        // 设置Root Constants
        commandList->SetGraphicsRoot32BitConstants(
            ROOT_PARAMETER_INDEX_CONSTANTS, // Root Parameter索引 (0)
            numValues, // 32位值数量
            data, // 数据指针
            offset); // 偏移量（32位值单位）
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

        // 优化的单值设置
        commandList->SetGraphicsRoot32BitConstant(
            ROOT_PARAMETER_INDEX_CONSTANTS,
            value,
            offset);
    }

    // ========================================================================
    // 私有辅助方法
    // ========================================================================

    bool BindlessRootSignature::CreateRootSignature(ID3D12Device* device)
    {
        // 1. 创建Root Parameter - 只有1个32位常量参数
        CD3DX12_ROOT_PARAMETER rootParameter;
        rootParameter.InitAsConstants(
            ROOT_CONSTANTS_NUM_32BIT_VALUES, // 11 DWORDs = 44 bytes
            0, // b0 (register)
            0, // space0
            D3D12_SHADER_VISIBILITY_ALL); // 所有着色器阶段可见

        // 2. 创建Root Signature描述 - SM6.6关键配置
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(
            1, // 只有1个Root Parameter
            &rootParameter, // Root Parameter数组
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
        m_rootSignature->SetName(L"RENDERTARGETS Bindless Root Signature (44 bytes)");

        core::LogInfo("BindlessRootSignature",
                      "CreateRootSignature: RENDERTARGETS Bindless Root Signature created successfully");

        return true;
    }
} // namespace enigma::graphic
