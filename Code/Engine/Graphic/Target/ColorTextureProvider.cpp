#include "ColorTextureProvider.hpp"
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

    ColorTextureProvider::ColorTextureProvider(
        int                                    baseWidth,
        int                                    baseHeight,
        const std::vector<RenderTargetConfig>& configs,
        UniformManager*                        uniformMgr)
        : m_baseWidth(baseWidth)
          , m_baseHeight(baseHeight)
          , m_flipState()
    {
        // Validate parameters
        if (baseWidth <= 0 || baseHeight <= 0)
        {
            throw std::invalid_argument("ColorTextureProvider:: Base dimensions must be > 0");
        }

        if (configs.empty())
        {
            throw std::invalid_argument("ColorTextureProvider:: Config vector cannot be empty");
        }

        if (!uniformMgr)
        {
            throw std::invalid_argument("ColorTextureProvider:: UniformManager cannot be null (RAII requirement)");
        }

        // Clamp config count to valid range
        int configCount = static_cast<int>(configs.size());
        if (configCount > MAX_COLOR_TEXTURES)
        {
            LogWarn(LogRenderTargetProvider,
                    "ColorTextureProvider:: Config count %d exceeds max %d, clamping",
                    configCount, MAX_COLOR_TEXTURES);
            configCount = MAX_COLOR_TEXTURES;
        }

        m_activeCount = configCount;
        m_configs.reserve(m_activeCount);
        m_renderTargets.resize(m_activeCount);

        // Create render targets from configs
        for (int i = 0; i < m_activeCount; ++i)
        {
            const auto& config = configs[i];
            m_configs.push_back(config);

            // Calculate actual dimensions with scale
            int rtWidth  = static_cast<int>(baseWidth * config.widthScale);
            int rtHeight = static_cast<int>(baseHeight * config.heightScale);
            rtWidth      = (rtWidth > 0) ? rtWidth : 1;
            rtHeight     = (rtHeight > 0) ? rtHeight : 1;

            // Build render target using Builder pattern
            auto builder = D12RenderTarget::Create()
                           .SetFormat(config.format)
                           .SetDimensions(rtWidth, rtHeight)
                           .SetLinearFilter(config.allowLinearFilter)
                           .SetSampleCount(config.sampleCount)
                           .EnableMipmap(config.enableMipmap)
                           .SetClearValue(config.clearValue);

            // Set debug name (colortex0, colortex1, ...)
            char debugName[32];
            sprintf_s(debugName, "colortex%d", i);
            builder.SetName(debugName);

            // Build and initialize
            m_renderTargets[i] = builder.Build();
            m_renderTargets[i]->Upload();
            m_renderTargets[i]->RegisterBindless();
        }

        LogInfo(LogRenderTargetProvider,
                "ColorTextureProvider:: Initialized with %d/%d colortex (%dx%d base)",
                m_activeCount, MAX_COLOR_TEXTURES, baseWidth, baseHeight);

        // [RAII] Register uniform buffer and perform initial upload
        ColorTextureProvider::RegisterUniform(uniformMgr);
    }

    // ============================================================================
    // IRenderTargetProvider - Core Operations
    // ============================================================================

    void ColorTextureProvider::Copy(int srcIndex, int dstIndex)
    {
        ValidateIndex(srcIndex);
        ValidateIndex(dstIndex);

        if (srcIndex == dstIndex)
        {
            return; // No-op for same index
        }

        // TODO: Implement GPU copy operation
        // For now, throw not implemented
        throw CopyOperationFailedException("ColorTextureProvider", srcIndex, dstIndex);
    }

    void ColorTextureProvider::Clear(int index, const float* clearValue)
    {
        ValidateIndex(index);

        // TODO: Implement clear operation via command list
        // This requires access to current command list context
        (void)clearValue;
    }

    // ============================================================================
    // IRenderTargetProvider - RTV Access
    // ============================================================================

    D3D12_CPU_DESCRIPTOR_HANDLE ColorTextureProvider::GetMainRTV(int index)
    {
        ValidateIndex(index);
        return m_renderTargets[index]->GetMainRTV();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE ColorTextureProvider::GetAltRTV(int index)
    {
        ValidateIndex(index);
        return m_renderTargets[index]->GetAltRTV();
    }

    // ============================================================================
    // IRenderTargetProvider - Resource Access
    // ============================================================================

    ID3D12Resource* ColorTextureProvider::GetMainResource(int index)
    {
        ValidateIndex(index);
        auto mainTex = m_renderTargets[index]->GetMainTexture();
        return mainTex ? mainTex->GetResource() : nullptr;
    }

    ID3D12Resource* ColorTextureProvider::GetAltResource(int index)
    {
        ValidateIndex(index);
        auto altTex = m_renderTargets[index]->GetAltTexture();
        return altTex ? altTex->GetResource() : nullptr;
    }

    // ============================================================================
    // IRenderTargetProvider - Bindless Index Access
    // ============================================================================

    uint32_t ColorTextureProvider::GetMainTextureIndex(int index)
    {
        ValidateIndex(index);
        return m_renderTargets[index]->GetMainTextureIndex();
    }

    uint32_t ColorTextureProvider::GetAltTextureIndex(int index)
    {
        ValidateIndex(index);
        return m_renderTargets[index]->GetAltTextureIndex();
    }

    // ============================================================================
    // IRenderTargetProvider - FlipState Management
    // ============================================================================

    void ColorTextureProvider::Flip(int index)
    {
        ValidateIndex(index);
        m_flipState.Flip(index);

        // [NEW] Re-upload indices after flip state change
        UpdateIndices();
    }

    void ColorTextureProvider::FlipAll()
    {
        m_flipState.FlipAll();

        // [NEW] Re-upload indices after flip state change
        UpdateIndices();
    }

    void ColorTextureProvider::Reset()
    {
        m_flipState.Reset();
    }

    // ============================================================================
    // IRenderTargetProvider - Dynamic Configuration
    // ============================================================================

    void ColorTextureProvider::SetRtConfig(int index, const RenderTargetConfig& config)
    {
        ValidateIndex(index);

        const RenderTargetConfig& currentConfig = m_configs[index];

        // [REFACTOR] Only rebuild if format changes
        bool needRebuild = (currentConfig.format != config.format);

        // Update stored config
        m_configs[index] = config;

        if (needRebuild)
        {
            // Calculate new dimensions
            int rtWidth  = static_cast<int>(m_baseWidth * config.widthScale);
            int rtHeight = static_cast<int>(m_baseHeight * config.heightScale);
            rtWidth      = (rtWidth > 0) ? rtWidth : 1;
            rtHeight     = (rtHeight > 0) ? rtHeight : 1;

            // Recreate render target with new config
            auto builder = D12RenderTarget::Create()
                           .SetFormat(config.format)
                           .SetDimensions(rtWidth, rtHeight)
                           .SetLinearFilter(config.allowLinearFilter)
                           .SetSampleCount(config.sampleCount)
                           .EnableMipmap(config.enableMipmap)
                           .SetClearValue(config.clearValue);

            char debugName[32];
            sprintf_s(debugName, "colortex%d", index);
            builder.SetName(debugName);

            m_renderTargets[index] = builder.Build();
            m_renderTargets[index]->Upload();
            m_renderTargets[index]->RegisterBindless();

            // Re-upload indices after resource recreation
            UpdateIndices();
        }
    }

    // ============================================================================
    // [NEW] Reset and Config Query Implementation
    // ============================================================================

    void ColorTextureProvider::ResetToDefault(const std::vector<RenderTargetConfig>& defaultConfigs)
    {
        int count = static_cast<int>(std::min(static_cast<size_t>(m_activeCount), defaultConfigs.size()));

        for (int i = 0; i < count; ++i)
        {
            SetRtConfig(i, defaultConfigs[i]);
        }

        LogInfo(LogRenderTargetProvider,
                "ColorTextureProvider:: ResetToDefault - restored %d colortex to default config",
                count);
    }

    const RenderTargetConfig& ColorTextureProvider::GetConfig(int index) const
    {
        if (!IsValidIndex(index))
        {
            throw InvalidIndexException("ColorTextureProvider::GetConfig", index, m_activeCount);
        }
        return m_configs[index];
    }

    // ============================================================================
    // [NEW] Uniform Registration API - Shader RT Fetching Feature
    // ============================================================================

    void ColorTextureProvider::RegisterUniform(UniformManager* uniformMgr)
    {
        if (!uniformMgr)
        {
            LogError(LogRenderTargetProvider,
                     "ColorTextureProvider::RegisterUniform - UniformManager is nullptr");
            return;
        }

        m_uniformManager = uniformMgr;

        // Register ColorTextureIndexUniforms to slot b3 with PerFrame frequency
        m_uniformManager->RegisterBuffer<ColorTextureIndexUniforms>(
            SLOT_COLOR_TARGETS,
            UpdateFrequency::PerFrame,
            BufferSpace::Engine
        );

        LogInfo(LogRenderTargetProvider,
                "ColorTextureProvider::RegisterUniform - Registered at slot b%u",
                SLOT_COLOR_TARGETS);

        // Initial upload of indices
        UpdateIndices();
    }

    void ColorTextureProvider::UpdateIndices()
    {
        if (!m_uniformManager)
        {
            // Not registered yet, skip silently
            return;
        }

        // Collect bindless indices for all active colortex slots
        for (int i = 0; i < m_activeCount; ++i)
        {
            bool isFlipped = m_flipState.IsFlipped(i);

            // Read index: Main if not flipped, Alt if flipped
            uint32_t readIdx = isFlipped ? m_renderTargets[i]->GetAltTextureIndex() : m_renderTargets[i]->GetMainTextureIndex();

            // Write index: Alt if not flipped, Main if flipped
            uint32_t writeIdx = isFlipped ? m_renderTargets[i]->GetMainTextureIndex() : m_renderTargets[i]->GetAltTextureIndex();

            m_indexBuffer.SetReadIndex(static_cast<uint32_t>(i), readIdx);
            m_indexBuffer.SetWriteIndex(static_cast<uint32_t>(i), writeIdx);
        }

        // Upload to GPU via UniformManager
        m_uniformManager->UploadBuffer(m_indexBuffer);
    }

    // ============================================================================
    // Extended API
    // ============================================================================

    DXGI_FORMAT ColorTextureProvider::GetFormat(int index) const
    {
        if (!IsValidIndex(index))
        {
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        }
        return m_renderTargets[index]->GetFormat();
    }

    std::shared_ptr<D12RenderTarget> ColorTextureProvider::GetRenderTarget(int index) const
    {
        if (!IsValidIndex(index))
        {
            throw InvalidIndexException("ColorTextureProvider", index, m_activeCount);
        }
        return m_renderTargets[index];
    }

    bool ColorTextureProvider::IsFlipped(int index) const
    {
        if (!IsValidIndex(index))
        {
            return false;
        }
        return m_flipState.IsFlipped(index);
    }

    void ColorTextureProvider::OnResize(int newWidth, int newHeight)
    {
        if (newWidth <= 0 || newHeight <= 0)
        {
            throw std::invalid_argument("ColorTextureProvider:: New dimensions must be > 0");
        }

        m_baseWidth  = newWidth;
        m_baseHeight = newHeight;

        for (int i = 0; i < m_activeCount; ++i)
        {
            const auto& config = m_configs[i];

            int rtWidth  = static_cast<int>(newWidth * config.widthScale);
            int rtHeight = static_cast<int>(newHeight * config.heightScale);
            rtWidth      = (rtWidth > 0) ? rtWidth : 1;
            rtHeight     = (rtHeight > 0) ? rtHeight : 1;

            m_renderTargets[i]->ResizeIfNeeded(rtWidth, rtHeight);
        }

        LogInfo(LogRenderTargetProvider,
                "ColorTextureProvider:: Resized to %dx%d base", newWidth, newHeight);
    }

    void ColorTextureProvider::GenerateMipmaps(ID3D12GraphicsCommandList* cmdList)
    {
        if (!cmdList)
        {
            throw std::invalid_argument("ColorTextureProvider:: Command list cannot be null");
        }

        for (int i = 0; i < m_activeCount; ++i)
        {
            if (!m_configs[i].enableMipmap || !m_renderTargets[i])
            {
                continue;
            }

            auto mainTex = m_renderTargets[i]->GetMainTexture();
            if (mainTex)
            {
                mainTex->GenerateMips();
            }

            auto altTex = m_renderTargets[i]->GetAltTexture();
            if (altTex)
            {
                altTex->GenerateMips();
            }
        }
    }

    std::string ColorTextureProvider::GetDebugInfo(int index) const
    {
        if (!IsValidIndex(index))
        {
            std::ostringstream oss;
            oss << "ColorTextureProvider:: Invalid index " << index;
            oss << ", valid range [0, " << m_activeCount << ")";
            return oss.str();
        }

        const auto& rt      = m_renderTargets[index];
        const auto& config  = m_configs[index];
        bool        flipped = m_flipState.IsFlipped(index);

        std::ostringstream oss;
        oss << "=== colortex" << index << " ===\n";
        oss << "Status: Active (" << (index + 1) << "/" << m_activeCount << ")\n";
        oss << "FlipState: " << (flipped ? "Flipped" : "Normal") << "\n";
        oss << "MainIndex: " << rt->GetMainTextureIndex() << "\n";
        oss << "AltIndex: " << rt->GetAltTextureIndex() << "\n";
        oss << "Scale: " << config.widthScale << "x" << config.heightScale << "\n";
        oss << "Format: " << config.format << "\n";
        oss << "Mipmap: " << (config.enableMipmap ? "Yes" : "No") << "\n";

        return oss.str();
    }

    std::string ColorTextureProvider::GetAllInfo() const
    {
        std::ostringstream oss;
        oss << "=== ColorTextureProvider Overview ===\n";
        oss << "Base: " << m_baseWidth << "x" << m_baseHeight << "\n";
        oss << "Active: " << m_activeCount << "/" << MAX_COLOR_TEXTURES << "\n\n";

        oss << "Index | Name      | Resolution | Format | Flip | MainIdx | AltIdx\n";
        oss << "------|-----------|------------|--------|------|---------|-------\n";

        for (int i = 0; i < m_activeCount; ++i)
        {
            const auto& rt     = m_renderTargets[i];
            const auto& config = m_configs[i];

            int  w       = static_cast<int>(m_baseWidth * config.widthScale);
            int  h       = static_cast<int>(m_baseHeight * config.heightScale);
            bool flipped = m_flipState.IsFlipped(i);

            char line[256];
            sprintf_s(line, "%-5d | colortex%-1d | %4dx%-4d | %-6d | %-4s | %-7u | %u\n",
                      i, i, w, h, config.format,
                      flipped ? "Yes" : "No",
                      rt->GetMainTextureIndex(),
                      rt->GetAltTextureIndex());
            oss << line;
        }

        return oss.str();
    }

    // ============================================================================
    // Private Methods
    // ============================================================================

    void ColorTextureProvider::ValidateIndex(int index) const
    {
        if (!IsValidIndex(index))
        {
            throw InvalidIndexException("ColorTextureProvider", index, m_activeCount);
        }
    }

    bool ColorTextureProvider::IsValidIndex(int index) const
    {
        return index >= 0 && index < m_activeCount;
    }
} // namespace enigma::graphic
