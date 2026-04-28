#include "DepthTextureProvider.hpp"
#include "../Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"

#include <sstream>
#include <stdexcept>

#include "RTTypes.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

namespace enigma::graphic
{
    namespace
    {
        int CalculateScaledDimension(int baseDimension, float scale)
        {
            const int dimension = static_cast<int>(static_cast<float>(baseDimension) * scale);
            return (dimension > 0) ? dimension : 1;
        }

        RenderTargetConfig ResolveDepthTargetConfig(
            RenderTargetConfig config,
            int                baseWidth,
            int                baseHeight)
        {
            config.width  = CalculateScaledDimension(baseWidth, config.widthScale);
            config.height = CalculateScaledDimension(baseHeight, config.heightScale);
            return config;
        }

        bool HasDifferentDepthClearValue(const ClearValue& lhs, const ClearValue& rhs)
        {
            return lhs.depthStencil.depth != rhs.depthStencil.depth ||
                   lhs.depthStencil.stencil != rhs.depthStencil.stencil;
        }

        bool RequiresDepthTextureRebuild(
            const RenderTargetConfig&             currentConfig,
            const RenderTargetConfig&             nextConfig,
            const std::shared_ptr<D12DepthTexture>& depthTexture)
        {
            if (!depthTexture)
            {
                return true;
            }

            return currentConfig.format != nextConfig.format ||
                   currentConfig.width != nextConfig.width ||
                   currentConfig.height != nextConfig.height ||
                   currentConfig.sampleCount != nextConfig.sampleCount ||
                   HasDifferentDepthClearValue(currentConfig.clearValue, nextConfig.clearValue) ||
                   depthTexture->GetWidth() != static_cast<uint32_t>(nextConfig.width) ||
                   depthTexture->GetHeight() != static_cast<uint32_t>(nextConfig.height) ||
                   depthTexture->GetDepthFormat() != nextConfig.format;
        }

        std::shared_ptr<D12DepthTexture> CreateDepthTexture(const RenderTargetConfig& config)
        {
            DepthTextureCreateInfo createInfo(
                config.name,
                static_cast<uint32_t>(config.width),
                static_cast<uint32_t>(config.height),
                config.format,
                config.clearValue.depthStencil.depth,
                config.clearValue.depthStencil.stencil
            );

            auto depthTexture = std::make_shared<D12DepthTexture>(createInfo);
            depthTexture->Upload();
            depthTexture->RegisterBindless();
            return depthTexture;
        }

        void UnregisterDepthTexture(const std::shared_ptr<D12DepthTexture>& depthTexture)
        {
            if (depthTexture && depthTexture->IsBindlessRegistered())
            {
                depthTexture->UnregisterBindless();
            }
        }
    }

    // ============================================================================
    // Constructor
    // ============================================================================

    DepthTextureProvider::DepthTextureProvider(
        int                                    baseWidth,
        int                                    baseHeight,
        const std::vector<RenderTargetConfig>& configs,
        UniformManager*                        uniformMgr
    )
        : m_baseWidth(baseWidth)
          , m_baseHeight(baseHeight)
          , m_activeCount(static_cast<int>(configs.size()))
    {
        // Validate dimensions
        if (baseWidth <= 0 || baseHeight <= 0)
        {
            throw std::invalid_argument("DepthTextureProvider: Invalid dimensions");
        }

        // Validate config count
        if (configs.empty() || configs.size() > MAX_DEPTH_TEXTURES)
        {
            throw std::out_of_range("DepthTextureProvider: config count must be in [1-3]");
        }

        // [RAII] Validate UniformManager - required for Shader RT Fetching
        if (!uniformMgr)
        {
            throw std::invalid_argument("DepthTextureProvider: UniformManager cannot be null (RAII requirement)");
        }

        // Reserve space
        m_depthTextures.reserve(configs.size());
        m_configs.reserve(configs.size());

        // Create depth textures
        for (size_t i = 0; i < configs.size(); ++i)
        {
            const auto config = ResolveDepthTargetConfig(configs[i], baseWidth, baseHeight);

            // Validate config (RenderTargetConfig uses width/height > 0 and non-empty name)
            if (config.width <= 0 || config.height <= 0 || config.name.empty())
            {
                throw std::invalid_argument(
                    "DepthTextureProvider: Invalid config at index " + std::to_string(i)
                );
            }

            // Save config
            m_configs.push_back(config);

            m_depthTextures.push_back(CreateDepthTexture(config));
        }

        // [RAII] Register uniform buffer and perform initial upload
        DepthTextureProvider::RegisterUniform(uniformMgr);

        LogInfo(LogRenderTargetProvider, "DepthTextureProvider created with %d textures", m_activeCount);
    }

    // ============================================================================
    // IRenderTargetProvider - Core Operations
    // ============================================================================

    void DepthTextureProvider::Copy(int srcIndex, int dstIndex)
    {
        ValidateIndex(srcIndex);
        ValidateIndex(dstIndex);

        if (srcIndex == dstIndex)
        {
            LogWarn(LogRenderTargetProvider, "Copy: src and dst are same index %d", srcIndex);
            return;
        }

        // Get current command list
        ID3D12GraphicsCommandList* cmdList = D3D12RenderSystem::GetCurrentCommandList();
        if (!cmdList)
        {
            throw CopyOperationFailedException("DepthTextureProvider", srcIndex, dstIndex);
        }

        CopyDepth(cmdList, srcIndex, dstIndex);
    }

    void DepthTextureProvider::Clear(int index, const float* clearValue)
    {
        ValidateIndex(index);

        auto depthTex = m_depthTextures[index];
        if (!depthTex)
        {
            throw ResourceNotReadyException("depthtex" + std::to_string(index) + " is null");
        }

        // Get command list
        ID3D12GraphicsCommandList* cmdList = D3D12RenderSystem::GetCurrentCommandList();

        // Clear depth value (first float is depth, second is stencil if provided)
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
    // IRenderTargetProvider - RTV/DSV Access
    // ============================================================================

    D3D12_CPU_DESCRIPTOR_HANDLE DepthTextureProvider::GetMainRTV(int index)
    {
        ValidateIndex(index);

        // Depth textures use DSV, return DSV handle as "main" for compatibility
        return m_depthTextures[index]->GetDSVHandle();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DepthTextureProvider::GetAltRTV(int index)
    {
        UNUSED(index)
        // Depth textures do not support flip-state
        throw std::logic_error("DepthTextureProvider: GetAltRTV not supported (no flip-state)");
    }

    // ============================================================================
    // IRenderTargetProvider - Resource Access
    // ============================================================================

    ID3D12Resource* DepthTextureProvider::GetMainResource(int index)
    {
        ValidateIndex(index);
        auto depthTex = m_depthTextures[index];
        return depthTex ? depthTex->GetDepthTextureResource() : nullptr;
    }

    ID3D12Resource* DepthTextureProvider::GetAltResource(int index)
    {
        // Depth textures do not support flip-state (single-buffered)
        UNUSED(index);
        return nullptr;
    }

    // ============================================================================
    // IRenderTargetProvider - Bindless Index Access
    // ============================================================================

    uint32_t DepthTextureProvider::GetMainTextureIndex(int index)
    {
        ValidateIndex(index);

        auto depthTex = m_depthTextures[index];
        if (!depthTex)
        {
            throw ResourceNotReadyException("depthtex" + std::to_string(index) + " is null");
        }

        return depthTex->GetBindlessIndex();
    }

    uint32_t DepthTextureProvider::GetAltTextureIndex(int index)
    {
        // Depth textures do not support flip-state
        UNUSED(index)
        throw std::logic_error("DepthTextureProvider: GetAltTextureIndex not supported (no flip-state)");
    }

    // ============================================================================
    // IRenderTargetProvider - FlipState Management (no-op)
    // ============================================================================

    void DepthTextureProvider::Flip(int index)
    {
        // No-op: depth textures do not support flip-state
        UNUSED(index);
    }

    void DepthTextureProvider::FlipAll()
    {
        // No-op: depth textures do not support flip-state
    }

    void DepthTextureProvider::Reset()
    {
        // No-op: depth textures do not support flip-state
    }

    // ============================================================================
    // IRenderTargetProvider - Dynamic Configuration
    // ============================================================================

    void DepthTextureProvider::SetRtConfig(int index, const RenderTargetConfig& config)
    {
        ValidateIndex(index);

        // depthtex0 cannot be modified (always equals render resolution)
        if (index == 0)
        {
            throw std::invalid_argument("DepthTextureProvider: Cannot modify depthtex0 resolution");
        }

        const RenderTargetConfig& currentConfig = m_configs[index];
        const RenderTargetConfig nextConfig = ResolveDepthTargetConfig(config, m_baseWidth, m_baseHeight);

        const bool needRebuild = RequiresDepthTextureRebuild(
            currentConfig,
            nextConfig,
            m_depthTextures[index]);

        // Update config
        m_configs[index] = nextConfig;

        if (needRebuild)
        {
            UnregisterDepthTexture(m_depthTextures[index]);
            m_depthTextures[index] = CreateDepthTexture(nextConfig);

            LogInfo(LogRenderTargetProvider, "depthtex%d rebuilt (%dx%d, format=%d)",
                    index, nextConfig.width, nextConfig.height, static_cast<int>(nextConfig.format));

            UpdateIndices();
        }
    }

    // ============================================================================
    // Reset and Config Query Implementation
    // ============================================================================

    void DepthTextureProvider::ResetToDefault(const std::vector<RenderTargetConfig>& defaultConfigs)
    {
        // Skip depthtex0 (cannot be modified)
        int count = static_cast<int>(std::min(static_cast<size_t>(m_activeCount), defaultConfigs.size()));

        for (int i = 1; i < count; ++i)
        {
            SetRtConfig(i, defaultConfigs[i]);
        }

        LogInfo(LogRenderTargetProvider,
                "DepthTextureProvider:: ResetToDefault - restored %d depthtex to default config",
                count - 1);
    }

    const RenderTargetConfig& DepthTextureProvider::GetConfig(int index) const
    {
        if (!IsValidIndex(index))
        {
            throw InvalidIndexException("DepthTextureProvider::GetConfig", index, m_activeCount);
        }
        return m_configs[index];
    }

    // ============================================================================
    // Uniform Registration API - Shader RT Fetching Feature
    // ============================================================================

    void DepthTextureProvider::RegisterUniform(UniformManager* uniformMgr)
    {
        if (!uniformMgr)
        {
            LogError(LogRenderTargetProvider,
                     "DepthTextureProvider::RegisterUniform - UniformManager is nullptr");
            return;
        }

        m_uniformManager = uniformMgr;

        // Register buffer at slot b4 (SLOT_DEPTH_TEXTURES)
        m_uniformManager->RegisterBuffer<DepthTextureIndexUniforms>(
            SLOT_DEPTH_TEXTURES,
            UpdateFrequency::PerFrame,
            BufferSpace::Engine
        );

        LogInfo(LogRenderTargetProvider,
                "DepthTextureProvider::RegisterUniform - Registered at slot b%u",
                SLOT_DEPTH_TEXTURES);

        // Initial upload
        UpdateIndices();
    }

    void DepthTextureProvider::UpdateIndices()
    {
        if (!m_uniformManager)
        {
            return;
        }

        // Collect bindless indices for all active depth textures
        // Note: Depth textures have no flip mechanism - only read indices
        for (int i = 0; i < m_activeCount; ++i)
        {
            uint32_t bindlessIdx = GetMainTextureIndex(i);
            m_indexBuffer.SetIndex(static_cast<uint32_t>(i), bindlessIdx);
        }

        // Upload to GPU
        m_uniformManager->UploadBuffer(m_indexBuffer);
    }

    // ============================================================================
    // Extended API - DSV Access
    // ============================================================================

    DXGI_FORMAT DepthTextureProvider::GetFormat(int index) const
    {
        if (!IsValidIndex(index))
        {
            throw InvalidIndexException("DepthTextureProvider", index, m_activeCount);
        }

        return m_configs[index].format;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DepthTextureProvider::GetDSV(int index) const
    {
        if (!IsValidIndex(index))
        {
            throw InvalidIndexException("DepthTextureProvider", index, m_activeCount);
        }

        auto depthTex = m_depthTextures[index];
        if (!depthTex)
        {
            return {}; // Return null handle
        }

        return depthTex->GetDSVHandle();
    }

    std::shared_ptr<D12DepthTexture> DepthTextureProvider::GetDepthTexture(int index) const
    {
        if (!IsValidIndex(index))
        {
            throw InvalidIndexException("DepthTextureProvider", index, m_activeCount);
        }

        return m_depthTextures[index];
    }

    // ============================================================================
    // Extended API - Resize
    // ============================================================================

    void DepthTextureProvider::OnResize(int newWidth, int newHeight)
    {
        if (newWidth <= 0 || newHeight <= 0)
        {
            throw std::invalid_argument("DepthTextureProvider: Invalid resize dimensions");
        }

        for (size_t i = 0; i < m_depthTextures.size(); ++i)
        {
            if (!m_depthTextures[i]) continue;

            int targetWidth = newWidth;
            int targetHeight = newHeight;

            if (i > 0 && i < m_configs.size())
            {
                m_configs[i] = ResolveDepthTargetConfig(m_configs[i], newWidth, newHeight);
                targetWidth = m_configs[i].width;
                targetHeight = m_configs[i].height;
            }
            else if (i < m_configs.size())
            {
                m_configs[i].width = targetWidth;
                m_configs[i].height = targetHeight;
            }

            bool success = m_depthTextures[i]->Resize(
                static_cast<uint32_t>(targetWidth),
                static_cast<uint32_t>(targetHeight)
            );

            if (!success)
            {
                throw std::runtime_error("Failed to resize depthtex" + std::to_string(i));
            }
        }

        m_baseWidth  = newWidth;
        m_baseHeight = newHeight;

        LogInfo(LogRenderTargetProvider, "DepthTextureProvider resized to %dx%d", newWidth, newHeight);
    }

    // ============================================================================
    // Extended API - Debug
    // ============================================================================

    std::string DepthTextureProvider::GetDebugInfo() const
    {
        std::ostringstream oss;

        oss << "DepthTextureProvider (" << m_baseWidth << "x" << m_baseHeight << "):\n";
        oss << "  Active: " << m_activeCount << "/" << MAX_DEPTH_TEXTURES << "\n";

        for (size_t i = 0; i < m_depthTextures.size(); ++i)
        {
            if (i < m_configs.size())
            {
                const auto& cfg = m_configs[i];
                oss << "  [" << i << "] " << cfg.name
                    << " (" << cfg.width << "x" << cfg.height << ")";

                if (i == 0) oss << " - Main depth";
                else if (i == 1) oss << " - Pre-translucent";
                else if (i == 2) oss << " - Pre-hand";

                if (m_depthTextures[i])
                {
                    oss << ", Bindless: " << m_depthTextures[i]->GetBindlessIndex();
                }
                oss << "\n";
            }
        }

        return oss.str();
    }

    // ============================================================================
    // Private Methods
    // ============================================================================

    void DepthTextureProvider::ValidateIndex(int index) const
    {
        if (!IsValidIndex(index))
        {
            throw InvalidIndexException("DepthTextureProvider", index, m_activeCount);
        }
    }

    bool DepthTextureProvider::IsValidIndex(int index) const
    {
        return index >= 0 && index < m_activeCount;
    }


    // ============================================================================
    // Private Methods - Copy
    // ============================================================================

    void DepthTextureProvider::CopyDepth(
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
            throw CopyOperationFailedException("DepthTextureProvider", srcIndex, dstIndex);
        }

        auto srcResource = srcTex->GetDepthTextureResource();
        auto dstResource = dstTex->GetDepthTextureResource();

        if (!srcResource || !dstResource)
        {
            throw CopyOperationFailedException("DepthTextureProvider", srcIndex, dstIndex);
        }

        D3D12_RESOURCE_STATES srcPrevState = srcTex->GetCurrentState();
        D3D12_RESOURCE_STATES dstPrevState = dstTex->GetCurrentState();

        srcTex->TransitionResourceTo(D3D12_RESOURCE_STATE_COPY_SOURCE);
        dstTex->TransitionResourceTo(D3D12_RESOURCE_STATE_COPY_DEST);

        cmdList->CopyResource(dstResource, srcResource);

        srcTex->TransitionResourceTo(srcPrevState);
        dstTex->TransitionResourceTo(dstPrevState);
    }
} // namespace enigma::graphic
