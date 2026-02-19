/**
 * @file PSOManager.cpp
 * @brief PSO动态管理器实现
 * @date 2025-11-01
 */

#include "PSOManager.hpp"
#include "Engine/Graphic/Shader/Program/ShaderProgram.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Helper/StencilHelper.hpp"
#include "Engine/Graphic/Resource/VertexLayout/VertexLayout.hpp" // [NEW] VertexLayout for InputLayout
#include "Engine/Graphic/Resource/VertexLayout/VertexLayoutRegistry.hpp" // [NEW] Default layout fallback
#include "Engine/Core/ErrorWarningAssert.hpp"
#include <functional>

#include "Engine/Core/Logger/LoggerAPI.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // PSOKey implementation
    // ========================================================================
    bool PSOKey::operator==(const PSOKey& other) const
    {
        if (shaderProgram != other.shaderProgram)
            return false;
        // [NEW] VertexLayout pointer comparison
        if (vertexLayout != other.vertexLayout)
            return false;
        if (depthFormat != other.depthFormat)
            return false;
        if (blendConfig != other.blendConfig)
            return false;
        if (depthConfig != other.depthConfig)
            return false;
        if (!(stencilDetail == other.stencilDetail))
            return false;
        // [NEW] RasterizationConfig comparison
        if (!(rasterizationConfig == other.rasterizationConfig))
            return false;

        for (int i = 0; i < 8; ++i)
        {
            if (rtFormats[i] != other.rtFormats[i])
                return false;
        }

        return true;
    }

    std::size_t PSOKey::GetHash() const
    {
        std::size_t hash = 0;

        // Hash ShaderProgram pointer
        hash ^= std::hash<const void*>{}(shaderProgram);

        // [NEW] Hash VertexLayout pointer
        hash ^= std::hash<const void*>{}(vertexLayout) << 1;

        // Hash RT format
        for (int i = 0; i < 8; ++i)
        {
            hash ^= std::hash<DXGI_FORMAT>{}(rtFormats[i]) << i;
        }

        // Hash depth format
        hash ^= std::hash<DXGI_FORMAT>{}(depthFormat) << 8;

        // [REFACTORED] Hash BlendConfig (8 fields packed)
        uint32_t blendKey = (blendConfig.blendEnabled ? 1u : 0u) |
            (static_cast<uint32_t>(blendConfig.srcBlend) << 1) |
            (static_cast<uint32_t>(blendConfig.destBlend) << 6) |
            (static_cast<uint32_t>(blendConfig.blendOp) << 11) |
            (static_cast<uint32_t>(blendConfig.srcBlendAlpha) << 14) |
            (static_cast<uint32_t>(blendConfig.destBlendAlpha) << 19) |
            (static_cast<uint32_t>(blendConfig.blendOpAlpha) << 24) |
            (static_cast<uint32_t>(blendConfig.renderTargetWriteMask) << 27);
        hash ^= std::hash<uint32_t>{}(blendKey) << 16;

        // Hash depth config (3 fields)
        hash ^= std::hash<bool>{}(depthConfig.depthTestEnabled) << 24;
        hash ^= std::hash<bool>{}(depthConfig.depthWriteEnabled) << 25;
        hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(depthConfig.depthFunc)) << 26;

        // Hash StencilTestDetail (all 14 fields)
        hash ^= std::hash<bool>{}(stencilDetail.enable) << 25;
        hash ^= std::hash<uint8_t>{}(stencilDetail.refValue) << 26;
        hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(stencilDetail.stencilFunc)) << 27;
        hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(stencilDetail.stencilPassOp)) << 28;
        hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(stencilDetail.stencilFailOp)) << 29;
        hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(stencilDetail.stencilDepthFailOp)) << 30;
        hash ^= std::hash<uint8_t>{}(stencilDetail.stencilReadMask) << 31;
        hash ^= std::hash<uint8_t>{}(stencilDetail.stencilWriteMask);
        hash ^= std::hash<bool>{}(stencilDetail.depthWriteEnable) << 1;
        hash ^= std::hash<bool>{}(stencilDetail.useSeparateFrontBack) << 2;
        hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(stencilDetail.backFaceStencilFunc)) << 3;
        hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(stencilDetail.backFaceStencilPassOp)) << 4;
        hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(stencilDetail.backFaceStencilFailOp)) << 5;
        hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(stencilDetail.backFaceStencilDepthFailOp)) << 6;

        // [NEW] RasterizationConfig Hash (optimized strategy)
        // Strategy 1: High-frequency fields always hashed
        uint32_t rastKey = (static_cast<uint32_t>(rasterizationConfig.fillMode) << 16) |
            (static_cast<uint32_t>(rasterizationConfig.cullMode) << 8) |
            static_cast<uint32_t>(rasterizationConfig.windingOrder);
        hash ^= std::hash<uint32_t>{}(rastKey) << 10;

        // Strategy 2: Sparse fields only when non-default
        if (rasterizationConfig.depthBias != 0)
            hash ^= std::hash<int32_t>{}(rasterizationConfig.depthBias) << 11;
        if (rasterizationConfig.depthBiasClamp != 0.0f)
            hash ^= std::hash<float>{}(rasterizationConfig.depthBiasClamp) << 12;
        if (rasterizationConfig.slopeScaledDepthBias != 0.0f)
            hash ^= std::hash<float>{}(rasterizationConfig.slopeScaledDepthBias) << 13;

        // Strategy 3: Pack booleans into single byte
        uint8_t boolFlags = 0;
        if (rasterizationConfig.depthClipEnabled) boolFlags |= 0x01;
        if (rasterizationConfig.multisampleEnabled) boolFlags |= 0x02;
        if (rasterizationConfig.antialiasedLineEnabled) boolFlags |= 0x04;
        if (rasterizationConfig.conservativeRasterEnabled) boolFlags |= 0x08;
        hash ^= std::hash<uint8_t>{}(boolFlags) << 14;

        if (rasterizationConfig.forcedSampleCount != 0)
            hash ^= std::hash<uint32_t>{}(rasterizationConfig.forcedSampleCount) << 15;

        return hash;
    }

    // ========================================================================
    // PSOManager implementation
    // ========================================================================
    ID3D12PipelineState* PSOManager::GetOrCreatePSO(
        const ShaderProgram*       shaderProgram,
        const VertexLayout*        layout, // [NEW] Vertex layout parameters
        const DXGI_FORMAT          rtFormats[8],
        DXGI_FORMAT                depthFormat,
        const BlendConfig&         blendConfig, // [REFACTORED] BlendConfig replaces BlendMode
        const DepthConfig&         depthConfig, // [REFACTORED] DepthConfig replaces DepthMode
        const StencilTestDetail&   stencilDetail,
        const RasterizationConfig& rasterizationConfig
    )
    {
        PSOKey key;
        key.shaderProgram = shaderProgram;
        key.vertexLayout  = layout; // [NEW] Set vertex layout
        for (int i = 0; i < 8; ++i)
        {
            key.rtFormats[i] = rtFormats[i];
        }
        key.depthFormat         = depthFormat;
        key.blendConfig         = blendConfig; // [REFACTORED] Use BlendConfig
        key.depthConfig         = depthConfig; // [REFACTORED] Use DepthConfig
        key.stencilDetail       = stencilDetail;
        key.rasterizationConfig = rasterizationConfig; // [NEW] Set rasterization config

        // 2. Find cache
        auto it = m_psoCache.find(key);
        if (it != m_psoCache.end())
        {
            return it->second.Get();
        }

        // 3. Create a new PSO
        auto pso = CreatePSO(key);
        if (!pso)
        {
            return nullptr;
        }

        // 4. Cache and return
        m_psoCache[key] = pso;
        return pso.Get();
    }

    void PSOManager::ClearCache()
    {
        m_psoCache.clear();
    }

    Microsoft::WRL::ComPtr<ID3D12PipelineState> PSOManager::CreatePSO(const PSOKey& key)
    {
        // 1. Get Root Signature
        ID3D12RootSignature* rootSig = D3D12RenderSystem::GetBindlessRootSignature()->GetRootSignature();
        if (!rootSig)
        {
            ERROR_RECOVERABLE("Failed to get Bindless Root Signature");
            return nullptr;
        }

        // 2. Configure PSO descriptor
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature                     = rootSig;

        // 2.1 Shader bytecode (obtained from ShaderProgram private members, PSOManager is a friend class)
        psoDesc.VS.pShaderBytecode = key.shaderProgram->m_vertexShader.GetBytecodePtr();
        psoDesc.VS.BytecodeLength  = key.shaderProgram->m_vertexShader.GetBytecodeSize();

        psoDesc.PS.pShaderBytecode = key.shaderProgram->m_pixelShader.GetBytecodePtr();
        psoDesc.PS.BytecodeLength  = key.shaderProgram->m_pixelShader.GetBytecodeSize();

        // Geometry shaders are not supported yet
        // if (key.shaderProgram->m_geometryShader.has_value())
        // {
        // psoDesc.GS.pShaderBytecode = key.shaderProgram->m_geometryShader->GetBytecodePtr();
        // psoDesc.GS.BytecodeLength = key.shaderProgram->m_geometryShader->GetBytecodeSize();
        // }

        // 2.2 Blend state (using key.blendConfig)
        ConfigureBlendState(psoDesc.BlendState, key.blendConfig);

        // 2.3 Rasterization status (using key.rasterizationConfig)
        ConfigureRasterizerState(psoDesc.RasterizerState, key.rasterizationConfig);

        // 2.4 Depth stencil state (using key.depthConfig and key.stencilDetail)
        ConfigureDepthStencilState(psoDesc.DepthStencilState, key.depthConfig, key.stencilDetail);

        // 2.5 Input Layout - [REFACTORED] Use VertexLayout from key instead of hardcoded array
        const VertexLayout* layout = key.vertexLayout;
        if (!layout)
        {
            // [FALLBACK] Use default layout if not specified
            layout = VertexLayoutRegistry::GetDefault();
            LogWarn("PSOManager", "PSOManager::CreatePSO:: VertexLayout is null, using default layout '%s'", layout ? layout->GetLayoutName() : "NONE");
        }

        if (layout)
        {
            psoDesc.InputLayout.pInputElementDescs = layout->GetInputElements();
            psoDesc.InputLayout.NumElements        = layout->GetInputElementCount();
        }
        else
        {
            // [CRITICAL ERROR] No layout available - this should never happen
            ERROR_RECOVERABLE("PSOManager::CreatePSO:: No VertexLayout available, PSO creation will fail");
            return nullptr;
        }

        // 2.6 Graph element topology
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        // 2.7 Render target format (use key.rtFormats, fix hard coding problem)
        // [FIX] First traverse all RTs and find the last non-UNKNOWN index
        UINT lastValidRTIndex = 0;
        for (UINT i = 0; i < 8; ++i)
        {
            if (key.rtFormats[i] != DXGI_FORMAT_UNKNOWN)
            {
                psoDesc.RTVFormats[i] = key.rtFormats[i];
                lastValidRTIndex      = i; // [FIX] Record the index of the last valid RT
            }
            else
            {
                psoDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
            }
        }

        // [FIX] NumRenderTargets = last valid RT index + 1
        psoDesc.NumRenderTargets = lastValidRTIndex + 1;

        // Fallback: If there is no RT, use the default configuration
        if (psoDesc.NumRenderTargets == 0)
        {
            psoDesc.NumRenderTargets = 1;
            psoDesc.RTVFormats[0]    = DXGI_FORMAT_R8G8B8A8_UNORM;
        }

        // 2.8 depth format (using key.depthFormat)
        psoDesc.DSVFormat = key.depthFormat;

        // 2.9 Sampling description
        psoDesc.SampleDesc.Count   = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.SampleMask         = UINT_MAX;

        // 3. Create PSO
        auto pso = D3D12RenderSystem::CreateGraphicsPSO(psoDesc);
        if (!pso)
        {
            // Fallback: Try again with default configuration
            ERROR_RECOVERABLE("PSO creation failed, trying fallback configuration");

            PSOKey fallbackKey      = key;
            fallbackKey.blendConfig = BlendConfig::Opaque(); // [REFACTORED] Use BlendConfig
            fallbackKey.depthConfig = DepthConfig::Enabled(); // [REFACTORED] Use DepthConfig

            if (fallbackKey == key)
            {
                return nullptr; // It is already the default configuration and cannot be Fallback
            }

            return CreatePSO(fallbackKey);
        }

        return pso;
    }

    // ========================================================================
    // State configuration helper method
    // ========================================================================
    void PSOManager::ConfigureBlendState(D3D12_BLEND_DESC& blendDesc, const BlendConfig& blendConfig)
    {
        blendDesc.AlphaToCoverageEnable  = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;

        // [REFACTORED] Direct configuration from BlendConfig struct
        // No more switch-case on BlendMode enum - cleaner and more flexible
        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            blendDesc.RenderTarget[i].BlendEnable           = blendConfig.blendEnabled ? TRUE : FALSE;
            blendDesc.RenderTarget[i].LogicOpEnable         = FALSE;
            blendDesc.RenderTarget[i].SrcBlend              = blendConfig.srcBlend;
            blendDesc.RenderTarget[i].DestBlend             = blendConfig.destBlend;
            blendDesc.RenderTarget[i].BlendOp               = blendConfig.blendOp;
            blendDesc.RenderTarget[i].SrcBlendAlpha         = blendConfig.srcBlendAlpha;
            blendDesc.RenderTarget[i].DestBlendAlpha        = blendConfig.destBlendAlpha;
            blendDesc.RenderTarget[i].BlendOpAlpha          = blendConfig.blendOpAlpha;
            blendDesc.RenderTarget[i].RenderTargetWriteMask = blendConfig.renderTargetWriteMask;
        }
    }

    void PSOManager::ConfigureDepthStencilState(
        D3D12_DEPTH_STENCIL_DESC& depthStencilDesc,
        const DepthConfig&        depthConfig,
        const StencilTestDetail&  stencilDetail)
    {
        // [REFACTORED] Direct configuration from DepthConfig struct
        // No more switch-case on DepthMode enum - cleaner and more flexible
        depthStencilDesc.DepthEnable    = depthConfig.depthTestEnabled ? TRUE : FALSE;
        depthStencilDesc.DepthWriteMask = depthConfig.depthWriteEnabled ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        depthStencilDesc.DepthFunc      = depthConfig.depthFunc;

        // [STEP 2] Configure stencil test (existing integration)
        StencilHelper::ConfigureStencilState(depthStencilDesc, stencilDetail);
    }

    void PSOManager::ConfigureRasterizerState(D3D12_RASTERIZER_DESC& rasterDesc, const RasterizationConfig& config)
    {
        // [NEW] Core configuration from RasterizationConfig
        rasterDesc.FillMode              = config.fillMode;
        rasterDesc.CullMode              = config.cullMode;
        rasterDesc.FrontCounterClockwise = (config.windingOrder == RasterizeWindingOrder::CounterClockwise);

        // [NEW] Depth bias configuration (for shadow mapping)
        rasterDesc.DepthBias            = config.depthBias;
        rasterDesc.DepthBiasClamp       = config.depthBiasClamp;
        rasterDesc.SlopeScaledDepthBias = config.slopeScaledDepthBias;

        // [NEW] Feature switches
        rasterDesc.DepthClipEnable       = config.depthClipEnabled;
        rasterDesc.MultisampleEnable     = config.multisampleEnabled;
        rasterDesc.AntialiasedLineEnable = config.antialiasedLineEnabled;
        rasterDesc.ForcedSampleCount     = config.forcedSampleCount;

        // [NEW] Conservative rasterization
        rasterDesc.ConservativeRaster = config.conservativeRasterEnabled
                                            ? D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON
                                            : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    }
} // namespace enigma::graphic
