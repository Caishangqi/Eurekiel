// ============================================================================
// ShadowTextureProvider.cpp - [NEW] Shadow depth texture provider
// Implements IRenderTargetProvider for shadowtex0-1 management
// ============================================================================

#include "ShadowTextureProvider.hpp"
#include "../Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"

#include <sstream>
#include <stdexcept>

#include "RTTypes.hpp"
#include "ShadowColorProvider.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

namespace enigma::graphic
{
    // ============================================================================
    // Constructor
    // ============================================================================

    ShadowTextureProvider::ShadowTextureProvider(
        int                                    baseWidth,
        int                                    baseHeight,
        const std::vector<RenderTargetConfig>& configs,
        UniformManager*                        uniformMgr
    )
        : m_activeCount(static_cast<int>(configs.size()))
    {
        // Validate config count
        if (configs.empty() || configs.size() > MAX_SHADOW_TEXTURES)
        {
            throw std::out_of_range("ShadowTextureProvider: config count must be in [1-2]");
        }

        // [RAII] Validate UniformManager - required for Shader RT Fetching
        if (!uniformMgr)
        {
            throw std::invalid_argument("ShadowTextureProvider: UniformManager cannot be null (RAII requirement)");
        }

        // Determine actual dimensions:
        // If baseWidth/baseHeight > 0, use them as override; otherwise use config values
        bool useBaseOverride = (baseWidth > 0 && baseHeight > 0);

        if (useBaseOverride)
        {
            m_baseWidth  = baseWidth;
            m_baseHeight = baseHeight;
        }
        else
        {
            // Fallback to first config's dimensions
            m_baseWidth  = configs[0].width;
            m_baseHeight = configs[0].height;
        }

        // Validate resolution
        if (m_baseWidth <= 0 || m_baseHeight <= 0)
        {
            throw std::invalid_argument("ShadowTextureProvider: Resolution must be > 0");
        }

        // Reserve space
        m_depthTextures.reserve(configs.size());
        m_configs.reserve(configs.size());

        // Create shadow depth textures
        for (size_t i = 0; i < configs.size(); ++i)
        {
            const auto& config = configs[i];

            // Save config
            m_configs.push_back(config);

            // Use base override or config dimensions
            int texWidth  = useBaseOverride ? m_baseWidth : (config.width > 0 ? config.width : m_baseWidth);
            int texHeight = useBaseOverride ? m_baseHeight : (config.height > 0 ? config.height : m_baseHeight);

            // Create depth texture
            DepthTextureCreateInfo createInfo(
                config.name,
                static_cast<uint32_t>(texWidth),
                static_cast<uint32_t>(texHeight),
                config.format,
                1.0f, // clearDepth
                0 // clearStencil
            );

            m_depthTextures.push_back(std::make_shared<D12DepthTexture>(createInfo));
            m_depthTextures.back()->Upload();
            m_depthTextures.back()->RegisterBindless();
        }

        // [RAII] Register uniform buffer and perform initial upload
        ShadowTextureProvider::RegisterUniform(uniformMgr);

        LogInfo(LogRenderTargetProvider, "ShadowTextureProvider created: %d textures, %dx%d",
                m_activeCount, m_baseWidth, m_baseHeight);
    }

    // ============================================================================
    // IRenderTargetProvider - Core Operations
    // ============================================================================

    void ShadowTextureProvider::Copy(int srcIndex, int dstIndex)
    {
        ValidateIndex(srcIndex);
        ValidateIndex(dstIndex);

        if (srcIndex == dstIndex)
        {
            LogWarn(LogRenderTargetProvider, "ShadowTexture Copy: src and dst are same index %d", srcIndex);
            return;
        }

        ID3D12GraphicsCommandList* cmdList = D3D12RenderSystem::GetCurrentCommandList();
        if (!cmdList)
        {
            throw CopyOperationFailedException("ShadowTextureProvider", srcIndex, dstIndex);
        }

        CopyDepth(cmdList, srcIndex, dstIndex);
    }

    void ShadowTextureProvider::Clear(int index, const float* clearValue)
    {
        ValidateIndex(index);

        auto depthTex = m_depthTextures[index];
        if (!depthTex)
        {
            throw ResourceNotReadyException("shadowtex" + std::to_string(index) + " is null");
        }

        ID3D12GraphicsCommandList* cmdList = D3D12RenderSystem::GetCurrentCommandList();
        if (!cmdList)
        {
            LogWarn(LogRenderTargetProvider, "ShadowTexture Clear: No active command list");
            return;
        }

        float depthValue   = clearValue ? clearValue[0] : 1.0f;
        UINT8 stencilValue = 0;

        D3D12_CPU_DESCRIPTOR_HANDLE dsv = depthTex->GetDSVHandle();
        cmdList->ClearDepthStencilView(
            dsv,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            depthValue,
            stencilValue,
            0,
            nullptr
        );
    }

    // ============================================================================
    // IRenderTargetProvider - RTV Access (NOT SUPPORTED)
    // ============================================================================

    D3D12_CPU_DESCRIPTOR_HANDLE ShadowTextureProvider::GetMainRTV(int index)
    {
        ValidateIndex(index);

        // [FIX] Shadow depth textures use DSV, return DSV handle as "main" for compatibility
        // Same pattern as DepthTextureProvider::GetMainRTV
        return m_depthTextures[index]->GetDSVHandle();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE ShadowTextureProvider::GetAltRTV(int index)
    {
        UNUSED(index)
        throw std::logic_error("ShadowTextureProvider: GetAltRTV not supported (depth textures have no RTV)");
    }

    // ============================================================================
    // IRenderTargetProvider - Resource Access
    // ============================================================================

    ID3D12Resource* ShadowTextureProvider::GetMainResource(int index)
    {
        ValidateIndex(index);
        auto depthTex = m_depthTextures[index];
        return depthTex ? depthTex->GetDepthTextureResource() : nullptr;
    }

    ID3D12Resource* ShadowTextureProvider::GetAltResource(int index)
    {
        // Shadow depth textures do not support flip-state (single-buffered)
        UNUSED(index);
        return nullptr;
    }

    // ============================================================================
    // IRenderTargetProvider - Bindless Index Access
    // ============================================================================

    uint32_t ShadowTextureProvider::GetMainTextureIndex(int index)
    {
        ValidateIndex(index);

        auto depthTex = m_depthTextures[index];
        if (!depthTex)
        {
            throw ResourceNotReadyException("shadowtex" + std::to_string(index) + " is null");
        }

        return depthTex->GetBindlessIndex();
    }

    uint32_t ShadowTextureProvider::GetAltTextureIndex(int index)
    {
        UNUSED(index)
        throw std::logic_error("ShadowTextureProvider: GetAltTextureIndex not supported (no flip-state)");
    }

    // ============================================================================
    // IRenderTargetProvider - FlipState Management (no-op)
    // ============================================================================

    void ShadowTextureProvider::Flip(int index)
    {
        UNUSED(index)
        // No-op: shadow depth textures do not support flip-state
    }

    void ShadowTextureProvider::FlipAll()
    {
        // No-op: shadow depth textures do not support flip-state
    }

    void ShadowTextureProvider::Reset()
    {
        // No-op: shadow depth textures do not support flip-state
    }

    // ============================================================================
    // IRenderTargetProvider - Dynamic Configuration
    // ============================================================================

    void ShadowTextureProvider::SetRtConfig(int index, const RenderTargetConfig& config)
    {
        ValidateIndex(index);

        const RenderTargetConfig& currentConfig = m_configs[index];

        // [REFACTOR] Only rebuild if format or dimensions change
        bool needRebuild = (currentConfig.format != config.format) ||
            (currentConfig.width != config.width) ||
            (currentConfig.height != config.height);

        // Update config
        m_configs[index] = config;

        if (needRebuild)
        {
            // Recreate depth texture
            DepthTextureCreateInfo createInfo(
                config.name,
                static_cast<uint32_t>(config.width),
                static_cast<uint32_t>(config.height),
                config.format,
                1.0f,
                0
            );

            m_depthTextures[index] = std::make_shared<D12DepthTexture>(createInfo);
            m_depthTextures[index]->Upload();
            m_depthTextures[index]->RegisterBindless();

            LogInfo(LogRenderTargetProvider, "shadowtex%d rebuilt (%dx%d, format=%d)",
                    index, config.width, config.height, static_cast<int>(config.format));

            UpdateIndices();
        }
    }

    // ============================================================================
    // [NEW] Reset and Config Query Implementation
    // ============================================================================

    void ShadowTextureProvider::ResetToDefault(const std::vector<RenderTargetConfig>& defaultConfigs)
    {
        int count = static_cast<int>(std::min(static_cast<size_t>(m_activeCount), defaultConfigs.size()));

        for (int i = 0; i < count; ++i)
        {
            SetRtConfig(i, defaultConfigs[i]);
        }

        LogInfo(LogRenderTargetProvider,
                "ShadowTextureProvider:: ResetToDefault - restored %d shadowtex to default config",
                count);
    }

    const RenderTargetConfig& ShadowTextureProvider::GetConfig(int index) const
    {
        if (!IsValidIndex(index))
        {
            throw InvalidIndexException("ShadowTextureProvider::GetConfig", index, m_activeCount);
        }
        return m_configs[index];
    }

    // ============================================================================
    // Extended API - DSV Access
    // ============================================================================

    D3D12_CPU_DESCRIPTOR_HANDLE ShadowTextureProvider::GetDSV(int index) const
    {
        if (!IsValidIndex(index))
        {
            throw InvalidIndexException("ShadowTextureProvider", index, m_activeCount);
        }

        auto depthTex = m_depthTextures[index];
        if (!depthTex)
        {
            return {}; // Return null handle
        }

        return depthTex->GetDSVHandle();
    }

    std::shared_ptr<D12DepthTexture> ShadowTextureProvider::GetDepthTexture(int index) const
    {
        if (!IsValidIndex(index))
        {
            throw InvalidIndexException("ShadowTextureProvider", index, m_activeCount);
        }

        return m_depthTextures[index];
    }

    // ============================================================================
    // Extended API - Iris-compatible Copy Method
    // ============================================================================

    void ShadowTextureProvider::CopyPreTranslucentDepth(ID3D12GraphicsCommandList* cmdList)
    {
        if (m_activeCount < 2)
        {
            LogWarn(LogRenderTargetProvider, "CopyPreTranslucentDepth: shadowtex1 not available");
            return;
        }

        CopyDepth(cmdList, 0, 1);
    }

    // ============================================================================
    // Extended API - Resolution and Format
    // ============================================================================

    DXGI_FORMAT ShadowTextureProvider::GetFormat(int index) const
    {
        if (!IsValidIndex(index))
        {
            throw InvalidIndexException("ShadowTextureProvider", index, m_activeCount);
        }

        return m_configs[index].format;
    }

    void ShadowTextureProvider::SetResolution(int newWidth, int newHeight)
    {
        if (newWidth <= 0 || newHeight <= 0)
        {
            throw std::invalid_argument("ShadowTextureProvider: Invalid resolution");
        }

        m_baseWidth  = newWidth;
        m_baseHeight = newHeight;

        // Recreate all shadow depth textures with new resolution
        for (size_t i = 0; i < m_depthTextures.size(); ++i)
        {
            m_configs[i].width  = newWidth;
            m_configs[i].height = newHeight;

            DepthTextureCreateInfo createInfo(
                m_configs[i].name,
                static_cast<uint32_t>(newWidth),
                static_cast<uint32_t>(newHeight),
                m_configs[i].format,
                1.0f,
                0
            );

            m_depthTextures[i] = std::make_shared<D12DepthTexture>(createInfo);
        }

        LogInfo(LogRenderTargetProvider, "ShadowTextureProvider resolution changed to %dx%d",
                newWidth, newHeight);
    }

    // ============================================================================
    // Extended API - Debug
    // ============================================================================

    std::string ShadowTextureProvider::GetDebugInfo(int index) const
    {
        if (!IsValidIndex(index))
        {
            return "Invalid index";
        }

        std::ostringstream oss;
        const auto&        cfg = m_configs[index];

        oss << "shadowtex" << index << ": "
            << cfg.name << " (" << cfg.width << "x" << cfg.height << ")";

        if (m_depthTextures[index])
        {
            oss << ", Bindless: " << m_depthTextures[index]->GetBindlessIndex();
        }

        return oss.str();
    }

    std::string ShadowTextureProvider::GetAllInfo() const
    {
        std::ostringstream oss;

        oss << "ShadowTextureProvider (" << m_baseWidth << "x" << m_baseHeight << "):\n";
        oss << "  Active: " << m_activeCount << "/" << MAX_SHADOW_TEXTURES << "\n";

        for (int i = 0; i < m_activeCount; ++i)
        {
            oss << "  [" << i << "] " << GetDebugInfo(i);
            if (i == 0) oss << " - Main shadow depth";
            else if (i == 1) oss << " - Pre-translucent shadow";
            oss << "\n";
        }

        return oss.str();
    }

    // ============================================================================
    // [NEW] Uniform Registration API - Shader RT Fetching Feature
    // ============================================================================

    void ShadowTextureProvider::RegisterUniform(UniformManager* uniformMgr)
    {
        if (!uniformMgr)
        {
            LogError(LogRenderTargetProvider,
                     "ShadowTextureProvider::RegisterUniform - UniformManager is nullptr");
            return;
        }

        m_uniformManager = uniformMgr;

        // Register ShadowTexturesIndexUniforms to slot b6 with PerFrame frequency
        m_uniformManager->RegisterBuffer<ShadowTexturesIndexUniforms>(
            SLOT_SHADOW_TEXTURES,
            UpdateFrequency::PerFrame,
            BufferSpace::Engine
        );

        LogInfo(LogRenderTargetProvider,
                "ShadowTextureProvider::RegisterUniform - Registered at slot b%u",
                SLOT_SHADOW_TEXTURES);

        // Initial upload of indices
        UpdateIndices();
    }

    void ShadowTextureProvider::UpdateIndices()
    {
        if (!m_uniformManager)
        {
            // Not registered yet, skip silently
            return;
        }

        // Collect bindless indices for shadowtex0 and shadowtex1
        // Note: Shadow textures have no flip mechanism - only read indices
        for (int i = 0; i < m_activeCount; ++i)
        {
            uint32_t bindlessIdx = GetMainTextureIndex(i);
            m_indexBuffer.SetIndex(static_cast<uint32_t>(i), bindlessIdx);
        }

        // Upload to GPU via UniformManager
        m_uniformManager->UploadBuffer(m_indexBuffer);

        LogDebug(LogRenderTargetProvider,
                 "ShadowTextureProvider::UpdateIndices - Uploaded %d shadowtex indices",
                 m_activeCount);
    }

    // ============================================================================
    // Private Methods
    // ============================================================================

    void ShadowTextureProvider::ValidateIndex(int index) const
    {
        if (!IsValidIndex(index))
        {
            throw InvalidIndexException("ShadowTextureProvider", index, m_activeCount);
        }
    }

    bool ShadowTextureProvider::IsValidIndex(int index) const
    {
        return index >= 0 && index < m_activeCount;
    }

    void ShadowTextureProvider::CopyDepth(
        ID3D12GraphicsCommandList* cmdList,
        int                        srcIndex,
        int                        dstIndex
    )
    {
        if (!cmdList)
        {
            throw std::invalid_argument("CopyDepthInternal: cmdList is null");
        }

        ValidateIndex(srcIndex);
        ValidateIndex(dstIndex);

        if (srcIndex == dstIndex)
        {
            throw std::invalid_argument("CopyDepthInternal: src and dst cannot be same");
        }

        auto srcTex = m_depthTextures[srcIndex];
        auto dstTex = m_depthTextures[dstIndex];

        if (!srcTex || !dstTex)
        {
            throw CopyOperationFailedException("ShadowTextureProvider", srcIndex, dstIndex);
        }

        auto srcResource = srcTex->GetDepthTextureResource();
        auto dstResource = dstTex->GetDepthTextureResource();

        if (!srcResource || !dstResource)
        {
            throw CopyOperationFailedException("ShadowTextureProvider", srcIndex, dstIndex);
        }

        // Transition: DEPTH_WRITE -> COPY_SOURCE/DEST
        D3D12_RESOURCE_BARRIER barriers[2];

        barriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barriers[0].Transition.pResource   = srcResource;
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        barriers[1].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barriers[1].Transition.pResource   = dstResource;
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        D3D12RenderSystem::TransitionResources(cmdList, barriers, 2, "ShadowTextureProvider::Copy::Pre");

        // Copy
        cmdList->CopyResource(dstResource, srcResource);

        // Transition back: COPY_SOURCE/DEST -> DEPTH_WRITE
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_DEPTH_WRITE;

        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_DEPTH_WRITE;

        D3D12RenderSystem::TransitionResources(cmdList, barriers, 2, "ShadowTextureProvider::Copy::Post");
    }
} // namespace enigma::graphic
