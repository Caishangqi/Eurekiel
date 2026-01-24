#include "SamplerProvider.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

// ============================================================================
// SamplerProvider.cpp - [NEW] Sampler provider implementation
// Part of Dynamic Sampler System
// ============================================================================

namespace enigma::graphic
{
    // ============================================================================
    // Constructor
    // ============================================================================

    SamplerProvider::SamplerProvider(
        GlobalDescriptorHeapManager&      heapManager,
        const std::vector<SamplerConfig>& configs,
        UniformManager*                   uniformMgr)
        : m_heapManager(&heapManager)
    {
        // Validate parameters
        if (configs.empty())
        {
            throw std::invalid_argument("SamplerProvider:: Config vector cannot be empty");
        }

        if (!uniformMgr)
        {
            throw std::invalid_argument("SamplerProvider:: UniformManager cannot be null (RAII requirement)");
        }

        // Clamp config count to valid range
        uint32_t configCount = static_cast<uint32_t>(configs.size());
        if (configCount > MAX_SAMPLERS)
        {
            LogWarn(LogSamplerProvider,
                    "SamplerProvider:: Config count %u exceeds max %u, clamping",
                    configCount, MAX_SAMPLERS);
            configCount = MAX_SAMPLERS;
        }

        m_activeCount = configCount;
        m_configs.reserve(m_activeCount);
        m_samplers.reserve(m_activeCount);

        // Create samplers from configs
        for (uint32_t i = 0; i < m_activeCount; ++i)
        {
            const auto& config = configs[i];
            m_configs.push_back(config);

            // Create RAII sampler (allocates from heap, creates D3D12 descriptor)
            m_samplers.push_back(std::make_unique<Sampler>(heapManager, config));
        }

        LogInfo(LogSamplerProvider,
                "SamplerProvider:: Initialized with %u/%u samplers",
                m_activeCount, MAX_SAMPLERS);

        // [RAII] Register uniform buffer and perform initial upload
        RegisterUniform(uniformMgr);
    }

    // ============================================================================
    // Sampler Access API
    // ============================================================================

    void SamplerProvider::SetSamplerConfig(uint32_t index, const SamplerConfig& config)
    {
        ValidateIndex(index);

        m_configs[index] = config;
        m_samplers[index]->UpdateConfig(config);

        // Update index buffer (bindless index may have changed)
        UpdateIndices();

        LogDebug(LogSamplerProvider,
                 "SamplerProvider:: Updated sampler%u config", index);
    }

    const SamplerConfig& SamplerProvider::GetSamplerConfig(uint32_t index) const
    {
        ValidateIndex(index);
        return m_configs[index];
    }

    uint32_t SamplerProvider::GetBindlessIndex(uint32_t index) const
    {
        if (!IsValidIndex(index))
        {
            return INVALID_SAMPLER_INDEX;
        }
        return m_samplers[index]->GetBindlessIndex();
    }

    // ============================================================================
    // Uniform Registration API
    // ============================================================================

    void SamplerProvider::RegisterUniform(UniformManager* uniformMgr)
    {
        if (!uniformMgr)
        {
            LogError(LogSamplerProvider,
                     "SamplerProvider::RegisterUniform - UniformManager is nullptr");
            return;
        }

        m_uniformManager = uniformMgr;

        // Register SamplerIndicesUniforms to slot b8 with PerFrame frequency
        // [FIX] Use slot 8 to avoid conflict with MatricesUniforms (b7)
        m_uniformManager->RegisterBuffer<SamplerIndicesUniforms>(
            SLOT_SAMPLER_INDICES,
            UpdateFrequency::PerFrame,
            BufferSpace::Engine
        );

        LogInfo(LogSamplerProvider,
                "SamplerProvider::RegisterUniform - Registered at slot b%u",
                SLOT_SAMPLER_INDICES);

        // Initial upload of indices
        UpdateIndices();
    }

    void SamplerProvider::UpdateIndices()
    {
        if (!m_uniformManager)
        {
            // Not registered yet, skip silently
            return;
        }

        // Collect bindless indices for all active sampler slots
        m_indexBuffer.Reset();

        for (uint32_t i = 0; i < m_activeCount; ++i)
        {
            uint32_t bindlessIdx = m_samplers[i]->GetBindlessIndex();
            m_indexBuffer.SetIndex(i, bindlessIdx);
        }

        // Upload to GPU via UniformManager
        m_uniformManager->UploadBuffer(m_indexBuffer);

        LogDebug(LogSamplerProvider,
                 "SamplerProvider::UpdateIndices - Uploaded %u sampler indices",
                 m_activeCount);
    }

    // ============================================================================
    // Private Methods
    // ============================================================================

    void SamplerProvider::ValidateIndex(uint32_t index) const
    {
        if (!IsValidIndex(index))
        {
            throw InvalidSamplerIndexException("SamplerProvider", index, m_activeCount);
        }
    }

    bool SamplerProvider::IsValidIndex(uint32_t index) const
    {
        return index < m_activeCount;
    }
} // namespace enigma::graphic
