/**
 * @file TestPSO.cpp
 * @brief 临时PSO管理器实现
 * @date 2025-10-21
 */

#include "TestPSO.hpp"
#include "TestTriangleShader.hpp"
#include "TestInputLayout.hpp"
#include "Engine/Graphic/Shader/Compiler/DXCCompiler.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "ThirdParty/d3dx12/d3dx12.h" // 修复路径：使用ThirdParty目录中的d3dx12.h

namespace enigma::graphic::test
{
    Microsoft::WRL::ComPtr<ID3D12PipelineState> TestPSOManager::CreateTrianglePSO()
    {
        DebuggerPrintf("[TestPSO] Creating triangle test PSO...\n");

        // ========================================
        // Step 1: 编译顶点着色器
        // ========================================
        DebuggerPrintf("[TestPSO] Compiling Vertex Shader (vs_6_6)...\n");

        const char* vsSource = GetTestTriangleVS();

        // 配置顶点着色器编译选项
        DXCCompiler compiler;
        compiler.Initialize();

        DXCCompiler::CompileOptions vsOptions;
        vsOptions.entryPoint         = "VSMain";
        vsOptions.target             = "vs_6_6";
        vsOptions.enableBindless     = true;
        vsOptions.enableOptimization = true;

        auto vsResult = compiler.CompileShader(vsSource, vsOptions);

        if (!vsResult.success)
        {
            ERROR_AND_DIE(Stringf("[TestPSO] Failed to compile Vertex Shader: %s", vsResult.errorMessage.c_str()).c_str());
            return nullptr;
        }

        DebuggerPrintf("[TestPSO] Vertex Shader compiled successfully (%zu bytes)\n",
                       vsResult.GetBytecodeSize());

        // ========================================
        // Step 2: 编译像素着色器
        // ========================================
        DebuggerPrintf("[TestPSO] Compiling Pixel Shader (ps_6_6)...\n");

        const char* psSource = GetTestTrianglePS();

        // 配置像素着色器编译选项
        DXCCompiler::CompileOptions psOptions;
        psOptions.entryPoint         = "PSMain";
        psOptions.target             = "ps_6_6";
        psOptions.enableBindless     = true;
        psOptions.enableOptimization = true;

        auto psResult = compiler.CompileShader(psSource, psOptions);

        if (!psResult.success)
        {
            ERROR_AND_DIE(Stringf("[TestPSO] Failed to compile Pixel Shader: %s",
                psResult.errorMessage.c_str()).c_str());
            return nullptr;
        }

        DebuggerPrintf("[TestPSO] Pixel Shader compiled successfully (%zu bytes)\n",
                       psResult.GetBytecodeSize());

        // ========================================
        // Step 3: 获取Input Layout
        // ========================================
        UINT                            inputElementCount = 0;
        const D3D12_INPUT_ELEMENT_DESC* inputLayout       = GetVertex_PCUTBN_InputLayout(inputElementCount);

        DebuggerPrintf("[TestPSO] Input Layout: %u elements (Vertex_PCUTBN format)\n",
                       inputElementCount);

        // ========================================
        // Step 4: 配置Graphics Pipeline State
        // ========================================
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

        // Root Signature (使用全局Bindless RootSignature)
        psoDesc.pRootSignature = D3D12RenderSystem::GetBindlessRootSignature();
        if (!psoDesc.pRootSignature)
        {
            ERROR_AND_DIE("[TestPSO] Bindless RootSignature is null!");
            return nullptr;
        }

        // Shader字节码
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vsResult.GetBytecodePtr(), vsResult.GetBytecodeSize());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(psResult.GetBytecodePtr(), psResult.GetBytecodeSize());

        // Input Layout
        psoDesc.InputLayout.pInputElementDescs = inputLayout;
        psoDesc.InputLayout.NumElements        = inputElementCount;

        // Rasterizer State (默认配置)
        psoDesc.RasterizerState          = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // 禁用背面剔除（测试用）

        // Blend State (默认配置 - 不透明渲染)
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

        // Depth Stencil State (禁用深度测试 - 测试简化)
        psoDesc.DepthStencilState               = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable   = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;

        // Render Target格式
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0]    = DXGI_FORMAT_R8G8B8A8_UNORM; // 标准RGBA8格式

        // Depth Stencil格式（虽然禁用，但仍需指定格式）
        psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN; // 不使用深度缓冲

        // Sample Desc (无MSAA)
        psoDesc.SampleMask         = UINT_MAX;
        psoDesc.SampleDesc.Count   = 1;
        psoDesc.SampleDesc.Quality = 0;

        // Primitive Topology
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        DebuggerPrintf("[TestPSO] PSO Configuration:\n");
        DebuggerPrintf("  - RTV Format: R8G8B8A8_UNORM\n");
        DebuggerPrintf("  - Depth Test: Disabled\n");
        DebuggerPrintf("  - Cull Mode: None\n");
        DebuggerPrintf("  - Primitive: Triangle List\n");

        // ========================================
        // Step 5: 创建PSO
        // ========================================
        DebuggerPrintf("[TestPSO] Creating Graphics PSO...\n");

        auto pso = D3D12RenderSystem::CreateGraphicsPSO(psoDesc);

        if (!pso)
        {
            ERROR_AND_DIE("[TestPSO] Failed to create Graphics PSO!");
            return nullptr;
        }

        DebuggerPrintf("[TestPSO] Graphics PSO created successfully!\n");
        DebuggerPrintf("[TestPSO] ========================================\n");

        return pso;
    }
} // namespace enigma::graphic::test
