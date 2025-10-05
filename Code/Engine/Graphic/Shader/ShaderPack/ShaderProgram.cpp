/**
 * @file ShaderProgram.cpp
 * @brief 着色器程序实现
 * @date 2025-10-03
 */

#include "ShaderProgram.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // 构造和析构
    // ========================================================================

    ShaderProgram::~ShaderProgram()
    {
        // ComPtr 会自动释放 PSO 和 Root Signature
        // 不需要手动调用 Release()
    }

    // ========================================================================
    // 创建方法
    // ========================================================================

    bool ShaderProgram::Create(
        CompiledShader&&                vertexShader,
        CompiledShader&&                pixelShader,
        std::optional<CompiledShader>&& geometryShader,
        ShaderType                      type
    )
    {
        // 1. 存储着色器
        m_vertexShader   = std::move(vertexShader);
        m_pixelShader    = std::move(pixelShader);
        m_geometryShader = std::move(geometryShader);
        m_type           = type;

        // 2. 设置程序名称
        m_name = m_vertexShader.name;

        // 3. 合并 ShaderDirectives (像素着色器优先)
        m_directives = m_pixelShader.directives;

        // 如果像素着色器没有指令,使用顶点着色器的指令
        if (m_directives.renderTargets.empty() && m_directives.drawBuffers.empty())
        {
            m_directives = m_vertexShader.directives;
        }

        // 4. 创建 PSO
        if (!CreatePipelineState())
        {
            ERROR_AND_DIE(Stringf("Failed to create PSO for shader program: %s", m_name.c_str()));
            return false;
        }

        return true;
    }

    // ========================================================================
    // 使用方法
    // ========================================================================

    void ShaderProgram::Use(ID3D12GraphicsCommandList* commandList)
    {
        if (!IsValid())
        {
            ERROR_AND_DIE("Attempting to use invalid shader program");
            return;
        }

        // 1. 设置 PSO (对应 OpenGL glUseProgram)
        commandList->SetPipelineState(m_pipelineState.Get());

        // 2. 设置 Root Signature
        commandList->SetGraphicsRootSignature(m_rootSignature);

        // 3. 更新 Root Constants (对应 Iris ProgramUniforms.update())
        // 注意: Root Constants 的设置由外部调用者负责
        // 这里只负责绑定 PSO 和 Root Signature
    }

    void ShaderProgram::Unbind(ID3D12GraphicsCommandList* commandList)
    {
        // DirectX 12 中不需要显式解绑 PSO
        // 保留此方法是为了与 Iris 架构一致
        //
        // 对应 Iris:
        // public static void unbind() {
        //     ProgramManager.glUseProgram(0);
        // }
    }

    // ========================================================================
    // PSO 创建
    // ========================================================================

    bool ShaderProgram::CreatePipelineState()
    {
        // 1. 获取全局 Bindless Root Signature
        ID3D12RootSignature* rootSig = D3D12RenderSystem::GetBindlessRootSignature();
        if (!rootSig)
        {
            ERROR_AND_DIE("Failed to get Bindless Root Signature");
            return false;
        }

        // 教学要点 - Root Signature 引用管理:
        // - 全局 Root Signature 由 D3D12RenderSystem 持有所有权
        // - ShaderProgram 只持有裸指针引用 (不增加引用计数)
        // - 避免 ComPtr 导致的引用计数混乱
        m_rootSignature = rootSig;

        // 2. 配置 PSO 描述符
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

        // 2.1 Root Signature
        psoDesc.pRootSignature = m_rootSignature;

        // 2.2 着色器字节码
        psoDesc.VS.pShaderBytecode = m_vertexShader.GetBytecodePtr();
        psoDesc.VS.BytecodeLength  = m_vertexShader.GetBytecodeSize();

        psoDesc.PS.pShaderBytecode = m_pixelShader.GetBytecodePtr();
        psoDesc.PS.BytecodeLength  = m_pixelShader.GetBytecodeSize();

        // 2.3 几何着色器 (可选)
        if (m_geometryShader.has_value())
        {
            psoDesc.GS.pShaderBytecode = m_geometryShader->GetBytecodePtr();
            psoDesc.GS.BytecodeLength  = m_geometryShader->GetBytecodeSize();
        }

        // 2.4 Blend 状态 (根据 ShaderDirectives 配置)
        ConfigureBlendState(psoDesc.BlendState);

        // 2.5 光栅化状态 (根据 ShaderDirectives 配置)
        ConfigureRasterizerState(psoDesc.RasterizerState);

        // 2.6 深度模板状态 (根据 ShaderDirectives 配置)
        ConfigureDepthStencilState(psoDesc.DepthStencilState);

        // 2.7 固定 Input Layout (全局统一顶点格式 - Vertex_PCUTBN)
        static const D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };
        psoDesc.InputLayout.pInputElementDescs = inputLayout;
        psoDesc.InputLayout.NumElements        = _countof(inputLayout);

        // 2.8 图元拓扑
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        // 2.9 渲染目标格式 (根据 ShaderDirectives 配置)
        psoDesc.NumRenderTargets = static_cast<UINT>(m_directives.renderTargets.size());
        if (psoDesc.NumRenderTargets == 0)
        {
            psoDesc.NumRenderTargets = 1; // 默认至少有一个 RT
        }

        for (UINT i = 0; i < psoDesc.NumRenderTargets && i < 8; ++i)
        {
            // 从 ShaderDirectives.rtFormats 获取格式
            auto it = m_directives.rtFormats.find(Stringf("colortex%u", i));
            if (it != m_directives.rtFormats.end())
            {
                psoDesc.RTVFormats[i] = it->second;
            }
            else
            {
                psoDesc.RTVFormats[i] = DXGI_FORMAT_R8G8B8A8_UNORM; // 默认格式
            }
        }

        // 2.10 深度模板格式
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

        // 2.11 采样描述
        psoDesc.SampleDesc.Count   = 1;
        psoDesc.SampleDesc.Quality = 0;

        // 2.12 采样掩码
        psoDesc.SampleMask = UINT_MAX;

        // 3. 创建 PSO
        m_pipelineState = D3D12RenderSystem::CreateGraphicsPSO(psoDesc);
        if (!m_pipelineState)
        {
            ERROR_AND_DIE(Stringf("Failed to create PSO for %s", m_name.c_str()));
            return false;
        }

        return true;
    }

    // ========================================================================
    // PSO 状态配置
    // ========================================================================

    void ShaderProgram::ConfigureBlendState(D3D12_BLEND_DESC& blendDesc)
    {
        // 默认禁用混合
        blendDesc.AlphaToCoverageEnable  = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;

        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            blendDesc.RenderTarget[i].BlendEnable           = FALSE;
            blendDesc.RenderTarget[i].LogicOpEnable         = FALSE;
            blendDesc.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        // 根据 ShaderDirectives.blendMode 配置
        if (m_directives.blendMode.has_value())
        {
            const std::string& blendMode = m_directives.blendMode.value();

            if (blendMode == "ADD")
            {
                blendDesc.RenderTarget[0].BlendEnable    = TRUE;
                blendDesc.RenderTarget[0].SrcBlend       = D3D12_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlend      = D3D12_BLEND_ONE;
                blendDesc.RenderTarget[0].BlendOp        = D3D12_BLEND_OP_ADD;
                blendDesc.RenderTarget[0].SrcBlendAlpha  = D3D12_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
                blendDesc.RenderTarget[0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;
            }
            else if (blendMode == "ALPHA")
            {
                blendDesc.RenderTarget[0].BlendEnable    = TRUE;
                blendDesc.RenderTarget[0].SrcBlend       = D3D12_BLEND_SRC_ALPHA;
                blendDesc.RenderTarget[0].DestBlend      = D3D12_BLEND_INV_SRC_ALPHA;
                blendDesc.RenderTarget[0].BlendOp        = D3D12_BLEND_OP_ADD;
                blendDesc.RenderTarget[0].SrcBlendAlpha  = D3D12_BLEND_ONE;
                blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
                blendDesc.RenderTarget[0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;
            }
            // 可扩展更多 Blend 模式...
        }
    }

    void ShaderProgram::ConfigureDepthStencilState(D3D12_DEPTH_STENCIL_DESC& depthStencilDesc)
    {
        // 默认启用深度测试
        depthStencilDesc.DepthEnable    = TRUE;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depthStencilDesc.DepthFunc      = D3D12_COMPARISON_FUNC_LESS;
        depthStencilDesc.StencilEnable  = FALSE;

        // 根据 ShaderDirectives 配置
        if (m_directives.depthTest.has_value())
        {
            const std::string& depthTest = m_directives.depthTest.value();

            if (depthTest == "LESS")
            {
                depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
            }
            else if (depthTest == "LEQUAL")
            {
                depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
            }
            else if (depthTest == "EQUAL")
            {
                depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
            }
            else if (depthTest == "ALWAYS")
            {
                depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            }
            // 可扩展更多深度测试模式...
        }

        // 根据 depthWrite 配置
        if (m_directives.depthWrite.has_value())
        {
            if (m_directives.depthWrite.value())
            {
                depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            }
            else
            {
                depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
            }
        }
    }

    void ShaderProgram::ConfigureRasterizerState(D3D12_RASTERIZER_DESC& rasterDesc)
    {
        // 默认配置
        rasterDesc.FillMode              = D3D12_FILL_MODE_SOLID;
        rasterDesc.CullMode              = D3D12_CULL_MODE_BACK;
        rasterDesc.FrontCounterClockwise = FALSE;
        rasterDesc.DepthBias             = 0;
        rasterDesc.DepthBiasClamp        = 0.0f;
        rasterDesc.SlopeScaledDepthBias  = 0.0f;
        rasterDesc.DepthClipEnable       = TRUE;
        rasterDesc.MultisampleEnable     = FALSE;
        rasterDesc.AntialiasedLineEnable = FALSE;
        rasterDesc.ForcedSampleCount     = 0;
        rasterDesc.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // 根据 ShaderDirectives.cullFace 配置
        if (m_directives.cullFace.has_value())
        {
            const std::string& cullFace = m_directives.cullFace.value();

            if (cullFace == "BACK")
            {
                rasterDesc.CullMode = D3D12_CULL_MODE_BACK;
            }
            else if (cullFace == "FRONT")
            {
                rasterDesc.CullMode = D3D12_CULL_MODE_FRONT;
            }
            else if (cullFace == "NONE")
            {
                rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
            }
        }
    }
} // namespace enigma::graphic
