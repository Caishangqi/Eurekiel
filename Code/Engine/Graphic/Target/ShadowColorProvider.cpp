#include "ShadowColorProvider.hpp"
#include "D12RenderTarget.hpp"
#include "../Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"

#include <sstream>

#include "Engine/Core/Logger/LoggerAPI.hpp"

namespace enigma::graphic
{
    // ============================================================================
    // Constructor
    // ============================================================================

    ShadowColorProvider::ShadowColorProvider(
        int                                    baseWidth,
        int                                    baseHeight,
        const std::vector<RenderTargetConfig>& configs,
        UniformManager*                        uniformMgr)
        : m_flipState()
    {
        // Validate parameters
        if (configs.empty())
        {
            throw std::invalid_argument("ShadowColorProvider:: Config vector cannot be empty");
        }

        // [RAII] Validate UniformManager - required for Shader RT Fetching
        if (!uniformMgr)
        {
            throw std::invalid_argument("ShadowColorProvider:: UniformManager cannot be null (RAII requirement)");
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

        if (m_baseWidth <= 0 || m_baseHeight <= 0)
        {
            throw std::invalid_argument("ShadowColorProvider:: Resolution must be > 0");
        }

        // Clamp config count to valid range
        int configCount = static_cast<int>(configs.size());
        if (configCount > MAX_SHADOW_COLORS)
        {
            LogWarn(LogRenderTargetProvider,
                    "ShadowColorProvider:: Config count %d exceeds max %d, clamping",
                    configCount, MAX_SHADOW_COLORS);
            configCount = MAX_SHADOW_COLORS;
        }

        m_activeCount = configCount;
        m_configs.reserve(m_activeCount);
        m_renderTargets.resize(m_activeCount);

        // Create render targets from configs
        for (int i = 0; i < m_activeCount; ++i)
        {
            const auto& config = configs[i];
            m_configs.push_back(config);

            // Use base override or config dimensions
            int rtWidth  = useBaseOverride ? m_baseWidth : (config.width > 0 ? config.width : m_baseWidth);
            int rtHeight = useBaseOverride ? m_baseHeight : (config.height > 0 ? config.height : m_baseHeight);

            // Build render target using Builder pattern
            auto builder = D12RenderTarget::Create()
                           .SetFormat(config.format)
                           .SetDimensions(rtWidth, rtHeight)
                           .SetLinearFilter(config.allowLinearFilter)
                           .SetSampleCount(config.sampleCount)
                           .EnableMipmap(config.enableMipmap)
                           .SetClearValue(config.clearValue);

            // Set debug name (shadowcolor0, shadowcolor1, ...)
            char debugName[32];
            sprintf_s(debugName, "shadowcolor%d", i);
            builder.SetName(debugName);

            // Build and initialize
            m_renderTargets[i] = builder.Build();
            m_renderTargets[i]->Upload();
            m_renderTargets[i]->RegisterBindless();
        }

        // [RAII] Register uniform buffer and perform initial upload
        ShadowColorProvider::RegisterUniform(uniformMgr);

        LogInfo(LogRenderTargetProvider,
                "ShadowColorProvider:: Initialized with %d/%d shadowcolor (%dx%d resolution)",
                m_activeCount, MAX_SHADOW_COLORS, m_baseWidth, m_baseHeight);
    }

    // ============================================================================
    // IRenderTargetProvider - Core Operations
    // ============================================================================

    void ShadowColorProvider::Copy(int srcIndex, int dstIndex)
    {
        ValidateIndex(srcIndex);
        ValidateIndex(dstIndex);

        if (srcIndex == dstIndex)
        {
            return; // No-op for same index
        }

        // TODO: Implement GPU copy operation
        throw CopyOperationFailedException("ShadowColorProvider", srcIndex, dstIndex);
    }

    void ShadowColorProvider::Clear(int index, const float* clearValue)
    {
        ValidateIndex(index);

        // TODO: Implement clear operation via command list
        (void)clearValue;
    }

    // ============================================================================
    // IRenderTargetProvider - RTV Access
    // ============================================================================

    D3D12_CPU_DESCRIPTOR_HANDLE ShadowColorProvider::GetMainRTV(int index)
    {
        ValidateIndex(index);
        return m_renderTargets[index]->GetMainRTV();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE ShadowColorProvider::GetAltRTV(int index)
    {
        ValidateIndex(index);
        return m_renderTargets[index]->GetAltRTV();
    }

    // ============================================================================
    // IRenderTargetProvider - Resource Access
    // ============================================================================

    ID3D12Resource* ShadowColorProvider::GetMainResource(int index)
    {
        ValidateIndex(index);
        auto mainTex = m_renderTargets[index]->GetMainTexture();
        return mainTex ? mainTex->GetResource() : nullptr;
    }

    ID3D12Resource* ShadowColorProvider::GetAltResource(int index)
    {
        ValidateIndex(index);
        auto altTex = m_renderTargets[index]->GetAltTexture();
        return altTex ? altTex->GetResource() : nullptr;
    }

    // ============================================================================
    // IRenderTargetProvider - Bindless Index Access
    // ============================================================================

    uint32_t ShadowColorProvider::GetMainTextureIndex(int index)
    {
        ValidateIndex(index);
        return m_renderTargets[index]->GetMainTextureIndex();
    }

    uint32_t ShadowColorProvider::GetAltTextureIndex(int index)
    {
        ValidateIndex(index);
        return m_renderTargets[index]->GetAltTextureIndex();
    }

    // ============================================================================
    // IRenderTargetProvider - FlipState Management
    // ============================================================================

    void ShadowColorProvider::Flip(int index)
    {
        ValidateIndex(index);
        m_flipState.Flip(index);
    }

    void ShadowColorProvider::FlipAll()
    {
        m_flipState.FlipAll();
    }

    void ShadowColorProvider::Reset()
    {
        m_flipState.Reset();
    }

    // ============================================================================
    // IRenderTargetProvider - Dynamic Configuration
    // ============================================================================

    void ShadowColorProvider::SetRtConfig(int index, const RenderTargetConfig& config)
    {
        ValidateIndex(index);

        const RenderTargetConfig& currentConfig = m_configs[index];

        // [REFACTOR] Only rebuild if format changes
        bool needRebuild = (currentConfig.format != config.format);

        // Update stored config
        m_configs[index] = config;

        if (needRebuild)
        {
            // Recreate render target with new config
            auto builder = D12RenderTarget::Create()
                           .SetFormat(config.format)
                           .SetDimensions(config.width, config.height)
                           .SetLinearFilter(config.allowLinearFilter)
                           .SetSampleCount(config.sampleCount)
                           .EnableMipmap(config.enableMipmap)
                           .SetClearValue(config.clearValue);

            char debugName[32];
            sprintf_s(debugName, "shadowcolor%d", index);
            builder.SetName(debugName);

            m_renderTargets[index] = builder.Build();
            m_renderTargets[index]->Upload();
            m_renderTargets[index]->RegisterBindless();

            LogInfo(LogRenderTargetProvider,
                    "ShadowColorProvider:: Rebuilt shadowcolor%d (format changed to %d)",
                    index, static_cast<int>(config.format));

            UpdateIndices();
        }
    }

    // ============================================================================
    // [NEW] Reset and Config Query Implementation
    // ============================================================================

    void ShadowColorProvider::ResetToDefault(const std::vector<RenderTargetConfig>& defaultConfigs)
    {
        int count = static_cast<int>(std::min(static_cast<size_t>(m_activeCount), defaultConfigs.size()));

        for (int i = 0; i < count; ++i)
        {
            SetRtConfig(i, defaultConfigs[i]);
        }

        LogInfo(LogRenderTargetProvider,
                "ShadowColorProvider:: ResetToDefault - restored %d shadowcolor to default config",
                count);
    }

    const RenderTargetConfig& ShadowColorProvider::GetConfig(int index) const
    {
        if (!IsValidIndex(index))
        {
            throw InvalidIndexException("ShadowColorProvider::GetConfig", index, m_activeCount);
        }
        return m_configs[index];
    }

    // ============================================================================
    // Extended API
    // ============================================================================

    DXGI_FORMAT ShadowColorProvider::GetFormat(int index) const
    {
        if (!IsValidIndex(index))
        {
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        }
        return m_renderTargets[index]->GetFormat();
    }

    std::shared_ptr<D12RenderTarget> ShadowColorProvider::GetRenderTarget(int index) const
    {
        if (!IsValidIndex(index))
        {
            throw InvalidIndexException("ShadowColorProvider", index, m_activeCount);
        }
        return m_renderTargets[index];
    }

    bool ShadowColorProvider::IsFlipped(int index) const
    {
        if (!IsValidIndex(index))
        {
            return false;
        }
        return m_flipState.IsFlipped(index);
    }

    void ShadowColorProvider::SetResolution(int newWidth, int newHeight)
    {
        if (newWidth <= 0 || newHeight <= 0)
        {
            throw std::invalid_argument("ShadowColorProvider:: Resolution must be > 0");
        }

        m_baseWidth  = newWidth;
        m_baseHeight = newHeight;

        // Recreate all render targets with new resolution
        for (int i = 0; i < m_activeCount; ++i)
        {
            const auto& config = m_configs[i];

            auto builder = D12RenderTarget::Create()
                           .SetFormat(config.format)
                           .SetDimensions(newWidth, newHeight)
                           .SetLinearFilter(config.allowLinearFilter)
                           .SetSampleCount(config.sampleCount)
                           .EnableMipmap(config.enableMipmap)
                           .SetClearValue(config.clearValue);

            char debugName[32];
            sprintf_s(debugName, "shadowcolor%d", i);
            builder.SetName(debugName);

            m_renderTargets[i] = builder.Build();
            m_renderTargets[i]->Upload();
            m_renderTargets[i]->RegisterBindless();
        }

        LogInfo(LogRenderTargetProvider,
                "ShadowColorProvider:: Resolution changed to %dx%d", newWidth, newHeight);
    }

    std::string ShadowColorProvider::GetDebugInfo(int index) const
    {
        if (!IsValidIndex(index))
        {
            std::ostringstream oss;
            oss << "ShadowColorProvider:: Invalid index " << index;
            oss << ", valid range [0, " << m_activeCount << ")";
            return oss.str();
        }

        const auto& rt      = m_renderTargets[index];
        const auto& config  = m_configs[index];
        bool        flipped = m_flipState.IsFlipped(index);

        std::ostringstream oss;
        oss << "=== shadowcolor" << index << " ===\n";
        oss << "Status: Active (" << (index + 1) << "/" << m_activeCount << ")\n";
        oss << "Resolution: " << config.width << "x" << config.height << "\n";
        oss << "FlipState: " << (flipped ? "Flipped" : "Normal") << "\n";
        oss << "MainIndex: " << rt->GetMainTextureIndex() << "\n";
        oss << "AltIndex: " << rt->GetAltTextureIndex() << "\n";
        oss << "Format: " << config.format << "\n";

        return oss.str();
    }

    std::string ShadowColorProvider::GetAllInfo() const
    {
        std::ostringstream oss;
        oss << "=== ShadowColorProvider Overview ===\n";
        oss << "Resolution: " << m_baseWidth << "x" << m_baseHeight << "\n";
        oss << "Active: " << m_activeCount << "/" << MAX_SHADOW_COLORS << "\n\n";

        oss << "Index | Name         | Resolution | Format | Flip | MainIdx | AltIdx\n";
        oss << "------|--------------|------------|--------|------|---------|-------\n";

        for (int i = 0; i < m_activeCount; ++i)
        {
            const auto& rt      = m_renderTargets[i];
            const auto& config  = m_configs[i];
            bool        flipped = m_flipState.IsFlipped(i);

            char line[256];
            sprintf_s(line, "%-5d | shadowcolor%-1d | %4dx%-4d | %-6d | %-4s | %-7u | %u\n",
                      i, i, config.width, config.height, config.format,
                      flipped ? "Yes" : "No",
                      rt->GetMainTextureIndex(),
                      rt->GetAltTextureIndex());
            oss << line;
        }

        return oss.str();
    }

    // ============================================================================
    // [NEW] Uniform Registration API - Shader RT Fetching Feature
    // ============================================================================

    void ShadowColorProvider::RegisterUniform(UniformManager* uniformMgr)
    {
        if (!uniformMgr)
        {
            LogError(LogRenderTargetProvider,
                     "ShadowColorProvider::RegisterUniform - UniformManager is nullptr");
            return;
        }

        m_uniformManager = uniformMgr;

        // Register ShadowColorIndexUniforms to slot b5 with PerFrame frequency
        m_uniformManager->RegisterBuffer<ShadowColorIndexUniforms>(
            SLOT_SHADOW_COLOR,
            UpdateFrequency::PerFrame,
            BufferSpace::Engine
        );

        LogInfo(LogRenderTargetProvider,
                "ShadowColorProvider::RegisterUniform - Registered at slot b%u",
                SLOT_SHADOW_COLOR);

        // Initial upload of indices
        UpdateIndices();
    }

    void ShadowColorProvider::UpdateIndices()
    {
        if (!m_uniformManager)
        {
            // Not registered yet, skip silently
            return;
        }

        // Collect bindless indices for all active shadowcolor slots
        for (int i = 0; i < m_activeCount; ++i)
        {
            bool isFlipped = m_flipState.IsFlipped(i);

            // Read index: Main if not flipped, Alt if flipped
            uint32_t readIdx = isFlipped
                                   ? m_renderTargets[i]->GetAltTextureIndex()
                                   : m_renderTargets[i]->GetMainTextureIndex();

            // Write index: Alt if not flipped, Main if flipped
            uint32_t writeIdx = isFlipped
                                    ? m_renderTargets[i]->GetMainTextureIndex()
                                    : m_renderTargets[i]->GetAltTextureIndex();

            m_indexBuffer.SetIndex(static_cast<uint32_t>(i), readIdx);
            m_indexBuffer.writeIndices[i] = writeIdx;
        }

        // Upload to GPU via UniformManager
        m_uniformManager->UploadBuffer(m_indexBuffer);
    }

    // ============================================================================
    // Private Methods
    // ============================================================================

    void ShadowColorProvider::ValidateIndex(int index) const
    {
        if (!IsValidIndex(index))
        {
            throw InvalidIndexException("ShadowColorProvider", index, m_activeCount);
        }
    }

    bool ShadowColorProvider::IsValidIndex(int index) const
    {
        return index >= 0 && index < m_activeCount;
    }
} // namespace enigma::graphic
