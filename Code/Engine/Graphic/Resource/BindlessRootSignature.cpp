#include "BindlessRootSignature.hpp"
#include "../Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

#include "ThirdParty/d3dx12/d3dx12.h"

namespace enigma::graphic
{
    // ========================================================================
    // BindlessRootSignature Implementation
    // ========================================================================

    BindlessRootSignature::BindlessRootSignature()
        : m_rootSignature(nullptr)
          , m_initialized(false)
    {
        // Constructor: Only initialize members, actual creation in Initialize()
    }

    bool BindlessRootSignature::Initialize()
    {
        if (m_initialized)
        {
            core::LogWarn("BindlessRootSignature", "Already initialized");
            return true;
        }

        // 1. Get D3D12 device
        auto device = D3D12RenderSystem::GetDevice();
        if (!device)
        {
            core::LogError("BindlessRootSignature", "Failed to get D3D12 device");
            return false;
        }

        // 2. Create Root Signature
        if (!CreateRootSignature(device))
        {
            core::LogError("BindlessRootSignature", "Failed to create root signature");
            return false;
        }

        m_initialized = true;

        core::LogInfo("BindlessRootSignature",
                      "Initialized successfully (Root CBV + Dynamic Sampler)"); // [REFACTORED]
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
                      "  - Flags: CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | SAMPLER_HEAP_DIRECTLY_INDEXED"); // [REFACTORED]

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
    // Private Helper Methods
    // ========================================================================

    bool BindlessRootSignature::CreateRootSignature(ID3D12Device* device)
    {
        // 1. Create Root Parameters array (16 parameters: Slot 0-15)
        std::vector<CD3DX12_ROOT_PARAMETER> rootParameters(ROOT_PARAMETER_COUNT);

        // Slot 0-14: 15 Root CBVs
        for (uint32_t i = ROOT_CBV_UNDEFINE_0; i <= ROOT_CBV_UNDEFINE_14; ++i)
        {
            rootParameters[i].InitAsConstantBufferView(
                i, // shaderRegister (b0-b14)
                0, // registerSpace (space0)
                D3D12_SHADER_VISIBILITY_ALL);
        }

        // ============================================================================
        // [Slot 15] Custom Buffer Descriptor Table (space1 isolation)
        // ============================================================================
        // 
        // Design: Use registerSpace=1 (space1) for isolation from Root CBV
        // 
        // Architecture:
        // - Root CBV (Slot 0-14): space0 b0-b14, direct binding
        // - Custom Buffer Table:  space1, supports b0-b99
        // 
        // Mapping: Table[N] -> register(bN, space1)
        // Example: Table[88] -> register(b88, space1)
        // ============================================================================
        CD3DX12_DESCRIPTOR_RANGE customBufferRange;
        customBufferRange.Init(
            D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
            MAX_CUSTOM_BUFFERS, // 100 CBVs
            0, // baseShaderRegister (b0 in space1)
            1); // registerSpace = space1

        rootParameters[ROOT_DESCRIPTOR_TABLE_CUSTOM].InitAsDescriptorTable(
            1,
            &customBufferRange,
            D3D12_SHADER_VISIBILITY_ALL);

        // ============================================================================
        // [REFACTORED] Dynamic Sampler System - Remove Static Samplers
        // ============================================================================
        // 
        // Design Decision: Use Dynamic Sampler via SAMPLER_HEAP_DIRECTLY_INDEXED flag
        // instead of Static Samplers for runtime flexibility.
        // 
        // Benefits:
        // - Runtime sampler configuration (SetSamplerState API)
        // - Shader uses SamplerDescriptorHeap[index] for dynamic access
        // - SamplerIndicesBuffer (b7) provides bindless indices
        // 
        // Migration from Static Sampler:
        // - [OLD] SamplerState linearSampler : register(s0);
        // - [NEW] SamplerDescriptorHeap[samplerIndices.linearSampler]
        // ============================================================================

        // 3. Create Root Signature Desc - SM6.6 Key Configuration
        // [REFACTORED] Remove static samplers, add SAMPLER_HEAP_DIRECTLY_INDEXED flag
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(
            static_cast<UINT>(rootParameters.size()), // 16 parameters (Slot 0-15)
            rootParameters.data(),
            0, // [REFACTORED] NumStaticSamplers = 0 (Dynamic Sampler System)
            nullptr, // [REFACTORED] pStaticSamplers = nullptr
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
            D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED); // [NEW] Enable dynamic sampler access

        // 4. Serialize Root Signature
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

        // 5. Create Root Signature object
        hr = device->CreateRootSignature(
            0, // NodeMask (0 for single GPU)
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(&m_rootSignature));

        if (FAILED(hr))
        {
            core::LogError("BindlessRootSignature",
                           "CreateRootSignature failed with HRESULT: 0x%08X", hr);
            return false;
        }

        // 6. Set debug name
        m_rootSignature->SetName(L"Bindless Root Signature (Dynamic Sampler, 31 DWORDs)"); // [REFACTORED]

        core::LogInfo("BindlessRootSignature",
                      "CreateRootSignature: Root CBV + Dynamic Sampler created successfully"); // [REFACTORED]
        core::LogInfo("BindlessRootSignature",
                      "  - 15 Root CBV slots (b0-b14, 30 DWORDs)");
        core::LogInfo("BindlessRootSignature",
                      "  - 1 Descriptor Table (Custom Buffers, 1 DWORD)");
        core::LogInfo("BindlessRootSignature",
                      "  - Dynamic Samplers via SAMPLER_HEAP_DIRECTLY_INDEXED (0 DWORDs)"); // [REFACTORED]
        core::LogInfo("BindlessRootSignature",
                      "  - Custom Buffer Descriptor Table initialized: %u CBVs, b15-b%u",
                      MAX_CUSTOM_BUFFERS,
                      14 + MAX_CUSTOM_BUFFERS);
        core::LogInfo("BindlessRootSignature",
                      "  - Total: 31 DWORDs (48.4%% budget)");

        return true;
    }
} // namespace enigma::graphic
