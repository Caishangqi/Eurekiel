/**
 * @file PSOManager.cpp
 * @brief PSO动态管理器实现
 * @date 2025-11-01
 */

#include "PSOManager.hpp"
#include "Engine/Graphic/Shader/ShaderPack/ShaderProgram.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Helper/StencilHelper.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include <functional>

#include "Engine/Core/Logger/LoggerAPI.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // PSOKey 实现
    // ========================================================================

    bool PSOKey::operator==(const PSOKey& other) const
    {
        if (shaderProgram != other.shaderProgram)
            return false;
        if (depthFormat != other.depthFormat)
            return false;
        if (blendMode != other.blendMode)
            return false;
        if (depthMode != other.depthMode)
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

        // Hash ShaderProgram指针
        hash ^= std::hash<const void*>{}(shaderProgram);

        // Hash RT格式
        for (int i = 0; i < 8; ++i)
        {
            hash ^= std::hash<DXGI_FORMAT>{}(rtFormats[i]) << i;
        }

        // Hash 深度格式
        hash ^= std::hash<DXGI_FORMAT>{}(depthFormat) << 8;

        // Hash 混合模式
        hash ^= std::hash<uint8_t>{}(static_cast<uint8_t>(blendMode)) << 16;

        // Hash 深度模式
        hash ^= std::hash<uint8_t>{}(static_cast<uint8_t>(depthMode)) << 24;

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
    // PSOManager 实现
    // ========================================================================

    ID3D12PipelineState* PSOManager::GetOrCreatePSO(
        const ShaderProgram*       shaderProgram,
        const DXGI_FORMAT          rtFormats[8],
        DXGI_FORMAT                depthFormat,
        BlendMode                  blendMode,
        DepthMode                  depthMode,
        const StencilTestDetail&   stencilDetail,
        const RasterizationConfig& rasterizationConfig
    )
    {
        // [DIAGNOSTIC] Log input stencil configuration
        LogInfo("PSOManager", "[GetOrCreatePSO] stencil.enable=%d, func=%d, passOp=%d, writeMask=0x%02X",
                stencilDetail.enable, stencilDetail.stencilFunc, stencilDetail.stencilPassOp, stencilDetail.stencilWriteMask);

        // 1. 构建PSOKey
        PSOKey key;
        key.shaderProgram = shaderProgram;
        for (int i = 0; i < 8; ++i)
        {
            key.rtFormats[i] = rtFormats[i];
        }
        key.depthFormat         = depthFormat;
        key.blendMode           = blendMode;
        key.depthMode           = depthMode;
        key.stencilDetail       = stencilDetail;
        key.rasterizationConfig = rasterizationConfig; // [NEW] Set rasterization config

        // 2. 查找缓存
        auto it = m_psoCache.find(key);
        if (it != m_psoCache.end())
        {
            LogInfo("PSOManager", "[PSO Cache HIT] Returning cached PSO");
            return it->second.Get();
        }

        LogInfo("PSOManager", "[PSO Cache MISS] Creating new PSO");

        // 3. 创建新PSO
        auto pso = CreatePSO(key);
        if (!pso)
        {
            return nullptr;
        }

        // 4. 缓存并返回
        m_psoCache[key] = pso;
        return pso.Get();
    }

    void PSOManager::ClearCache()
    {
        m_psoCache.clear();
    }

    Microsoft::WRL::ComPtr<ID3D12PipelineState> PSOManager::CreatePSO(const PSOKey& key)
    {
        // 1. 获取Root Signature
        ID3D12RootSignature* rootSig = D3D12RenderSystem::GetBindlessRootSignature()->GetRootSignature();
        if (!rootSig)
        {
            ERROR_RECOVERABLE("Failed to get Bindless Root Signature");
            return nullptr;
        }

        // 2. 配置PSO描述符
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature                     = rootSig;

        // 2.1 着色器字节码（从ShaderProgram私有成员获取，PSOManager是友元类）
        psoDesc.VS.pShaderBytecode = key.shaderProgram->m_vertexShader.GetBytecodePtr();
        psoDesc.VS.BytecodeLength  = key.shaderProgram->m_vertexShader.GetBytecodeSize();

        psoDesc.PS.pShaderBytecode = key.shaderProgram->m_pixelShader.GetBytecodePtr();
        psoDesc.PS.BytecodeLength  = key.shaderProgram->m_pixelShader.GetBytecodeSize();

        // 几何着色器暂不支持
        // if (key.shaderProgram->m_geometryShader.has_value())
        // {
        //     psoDesc.GS.pShaderBytecode = key.shaderProgram->m_geometryShader->GetBytecodePtr();
        //     psoDesc.GS.BytecodeLength  = key.shaderProgram->m_geometryShader->GetBytecodeSize();
        // }

        // 2.2 Blend状态（使用key.blendMode）
        ConfigureBlendState(psoDesc.BlendState, key.blendMode);

        // 2.3 光栅化状态（使用key.rasterizationConfig）
        ConfigureRasterizerState(psoDesc.RasterizerState, key.rasterizationConfig);

        // 2.4 深度模板状态（使用key.depthMode和key.stencilDetail）
        ConfigureDepthStencilState(psoDesc.DepthStencilState, key.depthMode, key.stencilDetail);

        // 2.5 Input Layout
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

        // 2.6 图元拓扑
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        // 2.7 渲染目标格式（使用key.rtFormats，修复硬编码问题）
        // [FIX] 先遍历所有RT，找到最后一个非UNKNOWN的索引
        UINT lastValidRTIndex = 0;
        for (UINT i = 0; i < 8; ++i)
        {
            if (key.rtFormats[i] != DXGI_FORMAT_UNKNOWN)
            {
                psoDesc.RTVFormats[i] = key.rtFormats[i];
                lastValidRTIndex      = i; // [FIX] 记录最后一个有效RT的索引
            }
            else
            {
                psoDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
            }
        }

        // [FIX] NumRenderTargets = 最后一个有效RT索引 + 1
        psoDesc.NumRenderTargets = lastValidRTIndex + 1;

        // Fallback: 如果没有任何RT，使用默认配置
        if (psoDesc.NumRenderTargets == 0)
        {
            psoDesc.NumRenderTargets = 1;
            psoDesc.RTVFormats[0]    = DXGI_FORMAT_R8G8B8A8_UNORM;
        }

        // 2.8 深度格式（使用key.depthFormat）
        psoDesc.DSVFormat = key.depthFormat;

        // 2.9 采样描述
        psoDesc.SampleDesc.Count   = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.SampleMask         = UINT_MAX;

        // 3. 创建PSO
        auto pso = D3D12RenderSystem::CreateGraphicsPSO(psoDesc);
        if (!pso)
        {
            // Fallback: 使用默认配置重试
            ERROR_RECOVERABLE("PSO creation failed, trying fallback configuration");

            PSOKey fallbackKey    = key;
            fallbackKey.blendMode = BlendMode::Opaque;
            fallbackKey.depthMode = DepthMode::Enabled;

            if (fallbackKey == key)
            {
                return nullptr; // 已经是默认配置，无法Fallback
            }

            return CreatePSO(fallbackKey);
        }

        return pso;
    }

    // ========================================================================
    // 状态配置辅助方法
    // ========================================================================

    void PSOManager::ConfigureBlendState(D3D12_BLEND_DESC& blendDesc, BlendMode blendMode)
    {
        blendDesc.AlphaToCoverageEnable  = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;

        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            blendDesc.RenderTarget[i].BlendEnable           = FALSE;
            blendDesc.RenderTarget[i].LogicOpEnable         = FALSE;
            blendDesc.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        switch (blendMode)
        {
        case BlendMode::Alpha:
        case BlendMode::NonPremultiplied:
            blendDesc.RenderTarget[0].BlendEnable = TRUE;
            blendDesc.RenderTarget[0].SrcBlend       = D3D12_BLEND_SRC_ALPHA;
            blendDesc.RenderTarget[0].DestBlend      = D3D12_BLEND_INV_SRC_ALPHA;
            blendDesc.RenderTarget[0].BlendOp        = D3D12_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].SrcBlendAlpha  = D3D12_BLEND_ONE;
            blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
            blendDesc.RenderTarget[0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;
            break;

        case BlendMode::Additive:
            blendDesc.RenderTarget[0].BlendEnable = TRUE;
            blendDesc.RenderTarget[0].SrcBlend       = D3D12_BLEND_ONE;
            blendDesc.RenderTarget[0].DestBlend      = D3D12_BLEND_ONE;
            blendDesc.RenderTarget[0].BlendOp        = D3D12_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].SrcBlendAlpha  = D3D12_BLEND_ONE;
            blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
            blendDesc.RenderTarget[0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;
            break;

        case BlendMode::Multiply:
            blendDesc.RenderTarget[0].BlendEnable = TRUE;
            blendDesc.RenderTarget[0].SrcBlend       = D3D12_BLEND_DEST_COLOR;
            blendDesc.RenderTarget[0].DestBlend      = D3D12_BLEND_ZERO;
            blendDesc.RenderTarget[0].BlendOp        = D3D12_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].SrcBlendAlpha  = D3D12_BLEND_DEST_ALPHA;
            blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
            blendDesc.RenderTarget[0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;
            break;

        case BlendMode::Premultiplied:
            blendDesc.RenderTarget[0].BlendEnable = TRUE;
            blendDesc.RenderTarget[0].SrcBlend       = D3D12_BLEND_ONE;
            blendDesc.RenderTarget[0].DestBlend      = D3D12_BLEND_INV_SRC_ALPHA;
            blendDesc.RenderTarget[0].BlendOp        = D3D12_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].SrcBlendAlpha  = D3D12_BLEND_ONE;
            blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
            blendDesc.RenderTarget[0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;
            break;

        case BlendMode::Opaque:
        case BlendMode::Disabled:
        default:
            // 默认不混合
            break;
        }
    }

    void PSOManager::ConfigureDepthStencilState(
        D3D12_DEPTH_STENCIL_DESC& depthStencilDesc,
        DepthMode                 depthMode,
        const StencilTestDetail&  stencilDetail)
    {
        // [STEP 1] Configure depth test (existing logic)
        switch (depthMode)
        {
        case DepthMode::Enabled:
        case DepthMode::LessEqual:
            depthStencilDesc.DepthEnable = TRUE;
            depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            depthStencilDesc.DepthFunc      = D3D12_COMPARISON_FUNC_LESS_EQUAL;
            break;

        case DepthMode::Less:
            depthStencilDesc.DepthEnable = TRUE;
            depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            depthStencilDesc.DepthFunc      = D3D12_COMPARISON_FUNC_LESS;
            break;

        case DepthMode::ReadOnly:
            depthStencilDesc.DepthEnable = TRUE;
            depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
            depthStencilDesc.DepthFunc      = D3D12_COMPARISON_FUNC_LESS_EQUAL;
            break;

        case DepthMode::WriteOnly:
        case DepthMode::Always:
            depthStencilDesc.DepthEnable = TRUE;
            depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            depthStencilDesc.DepthFunc      = D3D12_COMPARISON_FUNC_ALWAYS;
            break;

        case DepthMode::GreaterEqual:
            depthStencilDesc.DepthEnable = TRUE;
            depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            depthStencilDesc.DepthFunc      = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
            break;

        case DepthMode::Greater:
            depthStencilDesc.DepthEnable = TRUE;
            depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            depthStencilDesc.DepthFunc      = D3D12_COMPARISON_FUNC_GREATER;
            break;

        case DepthMode::Equal:
            depthStencilDesc.DepthEnable = TRUE;
            depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            depthStencilDesc.DepthFunc      = D3D12_COMPARISON_FUNC_EQUAL;
            break;

        case DepthMode::Disabled:
        default:
            depthStencilDesc.DepthEnable = FALSE;
            depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
            depthStencilDesc.DepthFunc      = D3D12_COMPARISON_FUNC_ALWAYS;
            break;
        }

        // [STEP 2] Configure stencil test (new integration)
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
